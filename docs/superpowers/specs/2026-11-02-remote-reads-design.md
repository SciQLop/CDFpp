# Remote (S3) reads — design

Date: 2026-11-02. Status: draft, not approved.

## Intent

Let CDFpp open a CDF file that lives on S3, or any other remote store, without downloading it.
Users pay per byte and per request on S3, so CDFpp must fetch little beyond what it needs, and
fetch each byte at most once per open file. How much extra to fetch to save requests is a knob.

Users are mostly Python users (pycdfpp, speasy). The first transport is written in Python and is
meant for experimenting. It stays long-term only if measurements say it is fast enough.
Otherwise the transport moves to C++ (the user's call).

## Goals

- Open a remote CDF lazily: only metadata is fetched when the file is opened.
- Load a variable by fetching only its index records and data blocks.
- Never fetch a byte twice for one open file.
- Send as few requests as possible. On HelioCloud, from Europe, one GET costs about 0.3 s
  whatever its size up to 64 KB, so request count drives latency.
- Measure the Python transport against a native one, to decide whether it can stay.

## Non-goals

- Partial record reads (records N to M, or a time range). Planned later, as a separate change,
  not mixed with this one (the user's call).
- A cache that survives the process. The cache lives and dies with the open file (the user's
  call).
- Writing to remote stores.
- A C++ S3 client in this change. It comes only if the benchmark says so.
- The WASM build (wacdfpp). Browsers only fetch asynchronously while the parser reads
  synchronously; bridging that (Asyncify or a worker) is its own project. Later. The feature is
  compiled out of that target.

## Anti-goals

- Silent wrong data. Reading bytes that were never fetched, or whose fetch failed, must fail
  loudly (an exception, or a crash as last resort), never return zeros.
- Corrupt input killing the process. A bad offset in a file must raise a catchable error.
- A network dependency (libcurl, TLS, AWS SDK) in cpp_utils.
- Slowing down local mmap reads.

## What we measured (2026-11-02)

Bucket: `gov-nasa-hdrl-data1` (HelioCloud, public, anonymous reads work).
Sample: `spdf/cdaweb/data/mms/mms1/fgm/srvy/l2/2020/01/mms1_fgm_srvy_l2_20200101_v5.224.0.cdf`,
53.6 MB, 22 variables, per-variable compression.

- Range GET timing: 1 KB and 64 KB both about 0.38 s; 1 MB 0.69 s; 8 MB 1.27 s (6.6 MB/s raw
  on one connection; latency-corrected about 6–9 MB/s, re-measure in the benchmark).
- Metadata is 84 KB in total. The first 37 KB holds the CDR, GDR, all ADR/AgrEDR and most VDRs.
  A few VDRs and AzEDRs sit at the very end of the file.
- The 232 VXRs are spread through the file, between data blocks. They form linked lists, so
  following one costs one round trip per link.

### Benchmark of the implementation (2026-11-02, `benchmarks/remote/heliocloud.py`)

Same sample file, from the user's machine (one cold round trip 0.54 s, full download 4.2 s).
Default knobs (256 KiB read-ahead and tail) unless stated.

| workload | fsspec | obstore | batches | MB read | full download + load |
|---|---|---|---|---|---|
| open | 1.26 s | 1.17 s | 3 | 0.95 | 4.15 s, 53.6 MB |
| 1 variable | 0.97 s | 0.95 s | 4 | 7.24 | 4.16 s |
| 3 variables | 8.81 s | 10.00 s | 72 | 30.05 | 4.17 s |
| all variables | 16.93 s | 19.81 s | 142 | 53.64 | 4.18 s |

With `read_ahead` = 1 MiB (fsspec): open 0.66 s / 2.26 MB, 1 variable 1.15 s / 8.55 MB,
3 variables 5.56 s / 17 batches, all variables 10.51 s / 30 batches. With 64 KiB: all
variables 26.81 s / 187 batches.

What it says:
- The Python transports (fsspec, obstore) are within noise of each other; latency comes
  from sequential batches (VXR chains walked link by link), not from Python. The Python
  transport can stay; a C++ one would not change the batch count.
- Remote reads win clearly for opening and for a few variables. Reading most of a file
  costs more than downloading it, because of the chains.
- Decided (user, 2026-11-02): CDFpp's default `read_ahead` is 1 MiB (fewer batches, about
  1-2 MB more on small reads). cpp_utils keeps 256 KiB as its generic default. Batching
  further (several VXR links per window) is left for later.

## History that matters

CDFpp once had a stream reader (`stream_adapter`, removed in e4f2fc1, August 2023). The parser's
dependence on one pointer to the whole file is older than the move to `cpp_utils::serde`
(a4c2df8, July 2026); that move kept it. This design does not need to undo it: the mapped region
below gives the parser that pointer.

## The design

### 1. The reader interface (transport)

A transport answers one question: "give me these byte ranges". It is batched so one call can
fetch many ranges concurrently. Batching saves GIL crossings and lets requests overlap; it does
not reduce the number of requests (fsspec `cat_ranges` / obstore `get_ranges` send one GET per
range). The read-ahead window (section 2) is what reduces requests.

```
size() -> total file size in bytes (costs one HEAD; counted in budgets)
read_ranges([(offset, size), ...], destinations) -> fills each destination
```

Contract:
- Results are matched to requests by index.
- Each result is exactly the requested length; anything else raises.
- A batch either fills every range or raises. The cache never installs a partial batch.
- Retries are the transport's job, not the cache's.

C++ side: a small interface in cpp_utils (template concept or virtual class, builder's choice).
Python side: pycdfpp accepts any object with `size()` and `read_ranges(ranges) -> list of
bytes`. A buffer-filling variant (`read_ranges_into`) was built, benchmarked once, then removed
(2026-11-02): it was within noise of `bytes` in the first benchmark run (wall time is set by
sequential batches), and it let a reader under-fill buffers silently or keep them past the
call. The copy cost is therefore not measured separately any more.

### 2. The page cache over a reserved region (cpp_utils)

**Region and presence.**
- Reserve one region the size of the remote file. Default backing: anonymous memory. Opt-in
  backing: a deleted temp file on a real disk (region-backing knob).
- Track presence and protection per OS page. The page size is queried at runtime (4 KB on
  Linux/Windows x86, 16 KB on Apple Silicon). Unfetched pages have no access rights.
- The fetch unit is separate: the **read-ahead window** knob. `ensure(ranges)` widens each range
  outward to window-aligned bounds, drops pages already present, merges what is left into
  contiguous runs, and fetches all runs in one `read_ranges` batch. A chain of small records
  (VXRs, VDRs) lying within one window then costs one request instead of one per link.
- After `ensure`, the bytes are at `base + offset`, contiguous, read in place by the parser and
  the decompressors.

**Install protocol** (so a failed fetch can never read as zeros):
1. Validate every range against `size()`; an out-of-file offset throws a `std::runtime_error`
   naming the offset (corrupt file, not a crash).
2. Fetch. Validate lengths (contract above).
3. Write the bytes into the region, then give those pages read access.
4. Only then mark the pages present (release semantics).
5. On any failure, pages stay no-access and the exception propagates.

**Guards.**
- Record loads and `read` call `ensure`, so on those paths bytes are always present.
- Where fetching is forbidden (CDFpp worker threads), the same calls check the presence bitmap
  instead and throw ("remote bytes not fetched at offset X"). That catches a short prefetch as
  a catchable error.
- Any path that bypasses both hits a no-access page and crashes, on purpose. Tests cover every
  loading path, so such a path shows up in CI.
- A failed fetch puts its pages back to no-access, so partial bytes are never readable.

**Platform primitives** (whichever backing is chosen):
- POSIX: `mmap` with `PROT_NONE`, then `mprotect(PROT_READ)` per installed run. With a file
  backing: `MAP_SHARED`, write with `pwrite` before `mprotect`.
- Windows, memory backing: `VirtualAlloc(MEM_RESERVE, PAGE_NOACCESS)`, then `MEM_COMMIT` per
  run. Windows, file backing: `CreateFile(GENERIC_READ|GENERIC_WRITE`, share flags including
  `FILE_SHARE_DELETE`, `FILE_FLAG_DELETE_ON_CLOSE)`, mark sparse with `FSCTL_SET_SPARSE` (or
  the file is fully allocated), `CreateFileMapping(PAGE_READWRITE)`, one whole-file
  `MapViewOfFile`, `VirtualProtect` per run. The existing read-only `memory_mapped_file` cannot
  be reused as is.
- One whole-file view means no 64 KB allocation-granularity constraint on Windows.

**Thread safety and the GIL.** This is a correctness rule, not only a performance one.
- Today `Variable::type()` and friends take the variable's load mutex while pycdfpp holds the
  GIL (`pycdfpp/collections.hpp` `make_values_view`), and `load_values()` holds that mutex during
  the load. A Python transport needs the GIL inside the load. Thread A (GIL, waits for mutex) and
  thread B (mutex, waits for GIL) deadlock.
- Rule: never wait on a CDFpp lock while holding the GIL. pycdfpp releases the GIL before every
  call that can take a variable's load mutex.
- Rule: never fetch while holding the cache lock. Find missing runs under the lock, mark them
  in flight, drop the lock, fetch, re-take it to install. Threads wanting an in-flight page wait
  for it.
- Rule: fetches never happen on CDFpp's own worker threads (`parallel::for_each_index` spawns
  threads with no Python state). The prefetch in section 3 is therefore mandatory; a worker
  finding a missing page throws.

**Cost on local buffers.** `ensure` and the bitmap check are compile-time dispatched
(`if constexpr (requires { b.ensure(...); })`), so mmap and in-memory loads are unchanged.
`shared_buffer_t::read` is `const`; the remote buffer keeps its cache state `mutable` behind
the lock.

**Lifetime.** The region lives as long as something references the buffer. Today, after an
eager load, the block counter and stored-records lister run immediately and drop their
references, so the buffer dies at the end of `load`. In lazy mode it lives until the last
variable is loaded (or the CDF is dropped). That is unchanged by this design.

### Knobs

Each knob has a documented default and can be overridden per call (keyword) or globally (an
environment variable, so a deployment such as HelioCloud's JupyterHub can preset values for its
data). Which one wins between fewer requests and fewer bytes is the user's decision, through these.

| Knob | Default | Meaning |
|---|---|---|
| read-ahead window | 1 MiB in CDFpp (256 KiB in cpp_utils) | each fetch is widened to this aligned size; rounded up to the page size |
| tail prefetch | 256 KB | fetched with the first window at open: late-written VDRs/AEDRs sit at the end |
| region backing | memory | `memory`, or `file` (deleted temp file in a given directory on a real disk) |
| download whole below | off | opt-in: if the file is smaller than N bytes, fetch it whole at open |

Exact names of keywords and environment variables are left to the builder. The 1 MiB default comes from the HelioCloud benchmark below ("Benchmark of the
implementation"); it started at 256 KB, the assistant's call from the first timing curve.

### 3. Where CDFpp calls `ensure`

1. **Opening.** One batch: the first window plus the tail prefetch, plus the HEAD for `size()`.
   On the sample file that should cover the CDR, GDR, all ADRs and AEDRs and all VDRs. Attribute
   values are read eagerly at open (`attribute.hpp`), so files with large attribute values cost
   more; the benchmark reports open cost in requests and bytes.
2. **Every record load.** Records go through `load_record` / `load_mut_record`
   (`records-loading.hpp`). Each becomes: ensure the header (12 bytes in v3, 8 in v2), read the
   record size, ensure the whole record, deserialise. When the record type is itself a header
   (`is_cdf_DR_header_v`), only the header is ensured. The debug readers
   (`debug/record_stream.hpp`, `debug/nasa_compat_repr.hpp`) call `serde::deserialize` directly
   and must be routed through the same path, or be declared local-only.
3. **Direct reads.** `buffer.read(...)` (magic in `loading.hpp`, attribute values in
   `attribute.hpp`, `common.hpp`, VVR payloads in `variable.hpp`) ensure inside `read`. The parser
   itself does not use `read`; it reads through the span, which is why item 2 is needed.
4. **Block discovery.** `list_blocks` recurses into sub-VXRs and follows `VXRnext` inside its
   per-entry loop. It is restructured to: ensure all of one VXR's child headers in one batch,
   then visit them. It also keeps each child's record size in `values_block` (today the header is
   read and discarded), so step 5 needs no second header trip. The VXR chain stays one round trip
   per link, except where the read-ahead window covers several links.
5. **Data prefetch.** `load_blocks` ensures all block extents of the variable in one batch, before
   any decoding, on the calling thread. Then decoding runs as today (threads only for compressed
   data of 1 MB or more).

Whole-file-compressed CDFs (magic `cdf30001 cccc0001`): the CCR record is the whole file, so
ensuring it fetches everything; then it loads as today.

### 4. pycdfpp

`pycdfpp.load(reader_object, ...)` with the knobs as keywords (names left to the builder). Lazy
loading is the default for remote readers.

The transport wrapper owns a strong reference to the Python object and only touches it with the
GIL held, including when it is destroyed (the buffer is copied into every deferred loader, so it
can die on any thread). The CDF keeps the reader alive (`keep_alive`, as the in-memory lazy path
already does). No callback runs after interpreter finalisation: a fetch attempted then throws.

## How we check it works

- **Test seam.** A `load(reader)` overload in C++, plus a test fixture that wraps a local path in
  a counting reader. The existing loading tests run a second time through that fixture, so every
  parser path is exercised with `ensure` and the presence checks active.
- **Exact counts without a network.** Tests assert, from the counting reader: no byte fetched
  twice; opening costs one batch (plus the size query); loading one variable fetches only that
  variable's records plus window padding.
- **Failure paths.** A reader that returns short data, and one that raises: loads must raise,
  never return zeros. A corrupt offset raises, not crashes.
- **GIL deadlock.** From Python, load a variable on one thread while another reads its `.values`
  and `.type` through a Python reader.
- **Benchmark (outside CI).** On HelioCloud, for opening a file and loading one, a few, and all
  variables, record wall time, request count and bytes fetched. Compare:
  1. the Python reader with fsspec and with obstore (returning `bytes`; see section 1 for the
     dropped buffer-filling variant),
  2. a native reader (a minimal C++ HTTP range client, or a C++ reader over a local file plus the
     measured round-trip time, builder's choice),
  3. a full download plus a local load, as the baseline users have today.
  This benchmark decides whether the Python transport stays, and confirms the window default.

## Decided

- Explicit `ensure` calls, not page-fault-driven fetching (`userfaultfd`, signal handlers): a fault
  knows one page, not the whole range, so it would send many more requests. It also deadlocks with
  a Python reader holding the GIL and can't report network errors. (User agreed.)
- Page protection kept as a backstop guard, everywhere. (User proposed for CI; assistant's call to
  keep it in production too.) Presence checks before parsing turn missed `ensure` calls into
  catchable errors first. (From review.)
- Presence tracked per OS page; fetch size set by a read-ahead window. This replaces the earlier
  "block size + merge gap" pair, which could not shorten chains of small records. (From review;
  assistant's call.)
- Per-process cache only. (User.)
- Memory-backed region by default; temp-file backing is an opt-in knob. (User, after review.)
- "Download whole if smaller than N" is opt-in, never the default: many small files each read
  a little would cost far more if downloaded whole. (User.)
- Knobs with documented defaults, overridable per call and by environment variable. (User.)
- WASM later. (User: "if easy"; it is not.)
- Partial record reads later and separately. (User.)
- The generic reader interface and page cache go in cpp_utils, so other parsers (e.g.
  spaceouija) can use them. The S3 transport does not go in cpp_utils. (User and assistant.)
- Batched `read_ranges` interface with an exact-length, all-or-throw contract. (Assistant's call.)
- Lock rules against the GIL deadlock (section 2). (From review, verified in the code.)
- Whole-file-compressed CDFs are fetched in full. (Assistant's call.)
- pycdfpp accepts a reader object only. The fsspec and obstore adapters live in the benchmark
  script for now, so pycdfpp gains no new dependency. (Assistant's call.)
- cpp_utils is its own repo: its PR lands first, then CDFpp bumps the wrap revision.
  (Assistant's call.)

## Left to the builder

- Exact C++ shape of the reader interface (concept vs virtual class) and of the pycdfpp keywords.
- Whether the Python reader returns `bytes` or fills buffers passed from C++.
- How `ensure` is plumbed through `shared_buffer_t` and `parsing_context_t`.
- The native reader used in the benchmark.
- Test file choice for the counting-reader tests (existing `tests/resources` files are fine).
