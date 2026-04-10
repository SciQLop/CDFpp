[![GitHub License](https://img.shields.io/github/license/SciQLop/CDFpp)](https://mit-license.org/)
[![Documentation Status](https://readthedocs.org/projects/pycdfpp/badge/?version=latest)](https://pycdfpp.readthedocs.io/en/latest/?badge=latest)
[![CPP20](https://img.shields.io/badge/Language-C++20-blue.svg)]()
[![PyPi](https://img.shields.io/pypi/v/pycdfpp.svg)](https://pypi.python.org/pypi/pycdfpp)
[![Coverage](https://codecov.io/gh/SciQLop/CDFpp/coverage.svg?branch=main)](https://codecov.io/gh/SciQLop/CDFpp/branch/main)
[![Discover on MyBinder](https://mybinder.org/badge_logo.svg)](https://mybinder.org/v2/gh/SciQLop/CDFpp/main?labpath=examples/notebooks)

# CDFpp (CDF++)

A modern, from-scratch C++20 implementation of NASA's [CDF](https://cdf.gsfc.nasa.gov/) (Common Data Format) with full Python bindings.

**Why another CDF library?** NASA's official C implementation has no multi-thread support (global shared state), an aging C89 interface, and a license incompatible with most Linux distribution policies. CDFpp solves all three: it is thread-safe, idiomatic C++20, and MIT-licensed.

### Highlights

- **Header-only C++20 library** — just add the include path, no linking required
- **Complete read/write support** — CDF versions 2.2 through 3.x, row and column major, compressed files and variables (GZip, RLE)
- **Python bindings (`pycdfpp`)** via pybind11 — zero-copy NumPy integration, GIL-free I/O
- **SIMD-accelerated time conversions** — AVX512/AVX2/SSE2 runtime dispatch for TT2000, EPOCH, EPOCH16
- **Lazy loading** — variable data is read on first access, not at file open
- **Fast** — up to ~4 GB/s read throughput; SIMD time conversions at up to 14 billion epochs/s

## Packages & CI

| | Linux x86_64 | Windows x86_64  | macOS x86_64  | macOS ARM64  |
| --- | --- | --- | --- | --- |
| **Wheels** | [![linux_x86_64][1]][2] | [![windows_x86_64][3]][4] | [![macos_x86_64][5]][6] | [![macos_arm64][7]][8] |
| **Tests** | [![linux_x86_64][9]][10] | [![windows_x86_64][11]][12] | [![macos_x86_64][13]][14] | |

[1]: https://github.com/SciQLop/CDFpp/actions/workflows/CI.yml/badge.svg?event=release
[2]: https://github.com/SciQLop/CDFpp/actions/workflows/CI.yml
[3]: https://github.com/SciQLop/CDFpp/actions/workflows/CI.yml/badge.svg?event=release
[4]: https://github.com/SciQLop/CDFpp/actions/workflows/CI.yml
[5]: https://github.com/SciQLop/CDFpp/actions/workflows/CI.yml/badge.svg?event=release
[6]: https://github.com/SciQLop/CDFpp/actions/workflows/CI.yml
[7]: https://github.com/SciQLop/CDFpp/actions/workflows/CI.yml/badge.svg?event=release
[8]: https://github.com/SciQLop/CDFpp/actions/workflows/CI.yml
[9]: https://github.com/SciQLop/CDFpp/actions/workflows/CI.yml/badge.svg?event=push
[10]: https://github.com/SciQLop/CDFpp/actions/workflows/CI.yml
[11]: https://github.com/SciQLop/CDFpp/actions/workflows/CI.yml/badge.svg?event=push
[12]: https://github.com/SciQLop/CDFpp/actions/workflows/CI.yml
[13]: https://github.com/SciQLop/CDFpp/actions/workflows/CI.yml/badge.svg?event=push
[14]: https://github.com/SciQLop/CDFpp/actions/workflows/CI.yml

---

## Installing

### From PyPI

```bash
pip install pycdfpp
```

### From source (C++ library)

```bash
meson setup build
ninja -C build
sudo ninja -C build install
```

### From source (Python wheel)

```bash
python -m build .
# wheel is in dist/
```

---

## Quick start (Python)

### Reading a CDF file

```python
import pycdfpp

cdf = pycdfpp.load("my_data.cdf")

# Variables expose numpy arrays (zero-copy when possible)
data = cdf["variable_name"].values

# Global attributes
print(cdf.attributes["Project"][0])

# Variable attributes
print(cdf["variable_name"].attributes["UNITS"].value)

# Iterate over all variables
for name, var in cdf.items():
    print(f"{name}: shape={var.shape}, type={var.type}")
```

### Loading from memory

```python
import pycdfpp

# Load from any bytes-like object (useful with HTTP responses, S3, etc.)
with open("my_data.cdf", "rb") as f:
    cdf = pycdfpp.load(f.read())
```

### Time conversions

CDFpp handles all three CDF time types (EPOCH, EPOCH16, TT2000) and converts them to numpy `datetime64[ns]` or Python `datetime`:

```python
import pycdfpp
import numpy as np

cdf = pycdfpp.load("my_data.cdf")

# Convert any CDF time variable to numpy datetime64 (fast, ~2ns/element)
times = pycdfpp.to_datetime64(cdf["Epoch"])

# Or to Python datetime objects
times_dt = pycdfpp.to_datetime(cdf["Epoch"])

# Format as strings (e.g. for PDS4 compliance)
time_strings = pycdfpp.to_time_string(cdf["Epoch"], "%Y-%m-%dT%H:%M:%SZ")
# array([b'2020-02-01T00:00:00.000000000Z', ...])

# Convert Python/numpy times to CDF types
tt2000_values = pycdfpp.to_tt2000(np.array(['2020-01-01', '2020-06-15'], dtype='datetime64[ns]'))
epoch_values = pycdfpp.to_epoch(np.array(['2020-01-01', '2020-06-15'], dtype='datetime64[ns]'))
```

### Creating and writing CDF files

```python
import pycdfpp
import numpy as np
from datetime import datetime

cdf = pycdfpp.CDF()

# Add global attributes (each entry is a list of values)
cdf.add_attribute("Project", ["MyMission"])
cdf.add_attribute("PI_name", ["Jane Doe"])

# Add a time variable with TT2000 encoding
times = np.arange('2020-01-01', '2020-01-02', dtype='datetime64[h]').astype('datetime64[ns]')
cdf.add_variable("Epoch", values=times, data_type=pycdfpp.DataType.CDF_TIME_TT2000)

# Add a data variable with variable attributes
cdf.add_variable("B_GSM",
    values=np.random.randn(24, 3).astype(np.float32),
    attributes={
        "FIELDNAM": ["Magnetic Field"],
        "UNITS": ["nT"],
        "DEPEND_0": ["Epoch"],
    })

# Save to disk
pycdfpp.save(cdf, "output.cdf")

# Or save to memory (returns bytes)
data = pycdfpp.save(cdf)
```

### Compressed CDF files

```python
import pycdfpp
import numpy as np

cdf = pycdfpp.CDF()
cdf.add_variable("data", values=np.zeros(10000, dtype=np.float64))

# Whole-file GZip compression
cdf.compression = pycdfpp.CompressionType.gzip_compression
pycdfpp.save(cdf, "compressed.cdf")

# Or per-variable compression
cdf2 = pycdfpp.CDF()
cdf2.add_variable("data",
    values=np.zeros(10000, dtype=np.float64),
    compression=pycdfpp.CompressionType.gzip_compression)
```

### Filtering variables

```python
import pycdfpp

cdf = pycdfpp.load("large_file.cdf")

# Keep only specific variables and attributes (returns a new CDF)
filtered = cdf.filter(variables=["Epoch", "B_GSM"], attributes=["Project"])

# Filter with a regex pattern
filtered = cdf.filter(variables="B_.*", attributes=".*")

# Filter with a callable
filtered = cdf.filter(variables=lambda var: var.name.startswith("B_"))
```

### Cloning variables between CDF files

```python
import pycdfpp

src = pycdfpp.load("source.cdf")
dst = pycdfpp.CDF()

# Clone a variable (deep copy, including its attributes)
dst.add_variable(src["Epoch"])
dst.add_variable(src["B_GSM"])
```

### NumPy buffer protocol

Variables implement the Python buffer protocol, so they work directly with NumPy and any library that accepts array-like objects:

```python
import pycdfpp
import numpy as np

cdf = pycdfpp.load("my_data.cdf")

# Direct numpy array construction (zero-copy for numeric types)
arr = np.array(cdf["B_GSM"])
```

---

## Quick start (C++)

### Reading

```cpp
#include "cdfpp/cdf-io/cdf-io.hpp"
#include <iostream>

int main()
{
    // cdf::io::load returns std::optional<CDF>
    if (auto cdf = cdf::io::load("my_data.cdf"))
    {
        for (const auto& [name, variable] : cdf->variables)
            std::cout << name << " shape: " << variable.shape() << "\n";

        for (const auto& [name, attribute] : cdf->attributes)
            std::cout << name << "\n";
    }
}
```

### Writing

```cpp
#include "cdfpp/cdf-io/cdf-io.hpp"

int main()
{
    cdf::CDF my_cdf;

    // Save to file (returns bool)
    cdf::io::save(my_cdf, "output.cdf");

    // Or save to memory (returns a vector<char>)
    auto data = cdf::io::save(my_cdf);
}
```

### Loading from memory

```cpp
#include "cdfpp/cdf-io/cdf-io.hpp"
#include <vector>

void process(const std::vector<char>& buffer)
{
    if (auto cdf = cdf::io::load(buffer.data(), buffer.size()))
    {
        // ...
    }
}
```

---

## Benchmarks

All benchmarks measured on a 16-core machine (5.1 GHz boost, 16 MB L3), release build (`-O3`). Source code in [`benchmarks/`](benchmarks/).

### SIMD time conversions

Converting CDF time types to nanoseconds since 1970 (epochs/s, higher is better):

| Conversion | 64 | 1K | 64K | 1M | 64M |
|---|---|---|---|---|---|
| TT2000 scalar | 7.8e+08 | 8.7e+08 | 8.8e+08 | 8.6e+08 | 5.9e+08 |
| TT2000 SIMD | 2.5e+09 | **8.1e+09** | 5.3e+09 | 3.4e+09 | 1.5e+09 |
| EPOCH scalar | 2.2e+09 | 2.3e+09 | 2.2e+09 | 2.1e+09 | 1.1e+09 |
| EPOCH SIMD | 9.6e+09 | **1.4e+10** | 6.7e+09 | 3.8e+09 | 1.5e+09 |

SIMD vectorized TT2000 conversion peaks at ~**8 billion epochs/s** for L1/L2-resident data. EPOCH conversion (simpler, no leap-second table) peaks at ~**14 billion epochs/s**.

### Leap-second lookup

| Method | 1K | 64K | 1M | 64M |
|---|---|---|---|---|
| Branchless | 9.1e+07 | 9.4e+07 | 9.5e+07 | 1.0e+08 |
| Baseline | 2.4e+08 | 2.4e+08 | 2.4e+08 | 2.4e+08 |

### RLE compression (bytes/s)

| Operation | 1 KB | 16 KB | 64 KB | 1 MB |
|---|---|---|---|---|
| Deflate | 7.4e+08 | 6.9e+08 | 3.3e+08 | 2.7e+08 |
| Inflate | **1.8e+09** | **1.7e+09** | **1.8e+09** | 4.5e+08 |
| Roundtrip | 5.1e+08 | 5.0e+08 | 1.9e+08 | 1.7e+08 |

RLE inflate sustains ~**1.8 GB/s** for data that fits in cache.

---

## Features & roadmap

- **Reading**
    - [x] CDF versions 2.2 through 3.x
    - [x] Compressed files and variables (GZip, RLE)
    - [x] Row and column major
    - [x] Nested VXRs
    - [x] Lazy variable loading
    - [x] UTF-8 and ISO 8859-1 (Latin-1, auto-converted to UTF-8)
    - [x] In-memory loading (`std::vector<char>`, `char*`, Python `bytes`)
    - [ ] DEC floating-point encoding (VAX, Alpha, Itanium)
    - [ ] Pad values
- **Writing**
    - [x] Uncompressed and compressed files/variables
    - [x] All numeric types, strings, datetime types
    - [ ] Pad values
- **General**
    - [x] [libdeflate](https://github.com/ebiggers/libdeflate) for faster GZip
    - [x] SIMD time conversions (AVX512/AVX2/SSE2 with runtime dispatch)
    - [x] Leap-second handling
    - [x] Python bindings with GIL-free I/O
    - [x] [Documentation](https://pycdfpp.readthedocs.io/en/latest/)

---

## Caveats

- **NRV variables shape**: PyCDFpp exposes the record count as the first dimension, so NRV variables will have shape `(0, ...)` or `(1, ...)`.
- **Reference invalidation**: Python wrappers hold references into C++ containers. Adding or removing variables/attributes can invalidate them. Always re-fetch after mutation:
    ```python
    # UNSAFE - ref may dangle after add_variable
    var = cdf["B_GSM"]
    cdf.add_variable("new_var", values=np.zeros(10))
    var.values  # potential segfault

    # SAFE - re-fetch
    cdf.add_variable("new_var", values=np.zeros(10))
    var = cdf["B_GSM"]
    ```

See the [full documentation](https://pycdfpp.readthedocs.io/en/latest/) for more details.
