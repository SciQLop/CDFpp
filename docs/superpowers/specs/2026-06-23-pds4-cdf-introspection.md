# Handover: expose CDF structural introspection for PDS4/CDF-A validation

**Date:** 2026-06-23
**Status:** handover / design — not yet implemented
**Downstream driver:** [AstraLint](https://github.com/SciQLop/AstraLint) PDS4 suite (CDF-A archiving rules)

## Why

AstraLint validates CDFs against the PDS4 **CDF-A** archiving spec
(*"Guide to Archiving CDF Files in PDS4", Rev 7*). Of the CDF-A "Requirements for
Archivable CDF files", two are **per-variable / per-file structural** facts that
`pycdfpp` (0.10.0) does **not** expose, so AstraLint cannot check them:

- **No fragmented variables** — "all data for a variable must be contiguous in the file."
- **zVariables only** — "Use only zVariables."

(A third, "CDF version ≥ 3.4", is already covered: AstraLint reads it from
`cdf.distribution_version`. A fourth, "single-file CDF", is currently satisfied by
construction on AstraLint's side, but is trivial to expose here too — see Bonus.)

This handover adds three read-only introspection properties to CDFpp + `pycdfpp` so
AstraLint can finish `PDS4-CDFA-004` (contiguity) and `PDS4-CDFA-005` (zVariables-only).

## API to add

Mirror the existing read-only properties (`Variable.is_nrv`, `Variable.majority`,
`CDF.distribution_version`).

| Property | Type | Meaning |
| --- | --- | --- |
| `Variable.is_zvariable` | `bool` | `True` if the variable is a zVariable (modern), `False` if a legacy rVariable |
| `Variable.is_contiguous` | `bool` | `True` if the variable's record data is stored as a single VVR/CVVR block (not fragmented across multiple writes) |
| `CDF.is_single_file` *(bonus)* | `bool` | `True` if the CDF is single-file, `False` if multi-file |

Python usage AstraLint will write:

```python
cdf = pycdfpp.load("x.cdf")
all(v.is_zvariable for v in cdf.values())     # PDS4-CDFA-005
all(v.is_contiguous for v in cdf.values())    # PDS4-CDFA-004
cdf.is_single_file                            # bonus
```

## CDF-format background (so the approach is trustworthy)

- **z vs r variables.** CDF stores variables in two separate VDR (Variable Descriptor
  Record) linked lists: zVDRs from `GDR.zVDRhead` (count `NzVars`) and rVDRs from
  `GDR.rVDRhead` (count `NrVars`). rVariables are a legacy CDF-2.x concept; modern files
  are all-z. CDFpp already iterates both lists.
- **CDR flags** (`cdf_CDR_t::Flags`, `include/cdfpp/cdf-io/desc-records.hpp`): bit 0 = row
  majority, **bit 1 = single-file**, bit 2 = checksum, bit 3 = MD5. CDFpp already reads
  bit 0 in `common::majority` (`include/cdfpp/cdf-io/common.hpp:95`).
- **Variable record layout / fragmentation.** A VDR's records are indexed by
  `VXRhead → VXR → {VVR | CVVR | nested VXR}` (and `VXRnext` chains). Each VXR has
  `NusedEntries` with `First[]/Last[]/Offset[]`; each `Offset` points at a VVR (raw bytes),
  a CVVR (compressed bytes), or a sub-VXR. A variable written in a single operation has
  exactly **one** leaf VVR/CVVR block; incremental writes fragment it into several.
  So **`is_contiguous ⟺ leaf VVR/CVVR block count ≤ 1`**.

## Where & how (exact sites)

### 1. `Variable.is_zvariable` — statically known at parse

`load_all_Vars<cdf_r_z type, ...>` (`include/cdfpp/cdf-io/loading/variable.hpp`, ~line 246)
is templated on `type`, so the z/r flag is a compile-time constant there:

```cpp
constexpr bool is_z = (type == cdf_r_z::z);
```

Thread it through:
- `common::add_variable` / `common::add_lazy_variable`
  (`include/cdfpp/cdf-io/common.hpp:162` / `:172`) — add a `bool is_zvariable` param.
- The two `Variable` constructors (`include/cdfpp/variable.hpp:94` and `:116`) — add a
  trailing `bool is_zvariable = true` param + a `p_is_zvariable` member, defaulting to
  `true` for backward-compat (most callers create z-vars).
- Getter `is_zvariable()` next to `is_nrv()` (`include/cdfpp/variable.hpp:225`).
- Binding `.def_property_readonly("is_zvariable", &Variable::is_zvariable)` next to
  `is_nrv` (`pycdfpp/variable.hpp:597`).
- Keep it out of `operator==` (or include it — see "Open questions").

### 2. `Variable.is_contiguous` — count leaf blocks via the VXR chain

Add a counting traversal that mirrors `load_var_data`'s VXR walk
(`include/cdfpp/cdf-io/loading/variable.hpp:133-211`) but **reads no variable data** —
it only follows VXR index records (small, cheap), so it works in lazy-load mode too:

```cpp
template <typename cdf_version_tag_t, typename stream_t>
std::size_t count_var_blocks(stream_t& stream, std::size_t vxr_offset);  // leaf VVR/CVVR count
```

Walk `vdr.VXRhead` then `VXRnext`; for each VXR, for each of `NusedEntries`, load the
record at `Offset[i]`: if it's a VVR or CVVR, `++count`; if it's a sub-VXR, recurse. Then
`is_contiguous = (count <= 1)`. Compute it in `load_all_Vars` (it has `context`/stream +
`vdr`) and thread it through `add_variable`/`add_lazy_variable` → `Variable` ctor (member
`p_contiguous`, default `true`) → getter `is_contiguous()` → binding.

Edge cases:
- `vdr.VXRhead == 0` (0-record variable) → count 0 → contiguous (vacuously `true`).
- NRV variable → single record → one VVR → contiguous.
- Compressed variable → CVVR leaf; still counts as one block. (CDF-A also forbids
  compression, but AstraLint checks that separately — keep `is_contiguous` orthogonal to
  compression.)

### 3. `CDF.is_single_file` — CDR flag (bonus, ~10 lines)

Mirror `common::majority` exactly:

```cpp
// include/cdfpp/cdf-io/common.hpp, beside majority()
template <typename T>
auto is_single_file(const T& cdr) -> decltype(cdr.Flags, true) { return (cdr.Flags & 2) == 2; }
```

- Add `bool is_single_file` to `common::cdf_repr` (`common.hpp:111`) and to `CDF`
  (`include/cdfpp/cdf.hpp`, beside `majority`/`distribution_version`).
- Set it in the loader where `majority`/`distribution_version` are set:
  `parsing_context`/`repr` (`loading.hpp:65, 87-88`) and `from_repr` (`loading.hpp:72-73`).
- Bind `.def_property_readonly("is_single_file", ...)` in `pycdfpp/cdf.hpp:93`.

## Testing

CDFpp tests: Catch2 v3, one dir per test under `tests/` (C++), plus `tests/*/test.py`
(Python). Add:

- **C++**: a `tests/structural_introspection/` that loads fixtures and asserts
  `is_zvariable`, `is_contiguous`, `is_single_file`.
- **Python**: assert the three properties on the fixtures + a regression against a known
  real file.

Fixtures needed (generate with NASA's `cdfconvert`/`SkeletonCDF`, or craft):
- A **fragmented** variable (write a variable's records in ≥2 separate operations) →
  `is_contiguous == False`; and its `cdfconvert`-normalised copy → `True`.
- A CDF containing an **rVariable** → `is_zvariable == False`.
- A **multi-file** CDF → `is_single_file == False`.
- AstraLint's reference `mms1_asp2_srvy_l1b_stat_00000000_v01.cdf` is contiguous, all-z,
  single-file, v3.5.0 — good as a positive control if copied into fixtures.

## Downstream (AstraLint) — after a pycdfpp release

1. Add `is_zvariable: bool` and `is_contiguous: bool` to `Variable` in
   `src/astralint/base/file.py` (optional, default `None`/`True`).
2. Populate them in `src/astralint/codecs/cdf.py::_parse_variable` from the new pycdfpp props.
3. Add rules `PDS4-CDFA-004` (NoFragmentedVariables, per-var `is_contiguous`) and
   `PDS4-CDFA-005` (zVariablesOnly, per-var `is_zvariable`), both ERROR, under
   `src/astralint/suites/PDS4/rules/Structural/`. See the AstraLint spec
   `docs/superpowers/specs/2026-06-22-pds4-cdfa-rules-design.md`.
4. Bump the AstraLint demo's `micropip.install('astralint>=…')` and the pycdfpp lower
   bound; ensure the new pycdfpp release ships **emscripten/Pyodide** wheels (the AstraLint
   web demo runs pycdfpp in-browser).

## Open questions

- Should `is_zvariable`/`is_contiguous` participate in `Variable::operator==`? Probably
  **no** — they're physical-layout facts, not logical content; equality should stay
  content-based (current behaviour compares name/is_nrv/compression/shape/data).
- Naming: `is_contiguous` vs `is_fragmented` (inverse). Recommend `is_contiguous` so all
  three new props read as "good when true."
- Should `count_var_blocks` be cached/lazy? It's cheap (index records only); compute eagerly
  at parse for both eager and lazy variable loading.
