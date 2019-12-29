[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
[![CPP17](https://img.shields.io/badge/Language-C++17-blue.svg)]()
[![CI](https://github.com/SciQLop/CDFpp/workflows/C//C++%20CI/badge.svg)](https://github.com/SciQLop/CDFpp/actions?query=workflow%3A%22C%2FC%2B%2B+CI%22)
[![Coverage](https://codecov.io/gh/SciQLop/CDFpp/coverage.svg?branch=master)](https://codecov.io/gh/SciQLop/CDFpp/branch/master)

# CDFpp (CDF++)
A NASA's [CDF](https://cdf.gsfc.nasa.gov/) modern C++ library. 
This is not a C++ wrapper but a full C++ implementation.
Why? CDF files are still used for space physics missions but few implementations are available.
The main one is NASA's C implementation available [here](https://cdf.gsfc.nasa.gov/) but it lacks multi-threads support, has an old C interface and has a license which isn't compatible with most Linux distributions policy.
There are also Java and Python implementations which are not usable in C++.

List of features and roadmap:

- [x] read uncompressed file headers (v2x or v3x)
- [x] read uncompressed attributes (v2x or v3x)
- [ ] read uncompressed variables (v2x or v3x)
- [ ] read compressed file (v2x or v3x)
- [ ] read compressed variables (v2x or v3x)
- [ ] write uncompressed headers (v2x or v3x)
- [ ] write uncompressed attributes (v2x or v3x)
- [ ] write uncompressed variables (v2x or v3x)
- [ ] write compressed attributes (v2x or v3x)
- [ ] write compressed file variables (v2x or v3x)
- [ ] handle leap seconds
- [ ] Python wrappers
- [ ] Documentation
- [ ] Examples
- [ ] Benchmarks

If you want to understand how it works, how to use the code or what works, you may have to read tests.
