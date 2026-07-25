# Design: transparent `cdfirsdump` CLI (Phase A)

**Date:** 2026-07-25
**Status:** approved design — not yet implemented
**Branch:** `feature/debug-record-stream-cdfdump`

## Why

`cdfdump --irsdump <path>` already reproduces NASA's real `cdfirsdump -full -nopage
-nosummary` output byte-for-byte (commit `96c2019`,
`include/cdfpp/cdf-io/debug/nasa_compat_repr.hpp`), but it's a flag bolted onto CDFpp's
own `cdfdump` tool, not something a NASA-toolkit user would find. The user wants a
**separate, name-matching `cdfirsdump` command** — conceptually a drop-in replacement
for the real tool, so anyone who knows `cdfirsdump` can reach for CDFpp's version
without learning a new tool name.

**Not a literal drop-in.** The real `cdfirsdump` uses single-dash flags
(`-full -nopage -nosummary`). This design uses modern double-dash flags
(`--level full --no-summary`) — semantic parity with the real tool's concepts, not
byte-for-byte CLI compatibility. This was an explicit user choice, confirmed via
`AskUserQuestion` in the prior session; don't reintroduce single-dash parsing.

## Scope

**In scope (Phase A):** a `cdfirsdump` console script with the flag surface below,
covering `-full`/`-brief` (as `--level`), `-nosummary` (as `--summary`), `-data`,
`-radix`, an output-file option, and an arbitrary start offset.

**Out of scope (explicitly deferred, all confirmed with the user — don't re-ask):**
- `-page`/`-nopage` — dropped entirely. This tool always behaves as `-nopage`; TTY
  paging isn't meaningful for a modern CLI and every existing fixture/test already
  assumes unpaged output.
- `-most` — accepted as a value but **aliases to `full`**. NASA's real MOST level gates
  individual fields per record type; that gating was never independently verified
  against real captured output (only FULL was, in `96c2019`), so treating it as
  full-with-a-documented-gap is honest; silently mislabeling it as a real MOST
  implementation would not be.
- `-ziso8601` (alternate Z-suffixed epoch precision) — real feature, lower value, moved
  to a follow-up.
- Phase B (rich tree mode, JSON/YAML output, filtering) — a separate design, not started
  until Phase A ships. See "Phase B" below for the scoped-but-undesigned summary.

## Flag surface

| Flag | Default | Behavior |
|---|---|---|
| `--level {brief,full}` | `brief` | `brief`: summary table only (record-type counts/bytes/percentages — new code, not built yet). `full`: existing per-record dump (`nasa::dump()`). `most` is accepted as an alias for `full`. |
| `--summary` / `--no-summary` | on | Whether to print the closing summary table after a `--level full` dump. `--no-summary --level brief` is a degenerate empty-output case — allowed, not special-cased. |
| `--data` / `--no-data` | off | Hex-dump VVR/CVVR/CCR payload bytes, 38 bytes/line (`BYTESperLINE=38`, matching the real tool's wrapping). Needs a fresh real `cdfirsdump -data` capture for TDD ground truth before implementation — no existing reference for this. |
| `--output <file>` | stdout | Write dump text to a file instead of stdout. |
| `--offset <n>` | none (walk from file start) | Start the physical-record walk at byte offset `n` instead of the file's magic-number preamble. |
| `--radix {10,16}` | `10` | Decimal (already built and covered by every existing test) vs. hex offsets. Real format per source research: `"0x%016llX"` — 16 hex digits, uppercase, `0x` prefix. Needs a fresh real capture with `-radix=16` to verify before trusting this from memory. |
| `--about` | — | Print CDFpp/pycdfpp version and exit. |

## Architecture

**Extend `nasa_compat_repr.hpp`, don't fork it.** Every `print_nasa(ostream&, offset,
record[, encoding])` overload (~15 functions, `nasa_compat_repr.hpp:319-850`) gains a
small options context threaded alongside the existing `encoding` parameter:

```cpp
struct dump_options
{
    int radix = 10;
    bool show_data = false;
};
```

This is a mechanical signature change across every `print_nasa` overload and the
`dump()` entry points (`nasa_compat_repr.hpp:858,875`). `nasa_offset()`
(`nasa_compat_repr.hpp:66`) becomes the one function that actually branches on radix;
everything else just forwards `dump_options` down to it.

**Two new entry points**, same walker, different consumer — no new parsing logic:

1. **Summary walk** — accumulate `record_type -> (count, bytes)` while walking via the
   existing `for_each_record`, print as a table at the end. Backs `--level brief` and
   the `--summary` trailer on `--level full`.
2. **Offset-starting dump** — `for_each_record` already has a `start_offset` parameter
   at the core-buffer overload (`record_stream.hpp:160`); the convenience entry points
   (`record_stream.hpp:363-420`) just don't expose it yet. Add an overload that does, and
   a corresponding `nasa::dump(os, path, start_offset, dump_options)`.

**Data hex-dump** (`--data`) plugs into the existing VVR/CVVR/CCR `print_nasa` overloads
(`nasa_compat_repr.hpp:784,792,818`) — when `dump_options.show_data` is set, dump the
record's raw payload bytes after the existing header line, 38 bytes/line. Implemented
last, after a fresh real capture exists to verify against (see Testing).

## CLI & Python surface

- New `pycdfpp/cli_irsdump.py`, cyclopts-based (same pattern as `pycdfpp/cli.py`), with
  a `[project.scripts]` entry `cdfirsdump = "pycdfpp.cli_irsdump:main"` in
  `pyproject.toml` (alongside the existing `cdfdump = "pycdfpp.cli:main"`).
- New C++ bindings in `pycdfpp/debug.hpp`: a second bound function taking the
  radix/data/offset options (the existing `nasa_compat_dump` binding takes no
  arguments today — leave it as the zero-option convenience form, add a new
  `nasa_compat_dump_ex(path, radix=10, show_data=False, offset=None)` rather than
  breaking the existing signature).
- `--about` reads whatever version symbol `cdfdump` itself already exposes (check
  `pycdfpp/cli.py` for the existing pattern rather than inventing a new one).

## Testing

Same TDD discipline as the rest of this feature area (`record_stream`, `record_repr`,
`nasa_compat_repr` were all written test-first against real captured
`cdfirsdump` output, never hand-derived expectations):

1. **Radix**: get a fresh real `cdfirsdump -full -nopage -nosummary -radix=16` capture
   of `a_cdf.cdf` first — it's the most mechanical change (touches only `nasa_offset`) —
   and add it as a second reference file alongside the existing
   `tests/resources/a_cdf_cdfirsdump_reference.txt`. Confirm the existing decimal-radix
   tests still pass unmodified (radix 10 stays the default).
2. **Summary**: capture real `cdfirsdump -brief` (or equivalent) output for the same
   fixture set used in `96c2019` (`a_cdf.cdf`, `a_compressed_cdf.cdf`,
   `a_cdf_with_compressed_vars.cdf`, `rvariable.cdf`, `testutf8.cdf`) as ground truth for
   the summary-table format.
3. **Offset**: no new capture needed — verify against a byte-offset slice of the
   existing `--radix 10 --level full` reference (dump from a mid-file offset should
   equal the tail of the full dump starting at the record whose offset matches).
4. **Data**: capture real `cdfirsdump -data` output last, once the walker/formatting
   groundwork above is in place.
5. New test files (`tests/nasa_compat_repr_ex/main.cpp` or extend the existing
   `tests/nasa_compat_repr/main.cpp` — decide at implementation time based on how large
   it gets) must be added to **both** `tests/meson.build`'s test list
   (`tests/meson.build:12`) **and** `.github/workflows/tests-with-sanitizers.yml`'s
   hand-enumerated list (`tests-with-sanitizers.yml:40`) — this project's CI does not
   discover tests automatically, confirmed the hard way twice already in this feature
   area (`96c2019`'s own history, plus the `cli.py` CLI rebuild before it). Python:
   `tests/python_debug/test.py` or a new `tests/python_cli_irsdump/` — same
   double-registration rule applies to `tests-with-sanitizers.yml:83` and
   `tests-windows.yml`'s hand-listed pip installs (needs `cyclopts`/`rich` added there
   too, same gap fixed for `cdfdump` in `96c2019`).

## Phase B (not designed — scoped only, for context)

Deferred until Phase A ships and is stable. Three pieces, one of which has an open
design question rather than a settled shape:

1. **Rich tree output** (`--pretty`) — reuse `cdfdump`'s existing `build_tree()`
   (`pycdfpp/cli.py:29`) directly; likely the cheapest of the three.
2. **Structured output** (`--format {text,json,yaml}`) — serialize the same
   `for_each_record` data as JSON/YAML instead of text.
3. **Filtering** — by record type and offset range are straightforward predicates over
   the physical walk. Filtering by **variable/attribute name** is a genuine open
   question: `for_each_record`'s entire design point is being variable-agnostic
   (physical order, no ownership tracking), so this requires cross-referencing the
   physical walk against `load()`'s reconstructed ownership graph — new correlation
   logic, not a predicate. Don't assume a shape for this; it needs its own brainstorm
   when Phase B starts.

## Open questions for implementation time

- Whether `--data`'s hex-dump format needs its own `dump_options` sub-struct or stays a
  flat `bool` — depends on what the real `-data` capture actually shows once fetched.
- Exact wording/columns of the `--level brief` summary table — depends on the real
  capture in Testing step 2; don't hand-derive from memory of the source.
