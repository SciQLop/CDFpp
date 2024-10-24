[![GitHub License](https://img.shields.io/github/license/SciQLop/CDFpp)](https://mit-license.org/)
[![Documentation Status](https://readthedocs.org/projects/pycdfpp/badge/?version=latest)](https://pycdfpp.readthedocs.io/en/latest/?badge=latest)
[![CPP17](https://img.shields.io/badge/Language-C++17-blue.svg)]()
[![PyPi](https://img.shields.io/pypi/v/pycdfpp.svg)](https://pypi.python.org/pypi/pycdfpp)
[![Coverage](https://codecov.io/gh/SciQLop/CDFpp/coverage.svg?branch=main)](https://codecov.io/gh/SciQLop/CDFpp/branch/main)
[![Discover on MyBinder](https://mybinder.org/badge_logo.svg)](https://mybinder.org/v2/gh/SciQLop/CDFpp/main?labpath=examples/notebooks)

# Python packages

| Linux x86_64 | Windows x86_64  | MacOs x86_64  | MacOs ARM64  |
| --- | --- | --- | --- |
| [![linux_x86_64][1]][2] | [![windows_x86_64][3]][4] | [![macos_x86_64][5]][6] | [![macos_arm64][7]][8] |

[1]: https://github.com/SciQLop/CDFpp/actions/workflows/CI.yml/badge.svg?event=release
[2]: https://github.com/SciQLop/CDFpp/actions/workflows/CI.yml
[3]: https://github.com/SciQLop/CDFpp/actions/workflows/CI.yml/badge.svg?event=release
[4]: https://github.com/SciQLop/CDFpp/actions/workflows/CI.yml
[5]: https://github.com/SciQLop/CDFpp/actions/workflows/CI.yml/badge.svg?event=release
[6]: https://github.com/SciQLop/CDFpp/actions/workflows/CI.yml
[7]: https://github.com/SciQLop/CDFpp/actions/workflows/CI.yml/badge.svg?event=release
[8]: https://github.com/SciQLop/CDFpp/actions/workflows/CI.yml


# Unit Tests

| Linux x86_64  | Windows x86_64 | MacOs x86_64  |
| --- | --- | --- |
| [![linux_x86_64][9]][10] | [![windows_x86_64][11]][12] | [![macos_x86_64][13]][14] |

[9]: https://github.com/SciQLop/CDFpp/actions/workflows/CI.yml/badge.svg?event=push
[10]: https://github.com/SciQLop/CDFpp/actions/workflows/CI.yml
[11]: https://github.com/SciQLop/CDFpp/actions/workflows/CI.yml/badge.svg?event=push
[12]: https://github.com/SciQLop/CDFpp/actions/workflows/CI.yml
[13]: https://github.com/SciQLop/CDFpp/actions/workflows/CI.yml/badge.svg?event=push
[14]: https://github.com/SciQLop/CDFpp/actions/workflows/CI.yml


# CDFpp (CDF++)
A NASA's [CDF](https://cdf.gsfc.nasa.gov/) modern C++ library. 
This is not a C++ wrapper but a full C++ implementation.
Why? CDF files are still used for space physics missions but few implementations are available.
The main one is NASA's C implementation available [here](https://cdf.gsfc.nasa.gov/) but it lacks multi-threads support (global shared state), has an old C interface and has a license which isn't compatible with most Linux distributions policy.
There are also Java and Python implementations which are not usable in C++.

List of features and roadmap:

- CDF reading 
    - [x] read files from cdf version 2.2 to 3.x
    - [x] read uncompressed file headers
    - [x] read uncompressed attributes
    - [x] read uncompressed variables
    - [x] read variable attributes
    - [x] loads cdf files from memory (std::vector<char> or char*)
    - [x] handles both row and column major files
    - [x] read variables with nested VXRs
    - [x] read compressed files (GZip, RLE)
    - [x] read compressed variables (GZip, RLE)
    - [x] read UTF-8 encoded files
    - [x] read ISO 8859-1(Latin-1) encoded files (converts to UTF-8 on the fly)
    - [x] variables values lazy loading
    - [ ] decode DEC's floating point encoding (Itanium, ALPHA and VAX)
    - [ ] pad values 
- CDF writing
    - [x] write uncompressed headers
    - [x] write uncompressed attributes
    - [x] write uncompressed variables
    - [x] write compressed variables
    - [x] write compressed files
    - [ ] pad values
- General features
    - [x] uses [libdeflate](https://github.com/ebiggers/libdeflate) for faster GZip decompression
    - [x] highly optimized CDF reads (up to ~4GB/s read speed from disk)
    - [x] handle leap seconds
    - [x] Python wrappers
    - [x] Documentation
    - [x] Examples (see below)
    - [x] [Benchmarks](https://github.com/SciQLop/CDFpp/tree/main/notebooks/benchmarks.ipynb)

If you want to understand how it works, how to use the code or what works, you may have to read tests.

# Installing

## From PyPi

```bash
python3 -m pip install --user pycdfpp
```

## From sources

```bash
meson build
cd build
ninja
sudo ninja install
```

Or if youl want to build a Python wheel:

```bash
python -m build . 
# resulting wheel will be located into dist folder
```

# Basic usage

## Python

### Reading CDF files
Basic example from a local file:

```python
import pycdfpp
cdf = pycdfpp.load("some_cdf.cdf")
cdf_var_data = cdf["var_name"].values #builds a numpy view or a list of strings
attribute_name_first_value = cdf.attributes['attribute_name'][0]
```

Note that you can also load in memory files:

```python
import pycdfpp
import requests
import matplotlib.pyplot as plt
tha_l2_fgm = pycdfpp.load(requests.get("https://spdf.gsfc.nasa.gov/pub/data/themis/tha/l2/fgm/2016/tha_l2_fgm_20160101_v01.cdf").content)
plt.plot(tha_l2_fgm["tha_fgl_gsm"])
plt.show()
```

Buffer protocol support:

```python
import pycdfpp
import requests
import xarray as xr
import matplotlib.pyplot as plt

tha_l2_fgm = pycdfpp.load(requests.get("https://spdf.gsfc.nasa.gov/pub/data/themis/tha/l2/fgm/2016/tha_l2_fgm_20160101_v01.cdf").content)
xr.DataArray(tha_l2_fgm['tha_fgl_gsm'], dims=['time', 'components'], coords={'time':tha_l2_fgm['tha_fgl_time'].values, 'components':['x', 'y', 'z']}).plot.line(x='time')
plt.show()

# Works with matplotlib directly too

plt.plot(tha_l2_fgm['tha_fgl_time'], tha_l2_fgm['tha_fgl_gsm'])
plt.show()

```

Datetimes handling:

```python
import pycdfpp
import os
# Due to an issue with pybind11 you have to force your timezone to UTC for 
# datetime conversion (not necessary for numpy datetime64)
os.environ['TZ']='UTC'

mms2_fgm_srvy = pycdfpp.load("mms2_fgm_srvy_l2_20200201_v5.230.0.cdf")

# to convert any CDF variable holding any time type to python datetime:
epoch_dt = pycdfpp.to_datetime(mms2_fgm_srvy["Epoch"])

# same with numpy datetime64:
epoch_dt64 = pycdfpp.to_datetime64(mms2_fgm_srvy["Epoch"])

# note that using datetime64 is ~100x faster than datetime (~2ns/element on an average laptop)

```

### Writing CDF files

Creating a basic CDF file:

```python
import pycdfpp
import numpy as np
from datetime import datetime

cdf = pycdfpp.CDF()
cdf.add_attribute("some attribute", [[1,2,3], [datetime(2018,1,1), datetime(2018,1,2)], "hello\nworld"])
cdf.add_variable(f"some variable", values=np.ones((10),dtype=np.float64))
pycdfpp.save(cdf, "some_cdf.cdf")

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

## caveats
- NRV variables shape, in order to expose a consistent shape, PyCDFpp exposes the reccord count as first dimension and thus its value will be either 0 or 1 (0 mean empty variable).
