#!python

"""Chrono profiling for pycdfpp datetime64 conversion.

This script is expected to be run from the build directory.
To profile, you can use:
    `perf record --debuginfod -F 16000 --call-graph dwarf -g -o perf.data python <path_to_here>/chrono_profiling.py`
"""
import sys, os
sys.path.append(".")
try:
    import pycdfpp
except ImportError as e:
    raise ImportError(
                "Please run this script from the build directory where pycdfpp is built."
                ) from e

import numpy as np
from subprocess import run
from tempfile import NamedTemporaryFile

prelude = """
import sys
sys.path.append(".")
import os
os.environ["TZ"]="UTC"
import pycdfpp
import numpy as np
from datetime import datetime
"""

build_tt2000_before_2017 = 'pycdfpp.to_tt2000((np.arange(0, 10_000_000, dtype=np.int64)*200_000_000_000).astype("datetime64[ns]"))'
build_tt2000_after_2017 = 'pycdfpp.to_tt2000((np.arange(0, 10_000_000, dtype=np.int64)*20_000_000_000 + 1500_000_000_000_000_000).astype("datetime64[ns]"))'
build_epoch16 = 'pycdfpp.to_epoch16((np.arange(0, 10_000_000, dtype=np.int64)*200_000_000_000).astype("datetime64[ns]"))'
build_epoch = 'pycdfpp.to_epoch((np.arange(0, 10_000_000, dtype=np.int64)*200_000_000_000).astype("datetime64[ns]"))'

fragments = {
    "tt2000_to_datetime64_before_2017": f"""
{prelude}
ref = {build_tt2000_before_2017}
for _ in range(10):
    res = pycdfpp.to_datetime64(ref)
""",
    "tt2000_to_datetime64_after_2017": f"""
{prelude}
ref = {build_tt2000_after_2017}
for _ in range(10):
    res = pycdfpp.to_datetime64(ref)
""",
    "epoch16_to_datetime64": f"""
{prelude}
ref = {build_epoch16}
for _ in range(10):
    res = pycdfpp.to_datetime64(ref)
""",
    "epoch_to_datetime64": f"""
{prelude}
ref = {build_epoch}
for _ in range(10):
    res = pycdfpp.to_datetime64(ref)
""",
}


for name,func in fragments.items():
    with NamedTemporaryFile("w") as f:
        f.write(func)
        f.flush()
        dest_dir = f"perf_data/{name}"
        os.makedirs(f"{dest_dir}", exist_ok=True)
        r=run(f"perf record --debuginfod -F 16000 --call-graph dwarf -g -o {dest_dir}/perf.data {sys.executable} {f.name}".split(),
            check=True,
            cwd=os.getcwd(),
            )
