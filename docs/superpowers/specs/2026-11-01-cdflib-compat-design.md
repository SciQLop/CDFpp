# cdflib compatibility layer — design

## Intent

People who read or produce CDF files with cdflib (L1/L2/L3 pipelines, pyspedas-style readers)
should be able to switch to pycdfpp by changing one import, and get its speed. They switch
softly: code keeps running, then they migrate to the native API when they want.

Target: **cdflib 1.3.16**, the whole public interface. Not a subset picked from a usage survey:
any missing call breaks whoever relies on it.

## Goals

- `import pycdfpp.cdflib as cdflib` runs existing cdflib code unchanged, readers and writers.
- **Exact same user API** (user's call): every public module, class, function, argument and
  returned dataclass of cdflib 1.3.16 exists with the same name and signature, and returns the
  same Python types and array shapes.
- **Right output, not identical output** (user's call): values read are correct, and files
  written are valid CDF (and valid ISTP for `xarray_to_cdf`) holding the intended data. They do
  not have to be byte-identical to cdflib's files, nor built the same way. Tests use real cdflib
  as a reference for the API and for reading, not as a byte-for-byte target.
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
| `epochs_astropy.CDFAstropy` | astropy flavour of the above | **reexport** cdflib's public module (it never touches the reader, writer or epochs) |
| `dataclasses` | `CDFInfo`, `VDRInfo`, `ADRInfo`, `AttData`, `VDR`, `AEDR`, `CDRInfo`, `GDRInfo` | **reexport** cdflib's public module (so `isinstance` checks keep working) |
| `xarray` | `cdf_to_xarray`, `xarray_to_cdf` (ISTP checks) | **rewrite** on pycdfpp: same arguments and options, ISTP-correct result |
| `s3`, `utils`, `logging` | S3/URL reading helpers | **rewrite** what the reader needs; reexport the rest if public and self-contained |

**cdflib is an optional dependency** (`pip install pycdfpp[cdflib]`, user's call). Importing
`pycdfpp.cdflib` without it raises an ImportError naming the extra. cdflib is pure Python on
numpy, so requiring it costs little.

**No dependency on cdflib internals** (user's call). We only import cdflib's public modules
as they are (`cdflib.dataclasses`, `cdflib.epochs_astropy`), and we never patch or rebind
anything inside cdflib. Everything that reads, writes or converts data is ours, including the
xarray converters. So a cdflib update can only break us through its public API, which the
signature test catches. The extra still pins a tested range (`cdflib>=1.3.16,<2`). Optional
dependencies stay optional as in cdflib: xarray, astropy, boto3 are imported only by the
modules that need them.

## Decided

- **Package layout** mirrors cdflib: `pycdfpp/cdflib/{__init__,cdfread,cdfwrite,epochs,
  epochs_astropy,dataclasses,s3,utils,logging}.py` and `pycdfpp/cdflib/xarray/`. So
  `from pycdfpp.cdflib.epochs import CDFepoch` works like `from cdflib.epochs import CDFepoch`.
- **Pure Python**, no C++ changes unless a behaviour cannot be expressed otherwise.
- **Reference tests**: real cdflib (plus xarray, astropy) is a test dependency.
  - API: every public callable's parameters match real cdflib (`inspect.signature`).
  - Reading: the same calls through both on `tests/resources/*.cdf` and generated files must
    give equal values, types and shapes, except listed differences.
  - Writing: a file we write must read back right through real cdflib and through pycdfpp's
    native API, and pass NASA's `cdfvalidate` where available. Bytes are not compared.
  - xarray: `xarray_to_cdf` output, read back by real cdflib's `cdf_to_xarray`, gives the
    dataset we wrote; our `cdf_to_xarray` gives the same dataset as cdflib's on the test files.
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
- **`import cdflib` for third-party code**: `pycdfpp.cdflib.install()` registers our modules
  in `sys.modules` under the `cdflib` names (`cdflib`, `cdflib.cdfread`, `cdflib.cdfwrite`,
  `cdflib.epochs`, `cdflib.xarray`, …), so libraries that import cdflib themselves (pyspedas)
  get ours, without editing them and without touching cdflib's code. It must run before those
  libraries import cdflib (stated in its docstring). Opt-in only, never done at import.
- **Exception types** match cdflib (they are part of the API: user code catches them).
  Messages may differ.

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
- Order of work. Suggested: dataclasses → epochs → cdfread → cdfwrite → xarray
  → `install()` → docs page. Each step lands with its reference tests.
