[![Gitpod Ready-to-Code](https://img.shields.io/badge/Gitpod-Ready--to--Code-blue?logo=gitpod)](https://gitpod.io/#https://github.com/SciQLop/CDFpp) 

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

- [x] read uncompressed file headers
- [x] read uncompressed attributes
- [x] read uncompressed variables
- [x] read variable attributes
- [x] loads cdf files from memory (std::vector<char> or char*)
- [ ] read variables with nested VXRs
- [x] read compressed file
- [x] read compressed variables
- [ ] write uncompressed headers
- [ ] write uncompressed attributes
- [ ] write uncompressed variables
- [ ] write compressed attributes
- [ ] write compressed file variables
- [ ] handle leap seconds
- [x] Python wrappers
- [ ] Documentation
- [ ] Examples
- [ ] Benchmarks

If you want to understand how it works, how to use the code or what works, you may have to read tests.

# Installing

## From PyPi

```bash
pip3 install --user pycdfpp
```

## From sources

```bash
meson build
cd build
ninja
sudo ninja install
```

# Basic usage

## Python

```python
from pycdfpp import pycdfpp
cdf = pycdfpp.load("some_cdf.cdf")
cdf_var_data = cdf["var_name"].as_array() #builds a numpy array
attribute_name_first_value = cdf.attributes['attribute_name'][0]
```

## C++
```cpp
#include "cdf-io/cdf-io.hpp"
#include <iostream>

std::ostream& operator<<(std::ostream& os, const cdf::Variable::shape_t& shape)
{
    os << "(";
    for (auto i = 0; i < static_cast<int>(std::size(shape)) - 1; i++)
        os << shape[i] << ',';
    if (std::size(shape) >= 1)
        os << shape[std::size(shape) - 1];
    os << ")";
    return os;
}

int main(int argc, char** argv)
{
    auto path = std::string(DATA_PATH) + "/a_cdf.cdf";
    // cdf::io::load returns a optional<CDF>
    if (const auto my_cdf = cdf::io::load(path); my_cdf)
    {
        std::cout << "Attribute list:" << std::endl;
        for (const auto& [name, attribute] : my_cdf->attributes)
        {
            std::cout << "\t" << name << std::endl;
        }
        std::cout << "Variable list:" << std::endl;
        for (const auto& [name, variable] : my_cdf->variables)
        {
            std::cout << "\t" << name << " shape:" << variable.shape() << std::endl;
        }
        return 0;
    }
    return -1;
}
```
