"""Compares pycdfpp, spacepy.pycdf and cdflib on everyday tasks with real CDAWeb files.

Each library is an adapter exposing the same operations, written the way its documentation
recommends. The scenarios are written once, against the adapter.

    pip install pycdfpp spacepy cdflib requests
    python benchmarks/python_libs/compare.py
"""
import json
import os
import platform
import statistics
import sys
import time
import warnings
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
from types import SimpleNamespace

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "compression"))
from corpus import CDAWEB_DATASETS, cdaweb_file  # noqa: E402

warnings.filterwarnings("ignore")  # spacepy warns about its leap-second table on import

import cdflib  # noqa: E402
import pycdfpp  # noqa: E402
from spacepy import pycdf  # noqa: E402

REPEATS = int(os.environ.get("REPEATS", 5))
THREADS = 8
RESULTS = Path(__file__).with_name("results.json")


PYCDFPP = SimpleNamespace(
    name="pycdfpp",
    open=pycdfpp.load,
    close=lambda cdf: None,
    variables=lambda cdf: list(cdf.keys()),
    global_attributes=lambda cdf: {name: list(attr) for name, attr in cdf.attributes.items()},
    variable_attributes=lambda cdf, var: {name: a.value for name, a in cdf[var].attributes.items()},
    values=lambda cdf, var: cdf[var].values,
    datetime64=lambda cdf, var: pycdfpp.to_datetime64(cdf[var]),
)

# spacepy converts time variables to Python datetime objects; raw_var skips that for plain reads.
SPACEPY = SimpleNamespace(
    name="spacepy.pycdf",
    open=pycdf.CDF,
    close=lambda cdf: cdf.close(),
    variables=lambda cdf: list(cdf.keys()),
    global_attributes=lambda cdf: {name: attr[...] for name, attr in cdf.attrs.items()},
    variable_attributes=lambda cdf, var: dict(cdf[var].attrs),
    values=lambda cdf, var: cdf.raw_var(var)[...],
    datetime64=lambda cdf, var: np.array(cdf[var][...], dtype="datetime64[ns]"),
)


def _cdflib_variables(cdf):
    info = cdf.cdf_info()
    return info.rVariables + info.zVariables


CDFLIB = SimpleNamespace(
    name="cdflib",
    open=cdflib.CDF,
    close=lambda cdf: None,
    variables=_cdflib_variables,
    global_attributes=lambda cdf: cdf.globalattsget(),
    variable_attributes=lambda cdf, var: cdf.varattsget(var),
    values=lambda cdf, var: cdf.varget(var),
    datetime64=lambda cdf, var: cdflib.cdfepoch.to_datetime(cdf.varget(var)),
)

LIBRARIES = (PYCDFPP, SPACEPY, CDFLIB)


def peek(lib, path):
    cdf = lib.open(path)
    lib.global_attributes(cdf)
    for var in lib.variables(cdf):
        lib.variable_attributes(cdf, var)
    lib.close(cdf)


def plot_field(lib, path, time_var, field_var):
    cdf = lib.open(path)
    lib.datetime64(cdf, time_var)
    lib.values(cdf, field_var)
    lib.close(cdf)


def load_everything(lib, path):
    cdf = lib.open(path)
    for var in lib.variables(cdf):
        lib.values(cdf, var)
    lib.close(cdf)


def load_folder(lib, paths):
    for path in paths:
        load_everything(lib, path)


def load_folder_threaded(lib, paths):
    with ThreadPoolExecutor(THREADS) as pool:
        list(pool.map(lambda path: load_everything(lib, path), paths))


def dataset(name):
    return str(cdaweb_file(name, dict(CDAWEB_DATASETS)[name]))


def scenarios():
    fpi = dataset("MMS1_FPI_FAST_L2_DES-DIST")
    fgm = dataset("MMS1_FGM_SRVY_L2")
    wind = dataset("WI_H2_MFI")
    folder = [str(cdaweb_file(name, day)) for name, day in CDAWEB_DATASETS]
    folder_data = f"{len(folder)} CDAWeb files, 11 missions, {_size_mb(folder)} MB"
    return (
        ("Open a file, list variables, read all attributes", "MMS FPI electron distribution, 178 MB",
         lambda lib: peek(lib, fpi), LIBRARIES),
        ("Read B and its time axis as datetime64", "MMS FGM survey, 1.2 M points, gzip, TT2000",
         lambda lib: plot_field(lib, fgm, "Epoch", "mms1_fgm_b_gse_srvy_l2"), LIBRARIES),
        ("Read B and its time axis as datetime64", "Wind MFI, 0.9 M points, CDF_EPOCH",
         lambda lib: plot_field(lib, wind, "Epoch", "BGSE"), LIBRARIES),
        ("Read every variable of a file", "MMS FPI electron distribution, 178 MB, gzip",
         lambda lib: load_everything(lib, fpi), LIBRARIES),
        ("Read every variable of a folder", folder_data,
         lambda lib: load_folder(lib, folder), LIBRARIES),
        (f"Same folder, {THREADS} threads", folder_data,
         lambda lib: load_folder_threaded(lib, folder), (PYCDFPP, CDFLIB)),
    )


def _size_mb(paths):
    return round(sum(os.path.getsize(p) for p in paths) / 1e6)


def median_seconds(task):
    task()  # warm-up: file in the page cache, imports and lazy initialisations done
    durations = []
    for _ in range(REPEATS):
        start = time.perf_counter()
        task()
        durations.append(time.perf_counter() - start)
    return statistics.median(durations)


def run_scenario(title, data, task, libraries):
    timings = {lib.name: median_seconds(lambda: task(lib)) for lib in libraries}
    return {"task": title, "data": data, "seconds": timings}


def machine():
    return {"cpu": _cpu_name(), "cores": os.cpu_count(), "python": platform.python_version(),
            "versions": {"pycdfpp": pycdfpp.__version__, "spacepy": _spacepy_version(),
                         "NASA CDF (in spacepy)": ".".join(map(str, pycdf.lib.version[:3])),
                         "cdflib": cdflib.__version__, "numpy": np.__version__}}


def _spacepy_version():
    import spacepy
    return spacepy.__version__


def _cpu_name():
    cpuinfo = Path("/proc/cpuinfo")
    if cpuinfo.exists():
        for line in cpuinfo.read_text().splitlines():
            if line.startswith("model name"):
                return line.split(":", 1)[1].strip()
    return platform.processor()


def markdown_table(results):
    rows = ["| Task | Data | pycdfpp | spacepy.pycdf | cdflib |", "|---|---|---|---|---|"]
    for result in results:
        rows.append(f"| {result['task']} | {result['data']} | " + " | ".join(
            _cell(result["seconds"], name) for name in ("pycdfpp", "spacepy.pycdf", "cdflib")) + " |")
    rows.append("\n(N×) = N times longer than pycdfpp. Median of %d runs, files in the page cache." % REPEATS)
    return "\n".join(rows)


def _cell(seconds, name):
    if name not in seconds:
        return "not thread-safe"
    if name == "pycdfpp":
        return f"**{_duration(seconds[name])}**"
    return f"{_duration(seconds[name])} ({_factor(seconds[name] / seconds['pycdfpp'])})"


def _factor(ratio):
    return f"{ratio:.0f}×" if ratio >= 10 else f"{ratio:.1f}×"


def _duration(seconds):
    return f"{seconds * 1e3:.1f} ms" if seconds < 1 else f"{seconds:.2f} s"


def main():
    results = [run_scenario(*scenario) for scenario in scenarios()]
    RESULTS.write_text(json.dumps({"machine": machine(), "results": results}, indent=2))
    print(markdown_table(results))


if __name__ == "__main__":
    main()
