# cdflib compatibility layer — design

## Intent

People who read or produce CDF files with cdflib (L1/L2/L3 pipelines, pyspedas-style readers)
should be able to switch to pycdfpp by changing one import, and get its speed. They switch
softly: code keeps running, then they migrate to the native API when they want.

Target: **cdflib 1.3.16**, the whole public interface. Not a subset picked from a usage survey:
any missing call breaks whoever relies on it.

## Goals

- `import pycdfpp.cdflib as cdflib` runs existing cdflib code unchanged, readers and writers.
- Every public module, class, function, argument and returned dataclass of cdflib 1.3.16 exists
  with the same name and signature.
- Results equal cdflib's, checked by tests that run real cdflib side by side as the oracle.
- Faster than cdflib on the same calls (file parsing, decompression, time conversion).

## Non-goals

- Tracking cdflib versions automatically. A newer cdflib is a new target, updated by hand.
- Exposing pycdfpp-only features through the cdflib API.

## Anti-goals

- Silently returning different values. Every deliberate difference is listed in the docs.
- Copying cdflib's bugs. Where cdflib is wrong, we are right, and document it (user's call).

## Public surface (cdflib 1.3.16)

| cdflib module | What it holds | Our approach |
|---|---|---|
| `cdflib` | `CDF`, `cdfepoch` (= `epochs.CDFepoch`), submodules | reexport |
| `cdfread.CDF` | `__init__(path, validate, string_encoding, s3_read_method)`, `cdf_info`, `varinq`, `varget`, `varattsget`, `globalattsget`, `attinq`, `attget`, `vdr_info`, `__getitem__`, context manager | **rewrite** on `pycdfpp.load` |
| `cdfwrite.CDF` | `__init__(path, cdf_spec, delete)`, `write_globalattrs`, `write_variableattrs`, `write_var`, `close`, context manager | **rewrite** on `pycdfpp.CDF` + `pycdfpp.save` |
| `epochs.CDFepoch` | `compute*`, `encode*`, `breakdown*`, `parse`, `to_datetime`, `unixtime`, `timestamp_to_*`, `findepochrange`, `epochrange_*` | **rewrite**, vectorised on pycdfpp chrono |
| `epochs_astropy.CDFAstropy` | astropy flavour of the above | **reuse** cdflib's (it never calls the reader/writer) |
| `dataclasses` | `CDFInfo`, `VDRInfo`, `ADRInfo`, `AttData`, `VDR`, `AEDR`, `CDRInfo`, `GDRInfo` | **reuse** cdflib's classes (so `isinstance` checks keep working) |
| `xarray` | `cdf_to_xarray`, `xarray_to_cdf` (ISTP checks) | **reuse** cdflib's, with its `CDF` and `cdfepoch` names rebound to ours |
| `s3`, `utils`, `logging` | S3/URL reading helpers | **reuse** cdflib's where they don't parse the file |

**cdflib is an optional dependency** (`pip install pycdfpp[cdflib]`, user's call). The layer
only replaces the parts that parse, write or convert data; everything else is cdflib's own code.
Importing `pycdfpp.cdflib` without cdflib installed raises an ImportError saying which extra to
install. cdflib is pure Python on numpy, so requiring it costs little.

Reused modules bind our classes at import time (`from cdflib import CDF`, `from cdflib.epochs
import CDFepoch as cdfepoch`). The layer rebinds those few module globals to our classes. That
couples us to cdflib internals, so: the extra pins a tested range (`cdflib>=1.3.16,<1.4`), and a
test fails if a reused module grows a new reference to the reader, writer or epochs that we
don't rebind. Optional dependencies stay optional as in cdflib: xarray, astropy, boto3 are
imported only by the modules that need them.

## Decided

- **Package layout** mirrors cdflib: `pycdfpp/cdflib/{__init__,cdfread,cdfwrite,epochs,
  epochs_astropy,dataclasses,s3,utils,logging}.py` and `pycdfpp/cdflib/xarray/`. So
  `from pycdfpp.cdflib.epochs import CDFepoch` works like `from cdflib.epochs import CDFepoch`.
- **Pure Python**, no C++ changes unless a behaviour cannot be expressed otherwise.
- **Oracle tests**: real cdflib (plus xarray, astropy) is a test-only dependency. Tests run the
  same calls through both and compare, on `tests/resources/*.cdf` and on generated files.
  Write tests write the same spec with both writers, then read each output with both readers.
- **Better behaviour where cdflib is wrong** is fine, listed in a "Differences from cdflib" doc
  page. Known so far:
  - CDF_EPOCH keeps fractional milliseconds (cdflib truncates).
  - Fill/pad epochs become NaT (cdflib raises OverflowError on EPOCH arrays with fill).
  - `write_var` with datetime64 data for a TT2000 variable stores the right instants
    (cdflib stores raw ns since 1970: dates 30 years late).

## Calls made by me (check these)

- **`cdfwrite` builds in memory and saves on `close()`/`__exit__`.** cdflib writes to the file as
  it goes; the visible result is the same file after `close()`. A writer never closed writes
  nothing (cdflib leaves a partial file).
- **rVariables are written as zVariables** with the same shape: pycdfpp's writer only emits
  zVariables. Readers see the same data; `varinq().Var_Type` on the re-read file says
  `zVariable`. Listed as a difference.
- **Low-level file offsets** in `VDR`, `ADRInfo`, `AEDR`, `GDRInfo` (`next_vdr_location`,
  `head_vxr`, `next_adr_loc`, `first_zvariable`, `eof`, …) have no pycdfpp equivalent. They are
  set to 0. They only mean something to code walking the file by hand. Listed as a difference.
- **`validate=True`** checks the MD5 of the file bytes in Python (hashlib), as cdflib does.
- **`import cdflib` for third-party code**: `pycdfpp.cdflib.install()` rebinds the reader,
  writer and epochs names inside the real, already-installed cdflib modules, so libraries that
  import cdflib themselves (pyspedas) switch too, without editing them. It must run before those
  libraries do `from cdflib import CDF`; names they already copied keep cdflib's classes (stated
  in its docstring). Opt-in only, never done at import.
- **Error types and messages** match cdflib where a test can observe them (e.g. `KeyError` vs
  `ValueError` on an unknown variable), checked by the oracle tests.

- **What counts as "wrong"**: the user set the policy (behaving better is fine); the builder
  applies it per case without sign-off. Judge = the CDF spec and NASA's reference library. If we
  agree with NASA and cdflib doesn't, keep ours and list it. Otherwise match cdflib exactly.
  Example: names with non-ASCII bytes decode as ISO-8859-1 (NASA) instead of failing.
- **Scope of "public"**: every name without a leading underscore — modules, classes, functions,
  methods, module constants, type aliases, class constants (`CDFepoch.J2000LeapSeconds`, …) and
  public instance attributes (reader `file`, `string_encoding`, `cdfversion`, …; writer `path`,
  `is_closed`, `zvars`, `gattrs`, `compression`, `checksum`). Underscore helpers are out.
  "Same signature" = same parameter names, order, kinds and defaults; annotations may differ. A
  test compares `inspect.signature` parameters of every public callable against real cdflib.
- **Speed is a target for real workloads**, not a per-call gate: load + `varget`, bulk time
  conversion, writing, xarray round trips. A small benchmark covers those. Single-value epoch
  calls may be slower; that's acceptable and noted.
- **Remote files**: all three `s3_read_method` values are accepted. All of them fetch the whole
  object into memory and pass bytes to `pycdfpp.load`, because pycdfpp needs the whole file.
  The visible result is the same. Tests serve a file over a local `http.server` for URLs and use
  a fake boto3 client for S3.
- **`__version__`** of the layer reports the cdflib version it mimics (`"1.3.16"`), so third-party
  version checks behave. pycdfpp's own version stays in `pycdfpp.__version__`.
- **Writer leaving a `with` block on an exception** does not save (no partial file).

## Left to the builder

- Internal structure of the three rewrites (keep functions small, data-driven type tables).
- Mapping tables between cdflib type names/numbers (`CDF_REAL4`, 21, …) and `pycdfpp.DataType`.
- Order of work. Suggested: dataclasses → epochs → cdfread → cdfwrite → vendored xarray/astropy
  → `install()` → docs page. Each step lands with its oracle tests.
