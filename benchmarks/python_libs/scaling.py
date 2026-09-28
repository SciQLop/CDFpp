"""How pycdfpp, spacepy.pycdf and cdflib scale: with threads, reading a folder of 23 CDAWeb
files, and with data size, reading and writing one gzip variable from 1 MB to 1 GB.

    pip install pycdfpp spacepy cdflib requests
    python benchmarks/python_libs/scaling.py
"""
import json
import os
import tempfile
from pathlib import Path
from types import SimpleNamespace

import numpy as np

from compare import (CDAWEB_DATASETS, CDFLIB, LIBRARIES, PYCDFPP, SPACEPY, cdaweb_file,
                     load_folder_threaded, machine, median_seconds, write_file)

THREAD_COUNTS = (1, 2, 4, 8, 16)
SIZES_MB = (1, 10, 100, 1000)
RESULTS = Path(__file__).with_name("results_scaling.json")


def thread_scaling():
    """spacepy is left out: NASA's library can't be used from several threads."""
    folder = [str(cdaweb_file(name, day)) for name, day in CDAWEB_DATASETS]
    return {lib.name: {threads: median_seconds(lambda: load_folder_threaded(lib, folder, threads))
                       for threads in THREAD_COUNTS} for lib in (PYCDFPP, CDFLIB)}


def magnetic_field_like(size_mb):
    """float32 (N, 3): slow variations plus noise, so gzip has something to do, as in real data."""
    records = size_mb * 1_000_000 // 12
    t = np.arange(records, dtype=np.float64)
    noise = np.random.default_rng(0).normal(scale=0.05, size=(records, 3))
    field = np.stack([np.sin(t / 997), np.cos(t / 1009), np.sin(t / 1013)], axis=1) * 50
    return (field + noise).astype(np.float32)


def read_variable(lib, path):
    cdf = lib.open(path)
    lib.values(cdf, "B")
    lib.close(cdf)


def size_scaling(out_dir):
    """Every library reads the same file, written by NASA's library (spacepy), like archive
    files; each writes its own."""
    results = {"write": {lib.name: {} for lib in LIBRARIES}, "read": {lib.name: {} for lib in LIBRARIES}}
    for size_mb in SIZES_MB:
        variables = [SimpleNamespace(name="B", values=magnetic_field_like(size_mb), datetime64=None,
                                     is_time=False, compressed=True)]
        repeats = 3 if size_mb >= 1000 else 5
        for lib in LIBRARIES:
            path = os.path.join(out_dir, f"{lib.name}.cdf")
            results["write"][lib.name][size_mb] = median_seconds(
                lambda: write_file(lib, path, variables), repeats)
        nasa_written = os.path.join(out_dir, f"{SPACEPY.name}.cdf")
        for lib in LIBRARIES:
            results["read"][lib.name][size_mb] = median_seconds(
                lambda: read_variable(lib, nasa_written), repeats)
    return results


def thread_table(results):
    rows = ["| Threads | " + " | ".join(results) + " |", "|---|" + "---|" * len(results)]
    for threads in THREAD_COUNTS:
        rows.append(f"| {threads} | " + " | ".join(
            f"{t[threads]:.2f} s ({t[1] / t[threads]:.1f}×)" for t in results.values()) + " |")
    return "\n".join(rows) + "\n\n(N×) = speed-up over one thread."


def size_table(results, operation):
    libs = list(results[operation])
    rows = [f"| {operation.capitalize()} | " + " | ".join(libs) + " |", "|---|" + "---|" * len(libs)]
    for size_mb in SIZES_MB:
        rows.append(f"| {size_mb} MB | " + " | ".join(
            f"{size_mb / results[operation][lib][size_mb]:.0f} MB/s" for lib in libs) + " |")
    return "\n".join(rows)


def main():
    threads = thread_scaling()
    with tempfile.TemporaryDirectory() as out_dir:
        sizes = size_scaling(out_dir)
    RESULTS.write_text(json.dumps({"machine": machine(), "threads": threads, "sizes": sizes},
                                  indent=2))
    print(thread_table(threads), size_table(sizes, "read"), size_table(sizes, "write"),
          sep="\n\n")


if __name__ == "__main__":
    main()
