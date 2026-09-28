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
import tempfile
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
from cdflib.cdfwrite import CDF as CDFWriter  # noqa: E402
import pycdfpp  # noqa: E402
import spacepy.time  # noqa: E402
from spacepy import pycdf  # noqa: E402

REPEATS = int(os.environ.get("REPEATS", 5))
THREADS = 8
RESULTS = Path(__file__).with_name("results.json")


GZIP_LEVEL = 6  # pycdfpp's level; the two others are set to it


def _pycdfpp_write(path, variables):
    """Times are written from datetime64: pycdfpp takes no raw TT2000 integers."""
    cdf = pycdfpp.CDF()
    for var in variables:
        cdf.add_variable(var.name, values=var.datetime64 if var.is_time else var.values,
                         compression=pycdfpp.CompressionType.gzip_compression if var.compressed
                         else pycdfpp.CompressionType.no_compression)
    pycdfpp.save(cdf, path)


PYCDFPP = SimpleNamespace(
    name="pycdfpp",
    open=pycdfpp.load,
    close=lambda cdf: None,
    variables=lambda cdf: list(cdf.keys()),
    global_attributes=lambda cdf: {name: list(attr) for name, attr in cdf.attributes.items()},
    variable_attributes=lambda cdf, var: {name: a.value for name, a in cdf[var].attributes.items()},
    values=lambda cdf, var: cdf[var].values,
    datetime64=lambda cdf, var: pycdfpp.to_datetime64(cdf[var]),
    write=_pycdfpp_write,
)

def _spacepy_datetime64(cdf, var):
    """Ticktock is spacepy's vectorized path, but it only handles CDF_EPOCH, not TT2000."""
    if cdf[var].type() == pycdf.const.CDF_EPOCH.value:
        unix_seconds = spacepy.time.Ticktock(cdf.raw_var(var)[...].reshape(-1), "CDF").UNX
        return (np.asarray(unix_seconds) * 1e9).astype("datetime64[ns]")
    return np.array(cdf[var][...], dtype="datetime64[ns]")


def _spacepy_write(path, variables):
    """Raw TT2000 integers, through raw_var: its fastest documented path (datetime objects
    would be much slower)."""
    with pycdf.CDF(path, "") as cdf:
        for var in variables:
            if var.is_time:
                cdf.new(var.name, type=pycdf.const.CDF_TIME_TT2000, recVary=True)
                cdf.raw_var(var.name)[...] = var.values
            elif var.compressed:
                cdf.new(var.name, data=var.values, compress=pycdf.const.GZIP_COMPRESSION,
                        compress_param=GZIP_LEVEL)
            else:
                cdf.new(var.name, data=var.values)


# spacepy converts time variables to Python datetime objects; raw_var skips that for plain reads.
SPACEPY = SimpleNamespace(
    name="spacepy.pycdf",
    open=pycdf.CDF,
    close=lambda cdf: cdf.close(),
    variables=lambda cdf: list(cdf.keys()),
    global_attributes=lambda cdf: {name: attr[...] for name, attr in cdf.attrs.items()},
    variable_attributes=lambda cdf, var: dict(cdf[var].attrs),
    values=lambda cdf, var: cdf.raw_var(var)[...],
    datetime64=_spacepy_datetime64,
    write=_spacepy_write,
)


def _cdflib_variables(cdf):
    info = cdf.cdf_info()
    return info.rVariables + info.zVariables


def _cdflib_write(path, variables):
    """Raw TT2000 integers: its datetime64 input is written as raw integers, 30 years off."""
    cdf = CDFWriter(path, cdf_spec={"Compressed": False})
    for var in variables:
        data_type = cdf.CDF_TIME_TT2000 if var.is_time else cdf.CDF_REAL4
        cdf.write_var({"Variable": var.name, "Data_Type": data_type, "Num_Elements": 1,
                       "Rec_Vary": True, "Dim_Sizes": list(var.values.shape[1:]),
                       "Compress": GZIP_LEVEL if var.compressed else 0},
                      var_attrs={}, var_data=var.values)
    cdf.close()


CDFLIB = SimpleNamespace(
    name="cdflib",
    open=cdflib.CDF,
    close=lambda cdf: None,
    variables=_cdflib_variables,
    global_attributes=lambda cdf: cdf.globalattsget(),
    variable_attributes=lambda cdf, var: cdf.varattsget(var),
    values=lambda cdf, var: cdf.varget(var),
    datetime64=lambda cdf, var: cdflib.cdfepoch.to_datetime(cdf.varget(var)),
    write=_cdflib_write,
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


def load_folder_threaded(lib, paths, threads=THREADS):
    with ThreadPoolExecutor(threads) as pool:
        list(pool.map(lambda path: load_everything(lib, path), paths))


def time_variable(cdf, name):
    """What each library writes from: raw TT2000 integers, or datetime64 for pycdfpp."""
    return SimpleNamespace(name=name, values=cdf[name].values.view(np.int64).ravel(),
                           datetime64=pycdfpp.to_datetime64(cdf[name]).ravel(), is_time=True,
                           compressed=False)


def data_variable(cdf, name, compressed):
    return SimpleNamespace(name=name, values=np.ascontiguousarray(cdf[name].values),
                           datetime64=None, is_time=False, compressed=compressed)


def write_file(lib, path, variables):
    if os.path.exists(path):  # spacepy and cdflib refuse to overwrite
        os.remove(path)
    lib.write(path, variables)


def written_file_check(lib, path, variables):
    """Every library must write the same values: read them back with pycdfpp."""
    back = pycdfpp.load(path)
    for var in variables:
        got = back[var.name].values
        got = got.view(np.int64).ravel() if var.is_time else got
        assert np.array_equal(got, var.values), f"{lib.name} wrote {var.name} wrong"
    return os.path.getsize(path)


def dataset(name):
    return str(cdaweb_file(name, dict(CDAWEB_DATASETS)[name]))


def write_scenarios(out_dir):
    fgm = pycdfpp.load(dataset("MMS1_FGM_SRVY_L2"), lazy_load=False)
    fpi = pycdfpp.load(dataset("MMS1_FPI_FAST_L2_DES-DIST"), lazy_load=False)
    b_field = [time_variable(fgm, "Epoch"), data_variable(fgm, "mms1_fgm_b_gse_srvy_l2", True)]
    b_field_raw = [time_variable(fgm, "Epoch"),
                   data_variable(fgm, "mms1_fgm_b_gse_srvy_l2", False)]
    distribution = [time_variable(fpi, "Epoch"), data_variable(fpi, "mms1_des_dist_fast", True),
                    data_variable(fpi, "mms1_des_disterr_fast", True),
                    data_variable(fpi, "mms1_des_energy_fast", False)]
    size = lambda variables: f"{sum(v.values.nbytes for v in variables) / 1e6:.0f} MB"
    return tuple(
        (title, data, variables, lambda lib, variables=variables, tag=tag:
            write_file(lib, os.path.join(out_dir, f"{tag}_{lib.name}.cdf"), variables), tag)
        for title, data, variables, tag in (
            ("Write B and its time axis, gzip", f"MMS FGM survey, 1.2 M points, {size(b_field)}",
             b_field, "b_gzip"),
            ("Write B and its time axis, uncompressed",
             f"MMS FGM survey, 1.2 M points, {size(b_field_raw)}", b_field_raw, "b_raw"),
            ("Write a particle distribution file, gzip",
             f"MMS FPI electron distribution, {size(distribution)}", distribution,
             "distribution")))


def run_write_scenario(out_dir, title, data, variables, task, tag):
    result = run_scenario(title, data, task, LIBRARIES)
    result["bytes"] = {lib.name: written_file_check(
        lib, os.path.join(out_dir, f"{tag}_{lib.name}.cdf"), variables) for lib in LIBRARIES}
    return result


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


def median_seconds(task, repeats=REPEATS):
    task()  # warm-up: file in the page cache, imports and lazy initialisations done
    durations = []
    for _ in range(repeats):
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
    with tempfile.TemporaryDirectory() as out_dir:
        results += [run_write_scenario(out_dir, *scenario) for scenario in write_scenarios(out_dir)]
    RESULTS.write_text(json.dumps({"machine": machine(), "results": results}, indent=2))
    print(markdown_table(results))


if __name__ == "__main__":
    main()
