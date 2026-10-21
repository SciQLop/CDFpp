# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

CDFpp is a C++20 implementation of NASA's CDF (Common Data Format) from scratch (not a wrapper). It provides multi-threaded support, a modern C++ API, and Python bindings (`pycdfpp`) via pybind11.

## Build Commands

```bash
# Configure and build (C++ only)
meson setup build
ninja -C build

# With tests enabled
meson setup -Dwith_tests=true build
ninja -C build

# Run all tests
ninja test -C build

# Run a single C++ test (Catch2)
./build/tests/simple_open/test-simple_open

# Run a single Python test
python3 tests/python_loading/test.py

# Build Python wheel
python -m build .

# Benchmarks
meson setup -Dwith_benchmarks=true build
ninja benchmark -C build
```

Key meson options: `-Duse_libdeflate=true` (default, faster gzip), `-Dwith_experimental_zstd=true`, `-Ddisable_python_wrapper=true`.

## Architecture

**Header-only C++ library** in `include/cdfpp/`:
- `cdf.hpp` — Root container: holds `variables` and `attributes` maps, file majority, compression type
- `variable.hpp` — Scientific data variable with shape, type, and lazy-loadable values
- `attribute.hpp` — Metadata key-value storage (vector of variant `data_t` values)
- `cdf-io/` — All I/O: `loading/` parses CDF binary format, `saving/` serializes it. Entry points: `cdf::io::load()`, `cdf::io::save()`
- `cdf-io/compression.hpp` / `decompression.hpp` — GZip (zlib/libdeflate), RLE, experimental Zstd
- `chrono/` — SIMD-optimized time conversions (CDF_EPOCH, EPOCH16, TT2000) with leap second handling

**Python bindings** in `pycdfpp/`:
- `pycdfpp.cpp` — pybind11 module definition (`_pycdfpp`)
- Per-type wrappers: `variable.hpp`, `attribute.hpp`, `cdf.hpp`, `chrono.hpp`
- GIL-free operations (`py::mod_gil_not_used()`)

**SIMD** in `simd/` and `src/arch/x86/`: runtime dispatch via xsimd for AVX512/AVX2/SSE2 chrono conversions.

**Custom containers**: `cdf-map.hpp` / `nomap.hpp` — ordered map used for variables and attributes (controlled by `-Duse_nomap`).

## Tests

- **C++**: Catch2 v3, one directory per test under `tests/` (e.g. `simple_open/`, `chrono/`, `records_loading/`)
- **Python**: unittest + ddt, under `tests/python_*/`
- **Test data**: `tests/resources/` with real CDF files; `tests/resources/make_cdf.py` generates additional test data

## Code Style

- C++20 standard
- `.clang-format`: WebKit-based, 100 column limit, 4-space indent, Allman braces
- Variables/attributes must have non-empty names (enforced at creation)

## Known Limitations

**Python bindings: removed entries dangle.** Python wrappers returned by `__getitem__` hold raw pointers into the C++ maps. `nomap` keeps each entry in its own allocation, so adding or removing *other* entries is safe. But a wrapper of an entry that is itself removed (`_remove_variable`, `_remove_attribute`) dangles: do not use it afterwards.

## Dependencies (as meson subprojects)

pybind11, fmt, Catch2, xsimd, libdeflate, hedley. Python ≥3.9, numpy, pyyaml at runtime.
