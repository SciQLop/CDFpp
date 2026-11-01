# cdflib compatibility layer Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** `import pycdfpp.cdflib as cdflib` runs any cdflib 1.3.16 code (readers and writers) unchanged, faster, with correct output.

**Architecture:** A pure-Python subpackage `pycdfpp/cdflib/` mirroring cdflib's module layout. The reader, writer, epoch class and both xarray converters are our own code on pycdfpp's public API. Only cdflib's public, self-contained modules `cdflib.dataclasses` and `cdflib.epochs_astropy` are reexported. Real cdflib is the reference in tests for the API and for read results.

**Tech Stack:** Python ≥3.9, numpy, pycdfpp, cdflib 1.3.16 (optional extra), xarray/astropy/boto3 (optional, imported lazily), unittest + ddt, meson.

**Spec:** `docs/superpowers/specs/2026-11-01-cdflib-compat-design.md`

## Global Constraints

- Target cdflib **1.3.16**. Extra: `cdflib = ['cdflib>=1.3.16,<2']` in `pyproject.toml`.
- Exact same user API: same public names, parameter names/order/kinds/defaults, return Python types, array shapes and dtypes, exception types. Annotations and messages may differ.
- Right output, not identical output: files we write need not be byte-identical to cdflib's.
- Never import a cdflib name starting with `_`, never patch or rebind anything inside cdflib.
- Only `cdflib.dataclasses` and `cdflib.epochs_astropy` are reexported; everything else is ours.
- Where cdflib disagrees with the CDF spec / NASA's library, follow NASA and add a line to `docs/cdflib.rst` "Differences from cdflib". Otherwise match cdflib.
- Pure Python; no C++ changes. pycdfpp's own API stays unchanged.
- Behaviour reference: read cdflib 1.3.16's source for each function before writing ours (`pip download cdflib==1.3.16`, or the test venv's site-packages).
- Build/test: the project venv (numpy pyyaml ddt cyclopts rich **+ cdflib==1.3.16 xarray astropy**) first on `PATH`. One `ninja -C build` then one `meson test -C build <names>` at a time, in the foreground. Never two builds/tests on the same build dir at once, never backgrounded (applies to subagents too).
- Python tests run from the build dir with `PYTHONPATH=<build root>` (registered in `tests/meson.build`); never from the repo root.

## Review Focus

1. **Shape squeezing**: cdflib returns scalars / squeezed arrays for single-record and non-record-varying variables, and for single-value epoch calls. Expect identical shapes and Python types → covered by the corpus-wide comparison in Task 3 and the scalar cases in Task 2.
2. **Column-major and rVariable files** (`a_col_major_cdf.cdf`, `rvariable.cdf`): `varget` shapes and values must match cdflib → explicit cases in Task 3.
3. **Strings**: `col_major_strings.cdf`, `testutf8.cdf`, and `string_encoding="ascii"` vs `"utf-8"`: return type (`str` vs numpy `str_` array) must match → Task 3.
4. **Sparse / virtual records** (`sparse_records.cdf`): missing records read as cdflib returns them (pad or previous) → Task 3.
5. **Writer input types**: lists, Python scalars, `datetime`, `datetime64`, numpy arrays; `Data_Type` as int (e.g. `33`) or name (`"CDF_TIME_TT2000"`) → Task 5.

---

### Task 1: Package skeleton, build wiring and API-parity check

**Files:**
- Create: `pycdfpp/cdflib/__init__.py`, `pycdfpp/cdflib/dataclasses.py`, `pycdfpp/cdflib/epochs_astropy.py`, `pycdfpp/cdflib/meson.build`
- Modify: `pycdfpp/meson.build` (add `subdir('cdflib')`), `pyproject.toml` (extra), `tests/meson.build` (register tests), CI workflows that `pip install ddt` (`.github/workflows/tests-with-coverage.yml`, `tests-with-nasa-cdf-validation.yml`, `tests-windows.yml`, `tests-with-sanitizers.yml`, `CI.yml:134`): add `cdflib==1.3.16 xarray astropy`
- Test: `tests/python_cdflib_api/test.py`, `tests/python_cdflib_api/api_parity.py`

**Interfaces:**
- Produces: `api_parity.public_api(module) -> dict[str, inspect.Signature | type | object]` (every public name, recursing into public classes: methods, class constants, and instance attributes set in `__init__` found via `vars(instance)` on a reference instance where the test can build one); `api_parity.assert_same_api(testcase, ours, theirs)`. Later tasks add one `test_<module>` method each to `tests/python_cdflib_api/test.py`.
- Produces: `pycdfpp.cdflib.__version__ == "1.3.16"`; `pycdfpp.cdflib.dataclasses` is the real `cdflib.dataclasses` module object reexported (`from cdflib.dataclasses import *` plus `__all__`, or `sys.modules` alias — implementer's choice, `isinstance` must hold).
- `pycdfpp/cdflib/__init__.py` imports `cdflib` first; on `ImportError` raises `ImportError("pycdfpp.cdflib needs cdflib: pip install pycdfpp[cdflib]")`.

- [ ] **Step 1:** Write `api_parity.py` and tests `test_dataclasses_api` (same public names and dataclass fields as `cdflib.dataclasses`), `test_epochs_astropy_api`, `test_version` (`== "1.3.16"`), `test_missing_cdflib_message` (subprocess with a `sys.modules['cdflib'] = None` shim: importing `pycdfpp.cdflib` raises `ImportError` whose message contains `pip install pycdfpp[cdflib]`).
- [ ] **Step 2:** Register `python_cdflib_api` in `tests/meson.build`; `ninja -C build && meson test -C build python_cdflib_api` → FAIL (module missing).
- [ ] **Step 3:** Create the package, `meson.build` (same `fs.copyfile` + `install_sources(subdir: 'pycdfpp/cdflib')` pattern as `pycdfpp/meson.build`), the extra and CI deps.
- [ ] **Step 4:** `ninja -C build && meson test -C build python_cdflib_api` → PASS.
- [ ] **Step 5:** Commit `feat(cdflib): package skeleton and API parity check`.

### Task 2: `epochs.CDFepoch`

**Files:**
- Create: `pycdfpp/cdflib/epochs.py`
- Modify: `pycdfpp/cdflib/__init__.py` (`from .epochs import CDFepoch as cdfepoch`), `tests/python_cdflib_api/test.py`
- Test: `tests/python_cdflib_epochs/test.py`

**Interfaces:**
- Produces: class `CDFepoch` with every public method, staticmethod/classmethod kind, and class constant of cdflib's (`compute`, `compute_epoch`, `compute_epoch16`, `compute_tt2000`, `encode*`, `breakdown*`, `parse`, `to_datetime`, `unixtime`, `timestamp_to_cdfepoch`, `timestamp_to_cdfepoch16`, `timestamp_to_tt2000`, `findepochrange`, `epochrange_epoch`, `epochrange_epoch16`, `epochrange_tt2000`, plus public constants and module-level public names of `cdflib.epochs`).
- Type dispatch rule (same as cdflib): `int`/`int64` → TT2000, `float`/`float64` → EPOCH, `complex`/`complex128` → EPOCH16.
- Bulk conversions go through `pycdfpp.to_datetime64` / `to_tt2000` / `to_epoch` / `to_epoch16` / `to_time_string`; per-element Python loops only where pycdfpp has no vectorised path (`parse`, `breakdown` fields), using numpy arithmetic on `datetime64`.

- [ ] **Step 1:** `test_epochs_api` in the API test. In `python_cdflib_epochs`, ddt-driven tests comparing ours vs real cdflib for each method on: a single value, a list, a 1000-element array, for each of the 3 types; dates around leap seconds (2016-12-31T23:59:60) and 1970/2000 boundaries; `iso_8601` True/False; `parse` on every format `encode` emits. Assertions: same type, shape, dtype, `np.testing.assert_array_equal` (EPOCH16 picoseconds included).
- [ ] **Step 2:** Listed differences, asserted against our expected value (not cdflib's): `to_datetime` of CDF_EPOCH `63745056000000.123` keeps `.123` ms; `to_datetime` of a CDF_EPOCH array containing fill `-1e31` gives `NaT` (cdflib raises `OverflowError`).
- [ ] **Step 3:** Run → FAIL. Implement. Run → PASS. Add the two differences to `docs/cdflib.rst` (create it with a "Differences from cdflib" section).
- [ ] **Step 4:** Commit `feat(cdflib): CDFepoch on pycdfpp chrono`.

### Task 3: `cdfread.CDF` on local files

**Files:**
- Create: `pycdfpp/cdflib/cdfread.py`, `pycdfpp/cdflib/_types.py` (table: CDF type number ↔ name ↔ `pycdfpp.DataType` ↔ numpy dtype, as data)
- Modify: `pycdfpp/cdflib/__init__.py` (`CDF`, `cdfread`), API test
- Test: `tests/python_cdflib_read/test.py`

**Interfaces:**
- Consumes: `CDFepoch` (Task 2), `cdflib.dataclasses` types (Task 1).
- Produces: `cdfread.CDF(path, validate=False, string_encoding='ascii', s3_read_method=1)` with `cdf_info() -> CDFInfo`, `varinq(variable) -> VDRInfo`, `varget(variable=None, epoch=None, starttime=None, endtime=None, startrec=0, endrec=None)`, `varattsget(variable) -> dict`, `globalattsget() -> dict`, `attinq(attribute) -> ADRInfo`, `attget(attribute, entry=None) -> AttData`, `vdr_info(variable) -> VDR`, `__getitem__`, `__enter__/__exit__`, `__del__`, and cdflib's public instance attributes. File offsets in `VDR`/`ADRInfo`/`AEDR`/`GDRInfo` are `0`.
- `_types.py` exports `CDF_TYPES: tuple[CdfType, ...]` (`NamedTuple(number, name, data_type, dtype)`) and lookups `by_number`, `by_name`, `by_data_type`; Task 5 reuses it.
- Load with `pycdfpp.load(path, iso_8859_1_to_utf8=...)` lazily; `validate=True` compares the trailing MD5 with `hashlib.md5` of the file bytes, raising what cdflib raises on mismatch.

- [ ] **Step 1:** `test_cdfread_api`. Corpus test: for every `tests/resources/*.cdf` that real cdflib opens (skip the ones it rejects, listing them in the test), compare ours vs cdflib for `cdf_info()`, then for every variable `varinq`, `varget`, `varattsget`, `vdr_info` (non-offset fields), and for every attribute `attinq` (non-offset fields), `attget` for each entry, plus `globalattsget`. Equality: dataclass fields equal; arrays same dtype/shape/values (NaN-aware); same Python type for scalars and strings.
- [ ] **Step 2:** Targeted cases: `varget` with `startrec`/`endrec` (first, middle, last record, endrec beyond end); `epoch=` + `starttime`/`endtime`; unknown variable/attribute names and out-of-range numbers raise the same exception types as cdflib; `string_encoding="utf-8"` on `testutf8.cdf`; `validate=True` on `checksum.cdf` passes and on a copy with one flipped data byte raises cdflib's exception type; open via `str` and `pathlib.Path`; `with` block.
- [ ] **Step 3:** Run → FAIL. Implement. Run → PASS. Note in `docs/cdflib.rst`: file offsets are 0; anything else the corpus test showed where we follow NASA.
- [ ] **Step 4:** Commit `feat(cdflib): CDF reader`.

### Task 4: Remote reads (URL and S3)

**Files:**
- Create: `pycdfpp/cdflib/s3.py` (cdflib's public `s3` names), `pycdfpp/cdflib/utils.py`, `pycdfpp/cdflib/logging.py` (public names of cdflib's modules)
- Modify: `pycdfpp/cdflib/cdfread.py`, API test
- Test: `tests/python_cdflib_remote/test.py`

**Interfaces:**
- Consumes: `cdfread.CDF` (Task 3).
- `CDF(path)` with `path` starting `http://`/`https://` downloads with `urllib.request` into bytes; `s3://bucket/key` fetches with `boto3` (imported lazily; missing boto3 raises cdflib's exception type). All three `s3_read_method` values produce the same in-memory load.

- [ ] **Step 1:** `test_s3_utils_logging_api`. Remote test: serve `tests/resources/a_cdf.cdf` with `http.server` on a free port in a thread; `CDF(url).varget(...)` equals the local read. S3: inject a fake `boto3` module in `sys.modules` whose client returns the file bytes; for `s3_read_method` 1, 2, 3 the read equals the local one.
- [ ] **Step 2:** Run → FAIL. Implement. Run → PASS.
- [ ] **Step 3:** Commit `feat(cdflib): URL and S3 reads`.

### Task 5: `cdfwrite.CDF`

**Files:**
- Create: `pycdfpp/cdflib/cdfwrite.py`
- Modify: `pycdfpp/cdflib/__init__.py` (`cdfwrite`), API test
- Test: `tests/python_cdflib_write/test.py`

**Interfaces:**
- Consumes: `_types.py` (Task 3), `CDFepoch` (Task 2), real cdflib reader in tests only.
- Produces: `cdfwrite.CDF(path, cdf_spec=None, delete=False)` with `write_globalattrs(globalAttrs)`, `write_variableattrs(variableAttrs)`, `write_var(var_spec, var_attrs=None, var_data=None)`, `close()`, `__enter__/__exit__`, cdflib's public attributes, and module-level public names of `cdflib.cdfwrite`.
- Builds a `pycdfpp.CDF` in memory, `pycdfpp.save` on `close()`/normal `__exit__`; an exception leaving the `with` block saves nothing. Existing `path` without `delete=True` raises what cdflib raises. `cdf_spec` keys: `Majority`, `Encoding`, `Checksum`, `rDim_sizes`, `Compressed`, `LeapSecond`… (all keys cdflib reads). `var_spec` keys: `Variable`, `Data_Type`, `Num_Elements`, `Rec_Vary`, `Var_Type`, `Dim_Sizes`, `Dim_Vary`, `Sparse`, `Compress`, `Block_Factor`, `Pad`… (all keys cdflib reads). rVariables are written as zVariables.

- [ ] **Step 1:** `test_cdfwrite_api`. Round-trip corpus test: for every resource file real cdflib reads, rebuild it through **our** writer using only values from real cdflib's reader (`cdf_info`, `varinq` → `var_spec`, `varattsget`, `globalattsget`, `varget`), then read the result with real cdflib: every variable's values, type, shape, `Rec_Vary`, `Dim_Sizes`, `Sparse`, and every attribute equal the original (rVariable → zVariable allowed, listed).
- [ ] **Step 2:** Targeted cases: each `Data_Type` given as int and as name; data as list, Python scalar, numpy array; `datetime64` data into a `CDF_TIME_TT2000` variable reads back as the same instants (difference vs cdflib, listed); `Compress` 6 and `Compressed` file-level; `Checksum=True` produces a file `CDF(path, validate=True)` (real cdflib) accepts; `Encoding` big-endian; `Majority` column; non-record-varying variable; global attribute entry dict `{0: value, 3: [value, 'CDF_INT4']}`; `with` block raising → no file; existing path with `delete=False` raises cdflib's type.
- [ ] **Step 3:** If `cdfvalidate` is available (see `tests/nasa_cdf_validation/validate.py` for how it is located), run it on every file written in Step 1; skip otherwise.
- [ ] **Step 4:** Run → FAIL. Implement. Run → PASS. Add differences to `docs/cdflib.rst`.
- [ ] **Step 5:** Commit `feat(cdflib): CDF writer`.

### Task 6: `xarray.cdf_to_xarray`

**Files:**
- Create: `pycdfpp/cdflib/xarray/__init__.py`, `pycdfpp/cdflib/xarray/cdf_to_xarray.py`, `pycdfpp/cdflib/xarray/meson.build`
- Modify: `pycdfpp/cdflib/meson.build`, API test
- Test: `tests/python_cdflib_xarray/test.py`

**Interfaces:**
- Consumes: `cdfread.CDF`, `CDFepoch`.
- Produces: `cdf_to_xarray(filename, to_datetime=True, to_unixtime=False, fillval_to_nan=False) -> xarray.Dataset`, built on pycdfpp directly where that's simpler than going through our `cdfread.CDF`.

- [ ] **Step 1:** `test_xarray_api`. For every resource file real cdflib converts, and each of the option combinations (`to_datetime` T/F, `to_unixtime` T/F, `fillval_to_nan` T/F), `xarray.testing.assert_identical(ours, cdflib's)` after applying listed differences (EPOCH ms). Files cdflib fails on: ours must either succeed with an ISTP-correct dataset or raise the same type; list them.
- [ ] **Step 2:** Run → FAIL. Implement. Run → PASS.
- [ ] **Step 3:** Commit `feat(cdflib): cdf_to_xarray`.

### Task 7: `xarray.xarray_to_cdf`

**Files:**
- Create: `pycdfpp/cdflib/xarray/xarray_to_cdf.py`
- Modify: `pycdfpp/cdflib/xarray/__init__.py`, API test
- Test: `tests/python_cdflib_xarray_write/test.py`

**Interfaces:**
- Consumes: `cdfwrite.CDF` or pycdfpp directly, `CDFepoch`, `_types.py`.
- Produces: `xarray_to_cdf(xarray_dataset, file_name, unix_time_to_cdf_time=False, istp=True, terminate_on_warning=False, auto_fix_depends=True, record_dimensions=['record0'], compression=0, compression_skip_vars=None, nan_to_fillval=True) -> None`. Same ISTP checks as cdflib 1.3.16: same conditions produce a warning (same category) or an exception (same type), `terminate_on_warning` escalates the same way.

- [ ] **Step 1:** Round trip: for every resource file, `cdflib.xarray.cdf_to_xarray` (real) → our `xarray_to_cdf` → real `cdf_to_xarray` → `assert_identical` with the first dataset. Each option: `unix_time_to_cdf_time`, `compression=6` with `compression_skip_vars`, `nan_to_fillval`, `record_dimensions`, `auto_fix_depends`.
- [ ] **Step 2:** ISTP checks: build a table of invalid datasets, one per check in cdflib's source (missing `DEPEND_0`, epoch var not named Epoch-like, missing `VAR_TYPE`, bad `DISPLAY_TYPE`, …). For each, with `istp=True` and `terminate_on_warning` True/False: real cdflib's outcome (warning category / exception type / file written) equals ours. Drive it as data (`ddt` over the table).
- [ ] **Step 3:** Run → FAIL. Implement. Run → PASS. `cdfvalidate` on written files when available.
- [ ] **Step 4:** Commit `feat(cdflib): xarray_to_cdf`.

### Task 8: `install()`, docs, benchmark

**Files:**
- Modify: `pycdfpp/cdflib/__init__.py`, `docs/cdflib.rst`, `docs/index.rst` (toctree), `docs/history.rst` (unreleased entry)
- Create: `benchmarks/python_libs/cdflib_compat.py`
- Test: `tests/python_cdflib_install/test.py`

**Interfaces:**
- Produces: `pycdfpp.cdflib.install() -> None`: puts our modules into `sys.modules` under `cdflib`, `cdflib.cdfread`, `cdflib.cdfwrite`, `cdflib.epochs`, `cdflib.xarray`, `cdflib.xarray.cdf_to_xarray`, `cdflib.xarray.xarray_to_cdf`, `cdflib.s3`, `cdflib.utils`, `cdflib.logging`; `cdflib.dataclasses` and `cdflib.epochs_astropy` stay the real modules (imported before the swap). Idempotent. Docstring states it must run before other libraries import cdflib.

- [ ] **Step 1:** Tests, each in a fresh subprocess: after `install()`, `import cdflib; from cdflib.epochs import CDFepoch; from cdflib.xarray import cdf_to_xarray; import cdflib.dataclasses` give our objects (real dataclasses); a read through `cdflib.CDF` returns the same as before install; calling `install()` twice is harmless.
- [ ] **Step 2:** Run → FAIL. Implement. Run → PASS.
- [ ] **Step 3:** `docs/cdflib.rst`: how to switch (`import pycdfpp.cdflib as cdflib`, `install()`), the extra, the full "Differences from cdflib" list. Benchmark script: cdflib vs layer on load + every `varget`, bulk `to_datetime`, writing, `cdf_to_xarray`, on `benchmarks/compression/corpus.py` files; label CPU + instruction set on numbers. Run it once, put the table in `docs/cdflib.rst`.
- [ ] **Step 4:** Full suite: `ninja -C build && meson test -C build --suite pycdfpp` → read the real pass/fail count and exit code.
- [ ] **Step 5:** Commit `feat(cdflib): install() and docs`.
