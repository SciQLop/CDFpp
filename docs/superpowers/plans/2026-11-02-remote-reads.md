# Remote (S3) Reads Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking. Reviews are done by an OpenCode agent through Herdr (`herdr-handoff` skill, OpenCode started with `--auto`), per the user.

**Goal:** Open a remote CDF lazily through a batched byte-range reader, fetching each byte at most once per open file, with a Python-reader path for experimenting.

**Architecture:** cpp_utils gets a range-reader concept, a page-protected reserved region, and a `remote_buffer` page cache that widens reads to a read-ahead window and fetches in batches. CDFpp wraps it like its mmap buffer, calls `ensure` at its record-loading choke points, and prefetches data blocks in one batch before decoding. pycdfpp wraps a Python object as a reader, with lock rules that rule out the GIL deadlock.

**Tech Stack:** C++20 header-only (cpp_utils, CDFpp), Meson, Catch2 v3, pybind11, Python unittest.

**Spec:** `docs/superpowers/specs/2026-11-02-remote-reads-design.md`

## Global Constraints

- cpp_utils gains no network dependency. The S3 transport is not in cpp_utils.
- Local mmap / in-memory loads are unchanged: every new hook is compile-time dispatched (`if constexpr (requires { ... })`).
- Never return zeros for unfetched or failed bytes. Failed fetches raise, and their pages go back to no-access.
- A corrupt offset raises `std::runtime_error` naming the offset; it never crashes. Bounds checks are written without addition (`offset > size || len > size - offset`), as in `include/cdfpp/cdf-io/debug/record_stream.hpp:202-207`.
- Never wait on a CDFpp lock while holding the GIL. Never fetch while holding the cache lock. Never fetch on CDFpp worker threads.
- Knob defaults: read-ahead 256 KiB, tail prefetch 256 KiB, backing `memory`, temp dir = platform user cache dir (used only with `file` backing), download-whole-below 0 (off). The whole-download rule is `size() < download_whole_below`. Every knob can be overridden per call and by environment variable; per-call values win.
- Environment variables: `CDFPP_REMOTE_READ_AHEAD`, `CDFPP_REMOTE_TAIL_PREFETCH`, `CDFPP_REMOTE_BACKING` (`memory`|`file`), `CDFPP_REMOTE_TEMP_DIR`, `CDFPP_REMOTE_DOWNLOAD_WHOLE_BELOW` (bytes).
- `iso_8859_1_to_utf8` defaults to `true` on every new load surface, matching `pycdfpp.load`.
- WASM: the feature is compiled out (`#ifndef __EMSCRIPTEN__` around the region and the new `load` overload).
- Every test resource is smaller than 256 KiB. Tests that observe batching set small knobs explicitly (`read_ahead = page_size()`, `tail_prefetch = 0` unless the test is about the tail).
- Portable tests: cpp_utils CI runs Linux, macOS and Windows. Use a `set_env` test helper (`setenv` / `_putenv_s`). Guard POSIX-only tests (`fork`, `/proc`) with `#ifndef _WIN32`.
- Builds: one `ninja` / `meson test` at a time, in the foreground, waited out, never backgrounded, never two against the same build dir. Subagents are told this explicitly. Part A runs in `~/Documents/prog/cpp_utils` (its own `build`); Part B runs in `~/Documents/prog/CDFpp`.
- CDFpp Python tests need the uv venv (numpy pyyaml ddt cyclopts rich) first on PATH for every `ninja` / `meson` call. Run Python from outside the repo root with `PYTHONPATH=build`.
- Style: `.clang-format` in each repo; small functions; no comment-decorated blocks.
- Never push without the user's consent.

## Review Focus

1. **A file truncated inside a record, or a pointer near `SIZE_MAX`** → `load` raises a `std::runtime_error` (Python: `RuntimeError`) naming the offset, no crash. Tests in Task 3 (bounds, overflow) and Task 4 (truncated file).
2. **The reader throws mid-batch** → the load raises; that batch's pages return to no-access; a later call with a healthy reader succeeds. Test in Task 3. A reader returning short data is caught by the Python wrapper's length check (Task 7); the C++ cache trusts the all-or-throw contract.
3. **Two Python threads touch the same lazy variable while it loads through a Python reader** (one reads `.values`, one reads `.type`) → both finish, with no deadlock and no byte fetched twice. Test in Task 7.
4. **A remote size that isn't a page multiple, an empty file, and a file smaller than the window or the tail** → the last partial page works; the tail start clamps to 0; an empty file gives `std::nullopt`. Tests in Task 2, Task 3 and Task 4.
5. **A lazy CDF from a Python reader still alive at interpreter exit** (module global) → the process exits with code 0, and nothing touches Python after finalisation. Test in Task 7.

---

## Part A — cpp_utils (repo `~/Documents/prog/cpp_utils`)

Setup, once: `git -C ~/Documents/prog/cpp_utils pull --ff-only` (local main is 3 behind origin, which has `d06e125`, the revision CDFpp pins), then `git switch -c feat/remote-buffer`.

### Task 1: Range-reader concept and a local counting reader

**Files:**
- Create: `include/cpp_utils/io/range_reader.hpp`
- Create: `include/cpp_utils/io/local_range_reader.hpp`
- Test: `tests/io/test_local_range_reader.cpp` (register `TestLocalRangeReader` in `tests/meson.build`, same shape as `TestMemoryMappedFile`)
- Doc: `docs/api/io.md` (new section), per cpp_utils CLAUDE.md

**Interfaces:**
- Produces:
  - `struct byte_range { std::size_t offset; std::size_t size; };`
  - `concept range_reader<R>`: `std::copy_constructible<R>`; `r.size() -> std::size_t`; `r.read_ranges(std::span<const byte_range>, std::span<char* const> destinations) -> void`. Contract: fills every destination with exactly its range's bytes, or throws.
  - `class local_range_reader`: `explicit local_range_reader(const std::string& path)`. It satisfies `range_reader` and is cheap to copy: the mapped file and the call log sit behind `std::shared_ptr`, so every copy shares one log. Methods: `const std::vector<std::vector<byte_range>> batches() const` (a snapshot), `std::size_t bytes_served() const`, `std::vector<std::thread::id> caller_threads() const`. A range past the end throws `std::out_of_range`. The log is guarded by a mutex inside the shared state.

- [ ] **Step 1: Write the failing tests.** `static_assert(range_reader<local_range_reader>)`. Reading `{0,4}` and `{8,4}` of a temp file written by the test returns those bytes. `batches()` holds one entry with 2 ranges, and `bytes_served() == 8`. A copy of the reader sees the same log. A range past the end throws `std::out_of_range`.
- [ ] **Step 2: Run** `cd ~/Documents/prog/cpp_utils && ninja -C build && meson test -C build Test-TestLocalRangeReader`. Expected: compile failure (header missing).
- [ ] **Step 3: Implement** both headers.
- [ ] **Step 4: Run** the same command. Expected: PASS.
- [ ] **Step 5: Commit** `feat(io): range_reader concept and local_range_reader`.

### Task 2: Page-protected reserved region

**Files:**
- Create: `include/cpp_utils/io/reserved_region.hpp`
- Test: `tests/io/test_reserved_region.cpp` (register `TestReservedRegion`)

**Interfaces:**
- Produces:
  - `std::size_t page_size()` (runtime query: `sysconf(_SC_PAGESIZE)` / `GetSystemInfo().dwPageSize`).
  - `enum class region_backing { memory, file };`
  - `class reserved_region` (move-only). Constructor: `reserved_region(std::size_t size, region_backing backing = region_backing::memory, const std::filesystem::path& temp_dir = {})`. Size 0 is valid and maps nothing (`data() == nullptr`).
  - `char* data() const`; `std::size_t size() const`.
  - `char* make_writable(std::size_t first_page, std::size_t page_count)`; `void seal(std::size_t first_page, std::size_t page_count)` (read-only); `void make_no_access(std::size_t first_page, std::size_t page_count)`.
  - Every `mprotect` / `VirtualProtect` / `VirtualAlloc` result is checked; a failure throws `std::system_error`.
- Memory backing: POSIX `mmap(PROT_NONE, MAP_PRIVATE|MAP_ANONYMOUS|MAP_NORESERVE)`. Windows: `VirtualAlloc(MEM_RESERVE, PAGE_NOACCESS)`; `make_writable` = `MEM_COMMIT` + `PAGE_READWRITE`; `seal` = `VirtualProtect(PAGE_READONLY)`; `make_no_access` = `VirtualProtect(PAGE_NOACCESS)`.
- File backing:
  - POSIX: `mkstemp` in `temp_dir`, `unlink` at once, `ftruncate(size)`, `mmap(PROT_NONE, MAP_SHARED)`.
  - Windows: the `CreateFile` / `FSCTL_SET_SPARSE` / `CreateFileMapping(PAGE_READWRITE)` / one whole-file `MapViewOfFile` / `VirtualProtect` sequence from spec section 2.
  - An empty `temp_dir` means the platform user cache dir: `$XDG_CACHE_HOME` or `~/.cache` on Linux, `~/Library/Caches` on macOS, `FOLDERID_LocalAppData` on Windows. It is created if missing. If it can't be created or written, the constructor throws.

- [ ] **Step 1: Write the failing tests.** Loop over both backings in a plain `for` (not `GENERATE`; `region_backing` has no Catch2 stringifier). The cases:
  - `make_writable(1, 1)`, write, `seal(1, 1)`: `data()[page_size()]` reads back the bytes.
  - A size of `3 * page_size() + 7` can seal its last partial page.
  - Size 0 doesn't throw, and `data() == nullptr`.
  - POSIX only (`fork` + `waitpid`): in the child, read page 0 after sealing only page 1; the parent asserts `WIFSIGNALED` and `WTERMSIG == SIGSEGV` (or `SIGBUS` on macOS). Do the same after `make_no_access` on a sealed page.
  - POSIX only: `file` backing with `temp_dir = "/proc"` throws.
- [ ] **Step 2: Run** `cd ~/Documents/prog/cpp_utils && ninja -C build && meson test -C build Test-TestReservedRegion`. Expected: compile failure.
- [ ] **Step 3: Implement.** No `#ifdef` soup inside functions: one small `details::` function per OS primitive.
- [ ] **Step 4: Run** the same command. Expected: PASS.
- [ ] **Step 5: Commit** `feat(io): reserved_region with page protection and memory/file backing`.

### Task 3: `remote_buffer` page cache

**Files:**
- Create: `include/cpp_utils/io/remote_buffer.hpp`
- Test: `tests/io/test_remote_buffer.cpp` (register `TestRemoteBuffer`; uses `std::thread`, no `-Dthreading` needed)

**Interfaces:**
- Consumes: `byte_range`, `range_reader`, `local_range_reader` (Task 1); `reserved_region`, `region_backing`, `page_size()` (Task 2).
- Produces:
  - `struct remote_buffer_options`. Fields: `std::size_t read_ahead = 256 * 1024`, `std::size_t tail_prefetch = 256 * 1024`, `region_backing backing = region_backing::memory`, `std::filesystem::path temp_dir {}`, `std::size_t download_whole_below = 0`. Plus `static remote_buffer_options from_env(std::string_view prefix, remote_buffer_options defaults = {})`.
    - `from_env("CDFPP_REMOTE_")` reads `READ_AHEAD`, `TAIL_PREFETCH`, `BACKING`, `TEMP_DIR` and `DOWNLOAD_WHOLE_BELOW`.
    - A bad value throws `std::invalid_argument` naming the variable.
    - `read_ahead` is rounded up to a page multiple, with a minimum of one page.
  - `template <range_reader R> class remote_buffer`:
    - `remote_buffer(R reader, remote_buffer_options = {})`. If `reader.size() == 0`, the buffer is invalid and holds no region.
    - Const members (the cache state is `mutable` behind the lock): `void ensure(std::span<const byte_range>) const`, `void ensure(std::size_t offset, std::size_t size) const`, `void check_present(std::size_t offset, std::size_t size) const`, `void read(char* dest, std::size_t offset, std::size_t size) const`, `const char* data() const`, `begin()`, `end()`, `std::size_t size() const`, `bool is_valid() const`, `const R& reader() const`.
    - `check_present` throws `std::runtime_error("remote bytes not fetched at offset N")` if any page is missing.
    - `read` ensures, then copies.
- Algorithm for `ensure`, in this order:
  1. Bounds: any range with `offset > size() || len > size() - offset` throws `std::runtime_error` naming the offset.
  2. Under the lock:
     - Widen each range outward to `read_ahead`-aligned bounds, clamped to the file.
     - On the very first `ensure`, also add the tail, `[size() > tail_prefetch ? size() - tail_prefetch : 0, size())`. Or add the whole file if `size() < download_whole_below`.
     - Collect the pages that are neither present nor in flight. Mark them in flight, owned by this call, and merge them into contiguous runs.
     - Note the pages this call needs that another call has in flight.
  3. Release the lock. Call `make_writable` on the runs, then one `reader.read_ranges` with every run, writing straight into the region.
  4. `seal` the runs. Under the lock, mark them present, clear in-flight, and `notify_all`.
  5. On an exception: call `make_no_access` on this call's runs. Under the lock, clear in-flight for them and `notify_all`. Rethrow.
  6. Wait on the condition variable for the pages other calls had in flight. If any of them ended up absent, go back to step 2.

- [ ] **Step 1: Write the failing tests.** They run over `local_range_reader` on a temp file of `5 * 256 KiB + 123` bytes with a known pattern. Use default options unless a case says otherwise.
  - `ensure(10, 4)`: `data()[10..13]` matches. `batches().size() == 1`, and that batch is the window `[0, 256 KiB)` plus the tail `[size()-256 KiB, size())`.
  - A second `ensure(20, 4)` sends no batch.
  - With `tail_prefetch = 0`, one `ensure` of two far-apart ranges sends one batch with two runs.
  - `check_present(3 * 256 KiB, 1)` throws before an `ensure` covers it, and passes after.
  - These throw `std::runtime_error`: `ensure(size(), 1)`, `ensure(size() - 1, 10)`, `ensure(SIZE_MAX - 10, 20)`.
  - With a file of 1000 bytes: the first batch is `[0, 1000)`. That's the tail clamp: no underflow, and no range past the end.
  - Every byte is served at most once over a random sequence of 200 `ensure` calls (`bytes_served() <= size()`).
  - A reader that throws on its second call: the second `ensure` throws. `check_present` on those pages still throws, and a POSIX fork-child read of one of them faults. A later `ensure` with the reader healed succeeds.
  - 8 threads `ensure` the same range at once: one batch in total, and every thread sees the bytes.
  - `download_whole_below = size() + 1`: the first `ensure(0, 1)` serves `size()` bytes. `download_whole_below = size()`: it does not (the boundary).
  - A zero-size file: `is_valid()` is false.
  - `from_env`: with `set_env("X_READ_AHEAD", "65536")`, `read_ahead == 65536`; `"abc"` throws `std::invalid_argument`.
- [ ] **Step 2: Run** `cd ~/Documents/prog/cpp_utils && ninja -C build && meson test -C build Test-TestRemoteBuffer`. Expected: compile failure.
- [ ] **Step 3: Implement.** Keep the page bitmap and the in-flight set in a small private struct. Write one function per algorithm step.
- [ ] **Step 4: Run** the same command, then the whole suite with `meson test -C build`. Expected: all PASS, exit code 0.
- [ ] **Step 5: Document** `remote_buffer`, `reserved_region` and `range_reader` in `docs/api/io.md` with a verified example. Commit `feat(io): remote_buffer page cache with read-ahead and batched fetches`.
- [ ] **Step 6: OpenCode review of Part A** (the whole `feat/remote-buffer` diff against `origin/main`). Check each finding against the code, fix those that hold, and flag false positives. Do not push.

---

## Part B — CDFpp (repo `~/Documents/prog/CDFpp`, branch `feat/remote-reads`)

Setup, once: point the vendored checkout at the local branch:
`git -C subprojects/cpp_utils fetch ~/Documents/prog/cpp_utils feat/remote-buffer && git -C subprojects/cpp_utils checkout FETCH_HEAD`.
The wrap revision is bumped in Task 9, after cpp_utils is pushed and merged.

### Task 4: `load(reader)` and `ensure` at every record choke point

**Files:**
- Modify: `include/cdfpp/cdf-io/loading/buffers.hpp`. Add `make_shared_remote_adapter`. `shared_buffer_t` forwards `ensure` and `check_present` when the inner buffer has them.
- Modify: `include/cdfpp/cdf-io/loading/records-loading.hpp:85-110` (the three `load_record` overloads).
- Modify: `include/cdfpp/cdf-io/loading/loading.hpp` (the new `load` overload).
- Modify: `include/cdfpp/cdf-io/debug/record_stream.hpp:254,340` and `include/cdfpp/cdf-io/debug/nasa_compat_repr.hpp:1154`. Route them through `ensure_record` / `read`.
- Create: `tests/remote_loading/main.cpp`. Include `tests_config.hpp` and use `DATA_PATH`, as `tests/record_stream/main.cpp` does. Add `'remote_loading'` to the C++ test list in `tests/meson.build:14`.

**Interfaces:**
- Consumes: `remote_buffer`, `remote_buffer_options`, `local_range_reader`, `range_reader`, `page_size()` (Part A).
- Produces:
  - `template <cpp_utils::io::range_reader R> [[nodiscard]] std::optional<CDF> cdf::io::load(R reader, bool iso_8859_1_to_utf8 = true, bool lazy_load = true, cpp_utils::io::remote_buffer_options options = cpp_utils::io::remote_buffer_options::from_env("CDFPP_REMOTE_"))`. It returns `std::nullopt` when `reader.size() == 0`.
  - `template <typename record_t, typename buffer_t> void ensure_record(const buffer_t&, std::size_t offset)`:
    - It does nothing unless the buffer has `ensure`.
    - Otherwise it ensures the header, reads the record size, then ensures the whole record. The header size comes from `record_t`'s version: 12 bytes in v3, 8 in v2 (see `record_stream.hpp:164`).
    - For a header-only `record_t` (`is_cdf_DR_header_v`), it ensures only the header.

- [ ] **Step 1: Write the failing tests.**
  - `SCENARIO("Scenario: a CDF loaded through a range reader equals the mmap load")`: for every `*.cdf` in `DATA_PATH` except `not_a_cdf.cdf`, eager and lazy, `load(local_range_reader{p})` equals `load(p)`. It must be `CDF ==` plus equal `majority`, `compression`, `distribution_version`, and each variable's `compression_type()`. Lazy loads use `remote_buffer_options{ .read_ahead = page_size(), .tail_prefetch = 0 }`, so the parser really does walk unfetched pages.
  - A copy of `a_cdf.cdf` cut to 20 bytes (inside the CDR), loaded eagerly, throws `std::runtime_error`.
  - An empty file returns `std::nullopt`.
  - `ensure_record` called with a `const remote_buffer&` fetches (`bytes_served() > 0`).
- [ ] **Step 2: Run** `ninja -C build && meson test -C build remote_loading`. Expected: compile failure (no `load(reader)`).
- [ ] **Step 3: Implement** the overload and the adapter, then call `ensure_record<record_t>` at the top of each `load_record` overload. Ensuring inside `read` comes for free from `remote_buffer::read`.
- [ ] **Step 4: Run** `remote_loading`. Expected: PASS. A SIGSEGV on a resource means some path bypasses `load_record` / `read`. Find it and route it; never patch the test.
- [ ] **Step 5: Run** the project suite `meson test -C build --suite pycdfpp` (Catch2's own self-tests fail by design, so they're excluded). Expected: the pass count on `main` + 1, exit code 0.
- [ ] **Step 6: Commit** `feat(io): load CDFs through a byte-range reader`.

### Task 5: Opening cost (characterisation)

**Files:**
- Test: `tests/remote_loading/main.cpp`

The opening batch is already produced by Task 3: the first `ensure` adds the tail. This task pins its effect with numbers, so later changes can't regress it.

- [ ] **Step 1: Measure.** For `fgm_blocks.cdf`, print the open-time batches and bytes in three setups: (a) `read_ahead = page_size()`, `tail_prefetch = 0`; (b) the same plus `tail_prefetch = 32 KiB`; (c) defaults.
- [ ] **Step 2: Write the tests** from those numbers: (b) uses fewer batches than (a), and (c) uses exactly 1 batch. Put the measured values in the test as named constants.
- [ ] **Step 3: Run** `remote_loading`. Expected: PASS.
- [ ] **Step 4: Commit** `test(io): pin remote open cost`, with the table in the message.

### Task 6: Batched block discovery and data prefetch

**Files:**
- Modify: `include/cdfpp/cdf-io/loading/variable.hpp:116-177`:
  - `values_block` gains `std::size_t block_record_size`.
  - `list_blocks` / `list_var_blocks` batch-ensure each VXR's child headers, then visit them.
- Modify: `include/cdfpp/cdf-io/loading/variable.hpp:216-232`. `load_blocks` ensures every block extent on the calling thread, then marks its workers as "no fetching allowed".
- Modify: `include/cdfpp/cdf-io/loading/buffers.hpp`. When the current thread is a worker, `shared_buffer_t`'s `ensure` forwarding calls `check_present` instead of `ensure`.
- Test: `tests/remote_loading/main.cpp`

**Interfaces:**
- Consumes: `shared_buffer_t::ensure(std::span<const byte_range>)`, `shared_buffer_t::check_present` (Task 4).
- Produces:
  - `struct values_block { record_range records; std::size_t file_offset; std::size_t block_record_size; };` where `block_record_size` is the on-disk VVR/CVVR record size. It is not the variable's record size.
  - `inline thread_local bool cdf::io::no_fetch_thread = false;`, set inside the `parallel::for_each_index` lambda in `load_blocks`.

- [ ] **Step 1: Write the failing tests** on `many_compressed_blocks.cdf`, lazy, with `read_ahead = page_size()` and `tail_prefetch = 0`:
  - Loading one variable sends at most (VXRs in its chain) + 2 batches.
  - No byte is served twice (`bytes_served() <= file size`).
  - Every `caller_threads()` entry equals the test thread's id.
  - The bytes served for the data equal the sum of the blocks' `block_record_size`, give or take page rounding.
  - The values still equal the mmap load.
- [ ] **Step 2: Run.** Expected: FAIL on the batch count and on the thread ids.
- [ ] **Step 3: Implement** the restructure:
  1. Walk a VXR and collect its children's offsets.
  2. Send one `ensure` for all the child headers.
  3. Read the headers, keeping `block_record_size`.
  4. Recurse into sub-VXRs, then follow `VXRnext`.
  5. Before `parallel::for_each_index`, `load_blocks` sends one `ensure` over every `{file_offset, block_record_size}`.

  `count_blocks_in_vxr` (`variable.hpp:406-433`, the block counter) stays as it is: it runs after the load, when its pages are already present.
- [ ] **Step 4: Add the worker-guard test.** A test hook makes the prefetch skip its last block. The load must throw "remote bytes not fetched", and no reader call may come from a non-test thread. Run `remote_loading`, then the project suite. Expected: PASS, no regression.
- [ ] **Step 5: Benchmark guard.**
  1. Make a `git worktree` of `main` with the same options (`-Dwith_benchmarks=true -Dbuildtype=release`), in its own build dir.
  2. Run `ninja -C <dir> benchmark` in each tree, one after the other.
  3. Local load times must match within noise.
- [ ] **Step 6: Commit** `perf(io): batch VXR child headers and prefetch data blocks before decoding`.
- [ ] **Step 7: OpenCode review of Tasks 4–6.**

### Task 7: pycdfpp Python reader and GIL rules

**Files:**
- Create: `pycdfpp/remote.hpp` (the `python_range_reader` wrapper).
- Modify: `pycdfpp/cdf.hpp:308-321` (a new `_pycdfpp.load_reader` binding).
- Modify: `pycdfpp/__init__.py:984` (`load` detects a reader and forwards the keywords).
- Modify: the bindings that take a `Variable` lock while holding the GIL. Wrap the call in `py::gil_scoped_release`:
  - `pycdfpp/variable.hpp:717` (`type`), `:731` (`is_contiguous`), `:735` (`values_loaded`), `:753` (`virtual_records`), `:754` (`_has_virtual_records`).
  - `pycdfpp/collections.hpp:808` (`make_values_view` → `variable.type()`) and `:823` (`make_buffer` → `variable.type()`).
  - Already safe, leave alone: `collections.hpp:274-277` and `:339`, `variable.hpp:711-712`.
- Create: `tests/python_remote_loading/test.py` and `tests/python_remote_loading/loading_via_reader.py`. Add both to the `py_test` list in `tests/meson.build:34`, following that list's pattern.

**Interfaces:**
- Consumes: `cdf::io::load(R, bool, bool, remote_buffer_options)` (Task 4).
- Produces:
  - `class python_range_reader` satisfying `range_reader`:
    - It holds a raw `PyObject*` behind a `std::shared_ptr`, since copies share it. It does not hold a `py::object`, whose destructor always decrefs.
    - `size()` and `read_ranges` first check `Py_IsInitialized() && !finalizing` and throw `std::runtime_error("Python reader used after interpreter shutdown")` otherwise. `finalizing` is `Py_IsFinalizing()` on 3.13+ and `_Py_IsFinalizing()` before. They then take the GIL with `py::gil_scoped_acquire`.
    - If the object has `read_ranges_into(ranges, buffers)`, it passes writable `memoryview`s over the destinations. Otherwise it calls `read_ranges(ranges) -> list[bytes-like]` and checks each length; a mismatch throws `std::runtime_error`.
    - The shared state's deleter decrefs under the GIL only when Python is alive and not finalising. Otherwise it leaks on purpose (say so in a one-line comment).
  - Python: `pycdfpp.load(file_or_buffer_or_reader, iso_8859_1_to_utf8=True, lazy_load=True, *, read_ahead=None, tail_prefetch=None, backing=None, temp_dir=None, download_whole_below=None)`.
    - A reader is any object with `size` and either `read_ranges` or `read_ranges_into`. It is checked before the buffer branch.
    - `None` means "environment variable, then default".
    - Passing a knob with a path or a buffer raises `TypeError`.
    - `py::keep_alive<0, 1>()` on the binding.

- [ ] **Step 1: Write the failing tests** in `test.py`, using a `FileReader` class over a resource file with a call log:
  - `test_equals_path_load`: every resource, eager and lazy, equals `pycdfpp.load(path)`.
  - `test_reader_raising_propagates`: a reader that raises `OSError` on its second call makes `.values` raise. Once the reader is healed, `.values` works.
  - `test_short_read_raises`: a reader that returns one byte less makes the load raise `RuntimeError`.
  - `test_read_ranges_into_used`: a reader with only `size` and `read_ranges_into` works.
  - `test_env_overrides`: a subprocess with `CDFPP_REMOTE_READ_AHEAD=4096` and `CDFPP_REMOTE_TAIL_PREFETCH=0` opens `fgm_blocks.cdf`. The first batch is 4096 bytes. `read_ahead=8192` as a keyword wins over the environment.
  - `test_no_deadlock_same_variable`: thread A reads `cdf["var"].values` while thread B loops on `cdf["var"].type` until A finishes. The reader sleeps 50 ms per call to widen the window. Both threads join within 30 s.
  - `test_exit_with_live_lazy_cdf`: a subprocess keeps a lazy CDF from a Python reader in a module global, with one variable unloaded, and exits. Return code 0, nothing on stderr.
  - `loading_via_reader.py` patches `pycdfpp.load` to wrap `str` / `PathLike` arguments in `FileReader`, then runs the `tests/python_loading` test module through `unittest`. That is the spec's "existing loading tests run a second time through that fixture".
- [ ] **Step 2: Run** `ninja -C build && meson test -C build python_remote_loading`. Expected: FAIL (`load` rejects the reader).
- [ ] **Step 3a: Implement** the wrapper, the binding and the `__init__.py` dispatch, with no GIL changes yet. Run the tests. Expected: everything passes except `test_no_deadlock_same_variable`, which times out. That proves the deadlock is real.
- [ ] **Step 3b: Add the GIL releases** listed under Files. Run again. Expected: all PASS.
- [ ] **Step 4: Run** `meson test -C build --suite pycdfpp`. Expected: all PASS, including `python_threads`, exit code 0.
- [ ] **Step 5: Document** remote loading in `docs/reading.rst`: the reader protocol, an fsspec example, and every knob with its default and environment variable. Explain why `download_whole_below` is opt-in: many small files each read partially would cost more. Run `docs/check_examples.py` if it covers `reading.rst`.
- [ ] **Step 6: Commit** `feat(pycdfpp): load from a Python byte-range reader`.
- [ ] **Step 7: OpenCode review of Task 7.**

### Task 8: HelioCloud benchmark (outside CI)

**Files:**
- Create: `benchmarks/remote/heliocloud.py`

**Interfaces:**
- Consumes: `pycdfpp.load(reader, ...)` (Task 7).

- [ ] **Step 1: Write the script.**
  - Adapters: `FsspecReader` (`s3fs`, `anon=True`, `cat_ranges`), and `ObstoreReader` with the sync `obstore.get_ranges` (the `read_ranges_into` variant was dropped with that protocol, see the spec, section 1). Each adapter counts requests and bytes.
  - Arms: each adapter; a local `FileReader` over a downloaded copy (the C++ cache with no network), to which the measured RTT × batches is added; and a full download plus `pycdfpp.load(path)`.
  - Workloads on the spec's sample file: open, load 1 variable, load 3, load all.
  - Output: one table of wall time, requests and bytes per arm and workload, plus a sweep of `read_ahead` over 64 KiB, 256 KiB and 1 MiB.
  - Run it with `PYTHONPATH=build uv run --no-project --with s3fs --with obstore --with numpy python benchmarks/remote/heliocloud.py`.
  - Downloads go into a `tempfile.TemporaryDirectory(dir="build")`, not `/tmp`, which is RAM here.
- [ ] **Step 2: Run it once.** Paste the table into the commit message and into the spec's "What we measured" section.
- [ ] **Step 3: Commit** `bench: HelioCloud remote read benchmark`.

### Task 9: Wrap bump and PRs (each push needs the user's consent)

- [ ] **Step 1:** Ask the user before pushing cpp_utils `feat/remote-buffer` and opening its PR.
- [ ] **Step 2:** After it merges:
  1. Set `revision` in `subprojects/cpp_utils.wrap` to the merged commit.
  2. Run `meson subprojects update cpp_utils`. `meson wrap update` does not move a wrap-git revision.
  3. Rebuild and run the project suite.
  4. Commit `build: bump cpp_utils for remote_buffer`.
- [ ] **Step 3:** Run `meson wrap update` as routine hygiene, then rebuild and run the full suite again. Commit only if it changed something.
- [ ] **Step 4:** Final OpenCode review of the whole `feat/remote-reads` branch. Then ask the user before pushing and opening the CDFpp PR.
