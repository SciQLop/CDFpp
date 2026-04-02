=======
History
=======

0.9.1 (unreleased)
------------------

* Add ``to_time_string()`` for vectorized CDF time to string conversion with user-defined format (closes #70)

0.9.0 (2026-04-01)
------------------

* SIMD-optimized time conversions (AVX512/AVX2/SSE2 runtime dispatch for CDF_EPOCH, EPOCH16, TT2000)
* Improved Python exception messages with actionable context
* Reject unexpected keyword arguments in Python API functions
* Reject Variables and Attributes with empty names
* Fix epoch16 to_epoch16 catastrophic cancellation losing sub-second precision
* Fix leap_second() off-by-one error and pre-1972 TT2000 handling
* Fix RLE corruption for sequences of >256 zeros
* Fix nomap container: swap corruption, const_iterator UB, asymmetric equality
* Fix mmap MAP_FAILED not detected as invalid mapping, fd leak on failure
* Fix null-check for libdeflate compressor
* Fix record_count * record_size uint32_t overflow
* Fix missing std::move in Variable constructors and rvalue assignment operators
* Fix blk_iterator postfix operator++ calling step_forward(0)
* Fix CDF_UCHAR shape handling
* Fix cdf_map __getitem__ to raise KeyError instead of silently inserting
* Fix missing CPR record for empty compressed variables (#52)
* Fix ZSTD error checking and meson ZSTD build
* C++20 modernization: concepts, structured bindings, std::ranges, if constexpr, std::visit with overload pattern
* Apple Clang 15 compatibility fix

0.8.6 (2026-01-14)
------------------

* Switch to bump-my-version
* Auto-convert Latin-1 strings to UTF-8 on load
* Bump actions/download-artifact from 6 to 7
* Bump actions/upload-artifact from 5 to 6

0.8.5 (2025-12-09)
------------------

* Add ccache support for Linux builds
* Migrate to macos-15 for Intel runner, raise minimum macOS version to 13.0
* Ensure wraps are always built for MACOSX_DEPLOYMENT_TARGET
* Ignore existing packages on PyPI upload

0.8.4 (2025-12-05)
------------------

* Do not push Wasm wheels to PyPI yet

0.8.3 (2025-11-28)
------------------

* Experimental Pyodide/Wasm builds
* Configure Dependabot for GitHub Actions updates
* Bump actions/checkout v4→v6, codecov-action v4→v5, upload-artifact v4→v5, download-artifact v4→v6

0.8.2 (2025-10-08)
------------------

* CI: use the latest pyenv on macOS

0.8.1 (2025-10-07)
------------------

* Update CI and bump dependencies

0.8.0 (2025-07-28)
------------------

* Add support for filtering CDF content using static list, regex, or callable
* Add support for setting Attributes and Variables values from other Attributes and Variables
* Add support for cloning variables or attributes
* Add Python 3.14(t) build and tests (free-threaded Python)
* Update PyBind11
* Improve support for CDF special values
* Honor numpy dtype for attributes values that are List[np.intX or np.uintX]

0.7.7 (2025-05-08)
------------------

* Bump CI build wheel and use native Linux ARM runners

0.7.6 (2025-01-22)
------------------

* Drop ppc wheels (too slow to build on GitHub Actions)

0.7.5 (2025-01-21)
------------------

* Switch to MIT license
* Add new package architectures, CI cleanup
* Build Linux wheels in parallel
* Bump cibuildwheel, update wraps for Python 3.13

0.7.4 (2024-09-18)
------------------

* Fix wrong majority swap with string labels

0.7.3 (2024-06-22)
------------------

* Add numpy 2.0 support and Python 3.13
* Ensure majority swap is correct with strings

0.7.2 (2024-06-20)
------------------

* Fix majority wrong swap with data cubes
* Avoid numpy 2.0 until next release

0.7.1 (2024-06-05)
------------------

* Add experimental ZSTD compression algorithm support
* Ensure C++ side always gets C-contiguous arrays and avoids views
* Fixes #30 and adds ImHex rudimentary patterns

0.7.0 (2024-05-17)
------------------

* Add preliminary support for CDF file format versions prior to 2.5
* Add minimum supported CDF file format version
* Benchmark with CDAWeb masters
* Sanitizer fixes and specific handling across 2.x versions

0.6.4 (2024-04-17)
------------------

* CI fixes and build architecture configuration

0.6.3 (2024-03-11)
------------------

* macOS compatibility fixes

0.6.2 (2024-03-08)
------------------

* Release GIL as much as possible
* Switch to cibuildwheel
* Allow building without Python wrapper
* Apple Clang fixes
* Basic WASM proof of concept

0.6.1 (2023-12-05)
------------------

* Fix writing empty attributes strings (ensure numElements cdf fields > 0)
* set_values assume values=[] if only data_type is provided
* Attributes values reset, user can now change attributes values once set

0.6.0 (2023-10-18)
------------------

* Fixes + Unfinished skeletons export
* Adds Python 3.12 support.
* Builds with O3 optimizations instead of O2.
* Always expose record count as first dimension (even with NRV variables)

0.5.0 (2023-09-06)
------------------

* Add support for writing CDF files.
* Add support for lazy loading variables (default behavior now).
* Read performances improvements.
* Exposes string variables values as numpy array of unencoded strings by default (.values).
* Add support for encoding string variables values (.values_encoded).
* Exposes CDF version, compression, majority,...

0.4.6 (2023-06-22)
------------------

* Fixes Windows 'access violation' error.


0.4.5 (2023-06-21)
------------------

* Mostly CI refactoring.


0.4.4 (2023-06-19)
------------------

* Packaging fix.
