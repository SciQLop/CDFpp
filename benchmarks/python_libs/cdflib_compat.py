"""Compares cdflib with pycdfpp.cdflib, its drop-in replacement, on real CDAWeb files.

Both run the same code through their own namespace. Runs alternate between the two, and the
best of REPEATS is kept: on a shared machine the minimum is the least disturbed run.

pycdfpp uses several threads for compressed variables (decompression on read, compression on
write), cdflib one: part of the gap on those workloads is multithreading.

    pip install pycdfpp cdflib xarray requests
    python benchmarks/python_libs/cdflib_compat.py
"""
import json
import os
import statistics
import sys
import tempfile
import time
import warnings
from pathlib import Path
from types import SimpleNamespace

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "compression"))
from corpus import CDAWEB_DATASETS, cdaweb_file  # noqa: E402

import cdflib  # noqa: E402
import cdflib.cdfwrite  # noqa: E402
import cdflib.epochs  # noqa: E402
import cdflib.xarray  # noqa: E402
import pycdfpp.cdflib  # noqa: E402
import pycdfpp.cdflib.xarray  # noqa: E402

REPEATS = int(os.environ.get("REPEATS", 7))
BULK = 1_000_000
RESULTS = Path(__file__).with_name("results_cdflib_compat.json")
LIBRARIES = (
    SimpleNamespace(name="cdflib", ns=cdflib, epochs=cdflib.epochs, writer=cdflib.cdfwrite,
                    xarray=cdflib.xarray),
    SimpleNamespace(name="pycdfpp.cdflib", ns=pycdfpp.cdflib, epochs=pycdfpp.cdflib.epochs,
                    writer=pycdfpp.cdflib.cdfwrite, xarray=pycdfpp.cdflib.xarray),
)


def dataset(name):
    return str(cdaweb_file(name, dict(CDAWEB_DATASETS)[name]))


def variable_names(cdf):
    info = cdf.cdf_info()
    return info.rVariables + info.zVariables


def open_and_read_one(lib, path, variable):
    lib.ns.CDF(path).varget(variable)


def open_and_read_all(lib, path):
    cdf = lib.ns.CDF(path)
    for name in variable_names(cdf):
        cdf.varget(name)


def to_datetime(lib, values):
    lib.epochs.CDFepoch.to_datetime(values)


def write_file(lib, out_dir, time_values, field):
    path = os.path.join(out_dir, f"{lib.name}.cdf")
    if os.path.exists(path):
        os.remove(path)
    cdf = lib.writer.CDF(path, cdf_spec={"Compressed": False})
    for name, data_type, data, dims, compress in (
            ("Epoch", cdf.CDF_TIME_TT2000, time_values, [], 0),
            ("B", cdf.CDF_REAL4, field, list(field.shape[1:]), 6)):
        cdf.write_var({"Variable": name, "Data_Type": data_type, "Num_Elements": 1,
                       "Rec_Vary": True, "Dim_Sizes": dims, "Compress": compress},
                      var_attrs={}, var_data=data)
    cdf.close()


def to_xarray(lib, path):
    lib.xarray.cdf_to_xarray(path)


def interleaved_best(task, libraries=LIBRARIES, repeats=REPEATS):
    for lib in libraries:
        task(lib)  # warm-up: page cache, lazy imports
    best = {lib.name: float("inf") for lib in libraries}
    for _ in range(repeats):
        for lib in libraries:
            start = time.perf_counter()
            task(lib)
            best[lib.name] = min(best[lib.name], time.perf_counter() - start)
    return best


def check_same_file(out_dir, time_values, field):
    """Both writers must produce files holding the same values."""
    import pycdfpp
    for lib in LIBRARIES:
        back = pycdfpp.load(os.path.join(out_dir, f"{lib.name}.cdf"))
        assert np.array_equal(back["B"].values, field), f"{lib.name} wrote B wrong"
        assert np.array_equal(back["Epoch"].values.view(np.int64).ravel(), time_values)


def scenarios(out_dir):
    fgm = dataset("MMS1_FGM_SRVY_L2")
    fpi = dataset("MMS1_FPI_FAST_L2_DES-DIST")
    wind = dataset("WI_H2_MFI")
    tt2000 = np.resize(cdflib.CDF(fgm).varget("Epoch"), BULK)
    cdf_epoch = np.resize(cdflib.CDF(wind).varget("Epoch"), BULK)
    time_values = cdflib.CDF(fgm).varget("Epoch")
    field = cdflib.CDF(fgm).varget("mms1_fgm_b_gse_srvy_l2")
    return (
        ("Open, read one variable", "MMS FGM survey, 1.2 M points, gzip",
         lambda lib: open_and_read_one(lib, fgm, "mms1_fgm_b_gse_srvy_l2")),
        ("Open, read every variable", "MMS FPI electron distribution, 178 MB, gzip",
         lambda lib: open_and_read_all(lib, fpi)),
        ("CDFepoch.to_datetime", f"{BULK:,} CDF_TIME_TT2000 values",
         lambda lib: to_datetime(lib, tt2000)),
        ("CDFepoch.to_datetime", f"{BULK:,} CDF_EPOCH values",
         lambda lib: to_datetime(lib, cdf_epoch)),
        ("cdfwrite: write a file", f"time and B, {len(time_values):,} records, gzip",
         lambda lib: write_file(lib, out_dir, time_values, field)),
        ("cdf_to_xarray", "MMS FGM survey, 1.2 M points, gzip", lambda lib: to_xarray(lib, fgm)),
    ), (time_values, field)


def _duration(seconds):
    return f"{seconds * 1e3:.1f} ms" if seconds < 1 else f"{seconds:.2f} s"


def markdown_table(results):
    rows = ["| Task | Data | cdflib | pycdfpp.cdflib | Speedup |", "|---|---|---|---|---|"]
    for r in results:
        theirs, ours = r["seconds"]["cdflib"], r["seconds"]["pycdfpp.cdflib"]
        rows.append(f"| {r['task']} | {r['data']} | {_duration(theirs)} | {_duration(ours)} "
                    f"| {theirs / ours:.1f}× |")
    return "\n".join(rows)


def load_average():
    return os.getloadavg()[0]


def main():
    warnings.filterwarnings("ignore")
    with tempfile.TemporaryDirectory() as out_dir:
        tasks, (time_values, field) = scenarios(out_dir)
        load = load_average()
        results = [{"task": title, "data": data, "seconds": interleaved_best(task)}
                   for title, data, task in tasks]
        check_same_file(out_dir, time_values, field)
    RESULTS.write_text(json.dumps({"load_1min_before": load, "repeats": REPEATS,
                                   "results": results}, indent=2) + "\n")
    print(markdown_table(results))
    print(f"\nBest of {REPEATS} interleaved runs, files in the page cache, "
          f"1 min load average {load:.2f} when starting.")


if __name__ == "__main__":
    main()
