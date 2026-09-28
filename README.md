[![GitHub License](https://img.shields.io/github/license/SciQLop/CDFpp)](https://mit-license.org/)
[![Documentation Status](https://readthedocs.org/projects/pycdfpp/badge/?version=latest)](https://pycdfpp.readthedocs.io/en/latest/?badge=latest)
[![CPP20](https://img.shields.io/badge/Language-C++20-blue.svg)]()
[![PyPi](https://img.shields.io/pypi/v/pycdfpp.svg)](https://pypi.python.org/pypi/pycdfpp)
[![Coverage](https://codecov.io/gh/SciQLop/CDFpp/coverage.svg?branch=main)](https://codecov.io/gh/SciQLop/CDFpp/branch/main)
[![Discover on MyBinder](https://mybinder.org/badge_logo.svg)](https://mybinder.org/v2/gh/SciQLop/CDFpp/main?labpath=examples/notebooks)
[![Try CDFpp Explorer](https://img.shields.io/badge/Try_it-CDFpp_Explorer-6c8aff?logo=webassembly&logoColor=white)](https://sciqlop.github.io/CDFpp/)

# CDFpp (CDF++)

A modern, from-scratch C++20 implementation of NASA's [CDF](https://cdf.gsfc.nasa.gov/) (Common Data Format) with full Python bindings.

📖 **[Documentation](https://pycdfpp.readthedocs.io/en/latest/)**: quickstart, guides for reading and for producing ISTP-compliant files, cookbook, and C++ guide. The examples run live in your browser.

**Why another CDF library?** NASA's official C implementation has no multi-thread support (global shared state), an aging C89 interface, and a license incompatible with most Linux distribution policies. CDFpp solves all three: it is thread-safe, idiomatic C++20, and MIT-licensed.

### Highlights

- **C++20 library, mostly headers** — see the [C++ guide](https://pycdfpp.readthedocs.io/en/latest/cpp.html) for the compiler flags
- **Complete read/write support** — CDF versions 2.2 through 3.x, row and column major, compressed files and variables (GZip, RLE)
- **Python bindings (`pycdfpp`)** via pybind11 — zero-copy NumPy integration, GIL-free I/O
- **SIMD-accelerated time conversions** — AVX512/AVX2/SSE2 runtime dispatch for TT2000, EPOCH, EPOCH16
- **Lazy loading** — variable data is read on first access, not at file open
- **Fast** — 6× to several hundred times faster than spacepy and cdflib at reading, 3.5× to 11× at writing compressed files ([see below](#compared-with-spacepy-and-cdflib)); SIMD time conversions at up to 8 billion epochs/s (TT2000, AVX-512)
- **Runs everywhere** — Linux, Windows, macOS (x86_64 + ARM64), and WebAssembly (Pyodide / emscripten-forge)
- **In-browser app** — [**CDFpp Explorer**](https://sciqlop.github.io/CDFpp/) inspects, plots, and ISTP-validates CDF files entirely client-side, no install

## Packages & CI

| | Linux x86_64 | Linux aarch64 | Windows x86_64 | macOS x86_64 | macOS ARM64 | WASM (Pyodide) |
| --- | --- | --- | --- | --- | --- | --- |
| **Wheels** | [![][1]][2] | [![][1]][2] | [![][1]][2] | [![][1]][2] | [![][1]][2] | [![][1]][2] |
| **Tests**  | [![][3]][2] | [![][3]][2] | [![][3]][2] | [![][3]][2] | [![][3]][2] | |

[1]: https://github.com/SciQLop/CDFpp/actions/workflows/CI.yml/badge.svg?event=release
[2]: https://github.com/SciQLop/CDFpp/actions/workflows/CI.yml
[3]: https://github.com/SciQLop/CDFpp/actions/workflows/CI.yml/badge.svg?event=push

Also available on [emscripten-forge](https://github.com/emscripten-forge/recipes/tree/main/recipes/recipes_emscripten/pycdfpp) for use in JupyterLite and other Emscripten-based environments.

---

## CDFpp Explorer — CDF files in your browser

CDFpp compiles to **WebAssembly**, so the whole library runs client-side — no install, no server, and your files never leave your machine. [**CDFpp Explorer**](https://sciqlop.github.io/CDFpp/) is a small web app built on it:

- **Inspect** — browse variables and attributes grouped by ISTP `VAR_TYPE`, with live search and a value preview
- **Plot** — line plots and spectrograms, ISTP-aware (`DEPEND_0` time axis, `DISPLAY_TYPE`, `SCALETYP`, fill/valid masking), with CSV / JSON export
- **Validate** — hand the file to [AstraLint](https://sciqlop.github.io/AstraLint/) for ISTP conformance checking

👉 **[Open CDFpp Explorer](https://sciqlop.github.io/CDFpp/)**

The WebAssembly wrapper lives in [`wacdfpp/`](wacdfpp/) and ships TypeScript declarations (`wacdfpp/cdfpp.d.ts`) for embedding CDF read/write — with zero-copy typed arrays — in your own JavaScript app.

---

## Installing

### From PyPI

```bash
pip install pycdfpp
```

### C++ library

See [Adding CDFpp to your project](https://pycdfpp.readthedocs.io/en/latest/cpp.html#adding-cdfpp-to-your-project) in the C++ guide.

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
        "FIELDNAM": "Magnetic Field",
        "UNITS": "nT",
        "DEPEND_0": "Epoch",
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

### Compared with spacepy and cdflib

Everyday tasks on real CDAWeb files, with each library used the way its documentation shows, including its fastest time conversion. All three return the same values.

| Task | Data | pycdfpp | spacepy.pycdf | cdflib |
|---|---|---|---|---|
| Open a file, list variables, read all attributes | MMS FPI electron distribution, 178 MB | **0.7 ms** | 264 ms (391×) | 10.1 ms (15×) |
| Read B and its time axis as `datetime64` | MMS FGM survey, 1.2 M points, gzip, TT2000 | **8.7 ms** | 3.83 s (439×) | 308 ms (35×) |
| Read B and its time axis as `datetime64` | Wind MFI, 0.9 M points, CDF_EPOCH | **4.0 ms** | 40.4 ms (10×) | 13.3 s (3318×) |
| Read every variable of a file | MMS FPI electron distribution, 178 MB, gzip | **111 ms** | 958 ms (8.7×) | 627 ms (5.7×) |
| Read every variable of a folder | 23 CDAWeb files, 11 missions, 528 MB | **392 ms** | 2.36 s (6.0×) | 2.86 s (7.3×) |
| Same folder, 8 threads | 23 CDAWeb files, 11 missions, 528 MB | **166 ms** | not thread-safe | 2.46 s (15×) |
| Write B and its time axis, gzip | MMS FGM survey, 1.2 M points, 29 MB | **49.4 ms** | 461 ms (9.3×) | 173 ms (3.5×) |
| Write B and its time axis, uncompressed | MMS FGM survey, 1.2 M points, 29 MB | 15.4 ms | **12.5 ms (0.8×)** | 17.0 ms (1.1×) |
| Write a particle distribution file, gzip | MMS FPI electron distribution, 210 MB | **357 ms** | 3.90 s (11×) | 1.89 s (5.3×) |

(N×) = N times longer than pycdfpp. Median of 5 runs, files in the page cache. AMD Ryzen 7 5800X (AVX2, no AVX-512), Python 3.13, pycdfpp main (after 0.14.0), spacepy 0.7.0 (NASA CDF 3.9.0), cdflib 1.3.14.

Why is pycdfpp faster?

- **Opening a file** only parses the headers, in C++. NASA's library, used by spacepy, hashes the whole file to check its MD5 checksum on every open. pycdfpp skips that check.
- **Time conversion** runs in C++ with SIMD, straight to `datetime64[ns]`. For TT2000, spacepy creates one Python `datetime` per value. cdflib converts CDF_EPOCH in a Python loop.
- **Gzip** blocks are decompressed on all cores at once (an MMS FPI distribution variable has 640 of them), with [libdeflate](https://github.com/ebiggers/libdeflate), itself 1.5–1.7× faster than zlib on these files. Big buffers use 2 MB huge pages.
- **Threads** work: pycdfpp releases the GIL while reading and decompressing. cdflib is mostly Python, so it holds the GIL. NASA's library keeps global state.
- **Writing** compresses 256 KB blocks on all cores, with libdeflate. The other two compress with zlib, on one thread. Without compression there is little to gain: pycdfpp copies the array into the variable first, spacepy writes straight from it. Pass `copy=False` to `add_variable` to skip that copy.

Reading scales to about 2× with threads, and to 3.5 GB/s on big files; writing reaches 680 MB/s with gzip, where spacepy stays at 43 MB/s. See the [scaling results](https://pycdfpp.readthedocs.io/en/latest/performance.html#scaling).

Details and caveats in the [performance page](https://pycdfpp.readthedocs.io/en/latest/performance.html). Reproduce with [`benchmarks/python_libs/compare.py`](benchmarks/python_libs/compare.py).

### C++ micro-benchmarks

Release builds (`-O3`). Source code in [`benchmarks/`](benchmarks/).

#### SIMD time conversions

Converting CDF time types to nanoseconds since 1970 (epochs/s, higher is better, one thread). CDFpp picks the best instruction set the CPU has at run time (AVX-512, AVX2 or SSE2).

AMD Ryzen 7 7840U/HS laptop CPU (Zen 4, 5.1 GHz boost, 16 MB L3), **AVX-512**, measured with pycdfpp 0.13, before CDF_EPOCH conversions became exact (see below):

| Conversion | 64 | 1K | 64K | 1M | 64M |
|---|---|---|---|---|---|
| TT2000 scalar | 7.8e+08 | 8.7e+08 | 8.8e+08 | 8.6e+08 | 5.9e+08 |
| TT2000 SIMD | 2.5e+09 | **8.1e+09** | 5.3e+09 | 3.4e+09 | 1.5e+09 |
| EPOCH scalar | 2.2e+09 | 2.3e+09 | 2.2e+09 | 2.1e+09 | 1.1e+09 |
| EPOCH SIMD | 9.6e+09 | **1.4e+10** | 6.7e+09 | 3.8e+09 | 1.5e+09 |

AMD Ryzen 7 5800X desktop CPU (Zen 3, 32 MB L3), **AVX2** (median of 3 runs):

| Conversion | 64 | 1K | 64K | 1M | 64M |
|---|---|---|---|---|---|
| TT2000 scalar | 8.4e+08 | 9.7e+08 | 1.0e+09 | 1.0e+09 | 9.2e+08 |
| TT2000 SIMD | 2.6e+09 | **2.9e+09** | 2.8e+09 | 2.8e+09 | 1.3e+09 |
| EPOCH scalar | 9.7e+08 | 1.1e+09 | 1.1e+09 | 1.1e+09 | 9.5e+08 |
| EPOCH SIMD | 2.6e+09 | **2.6e+09** | 2.7e+09 | 2.7e+09 | 1.4e+09 |

With AVX-512, TT2000 conversion peaked at ~**8 billion epochs/s** and EPOCH at ~**14 billion epochs/s** for L1/L2-resident data. With AVX2, SIMD runs 2.5–3× faster than scalar code. CDF_EPOCH conversions are exact since 0.14.0: they used to round to 256 ns. That made scalar EPOCH conversion about 2× slower (it was 2.3e+09 epochs/s on the 5800X), while the SIMD version, reworked to need no 64-bit integer conversions, is faster than the old scalar one. At 64M values (1 GB of data), every SIMD row drops to 1.2–1.5 billion epochs/s: the data no longer fits in cache.

#### Leap-second lookup

Epochs/s, one row per CPU:

| Method | CPU | 1K | 64K | 1M | 64M |
|---|---|---|---|---|---|
| Branchless | 7840U/HS | 9.1e+07 | 9.4e+07 | 9.5e+07 | 1.0e+08 |
| Branchless | 5800X | 1.2e+08 | 1.1e+08 | 1.2e+08 | 1.2e+08 |
| Baseline | 7840U/HS | 2.4e+08 | 2.4e+08 | 2.4e+08 | 2.4e+08 |
| Baseline | 5800X | 3.1e+08 | 3.2e+08 | 3.2e+08 | 3.2e+08 |

#### RLE compression (bytes/s)

| Operation | CPU | 1 KB | 16 KB | 64 KB | 1 MB |
|---|---|---|---|---|---|
| Deflate | 7840U/HS | 7.4e+08 | 6.9e+08 | 3.3e+08 | 2.7e+08 |
| Deflate | 5800X | 6.2e+08 | 3.8e+08 | 2.7e+08 | 2.6e+08 |
| Inflate | 7840U/HS | **1.8e+09** | **1.7e+09** | **1.8e+09** | 4.5e+08 |
| Inflate | 5800X | 1.3e+09 | 6.4e+08 | 4.4e+08 | 4.2e+08 |
| Roundtrip | 7840U/HS | 5.1e+08 | 5.0e+08 | 1.9e+08 | 1.7e+08 |
| Roundtrip | 5800X | 4.5e+08 | 2.0e+08 | 1.7e+08 | 1.7e+08 |

RLE inflate sustains ~**1.8 GB/s** on the 7840U/HS for data that fits in cache; on the 5800X it drops from 1.3 GB/s at 1 KB to 0.4 GB/s at 64 KB. At 1 MB both CPUs run at the same speed.

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
    - [x] Pad values and sparse records
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
