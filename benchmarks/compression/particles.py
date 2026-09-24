"""Codec and layout comparison on particle distributions (MMS FPI, PSP SWEAP, Solar Orbiter SWA).

For every distribution variable (multi-dimensional records, or records over 255 bytes) it compares
GZIP, Zstd and Blosc2 as CDFpp writes it ("record-major": records one after the other) with a
"transposed" layout that stores each record cell as one time series ([N, K] -> [K, N]), so the
codec sees how every cell evolves across records. Prints one table per file and a total.
"""
import sys
import time
import zlib
from collections import defaultdict
from compression import zstd
from math import prod

import blosc2
import numpy as np

from corpus import cdaweb_file
from sweep import raw_variables

PARTICLE_DATASETS = (
    ("MMS1_FPI_FAST_L2_DES-DIST", "2020-01-01"),
    ("MMS1_FPI_FAST_L2_DIS-DIST", "2020-01-01"),
    ("MMS1_FPI_BRST_L2_DES-DIST", "2017-07-11"),
    ("MMS1_FPI_BRST_L2_DIS-DIST", "2017-07-11"),
    ("PSP_SWP_SPI_SF00_L2_8DX32EX8A", "2021-04-28"),
    ("PSP_SWP_SPA_SF0_L2_16AX8DX32E", "2021-04-28"),
    ("SOLO_L2_SWA-PAS-VDF", "2021-06-01"),
    ("SOLO_L2_SWA-EAS1-NM3D-PSD", "2022-03-01"),
    ("SOLO_L2_SWA-EAS-PAD-DNF", "2022-03-01"),
)

F = blosc2.Filter


def blosc2_size(raw: bytes, typesize: int, filters) -> int:
    cparams = blosc2.CParams(codec=blosc2.Codec.ZSTD, clevel=5, nthreads=1, typesize=typesize,
                             filters=list(filters), filters_meta=[0] * len(filters))
    return len(blosc2.compress2(raw, cparams=cparams))


def transposed(raw: bytes, n_records: int, record_values: int, item_size: int) -> bytes:
    items = np.frombuffer(raw, dtype=np.dtype(f"V{item_size}")).reshape(n_records, record_values)
    return np.ascontiguousarray(items.T).tobytes()


def cdfpp_typesize(record_bytes: int, item_size: int) -> int:
    return record_bytes if record_bytes <= 255 else item_size


def timed(f):
    start = time.perf_counter()
    return f(), time.perf_counter() - start


def measure(raw: bytes, item_size: int, record_shape) -> dict:
    k = prod(record_shape)
    n = len(raw) // (k * item_size)
    t = transposed(raw, n, k, item_size)
    (b2, b2_s) = timed(lambda: blosc2_size(raw, cdfpp_typesize(k * item_size, item_size), [F.SHUFFLE]))
    (gz, gz_s) = timed(lambda: len(zlib.compress(raw, 6)))
    sizes = {
        "raw": len(raw),
        "gzip6": gz,
        "zstd1": len(zstd.compress(raw, level=1)),
        "blosc2": b2,
        "blosc2 bitshuffle": blosc2_size(raw, item_size, [F.BITSHUFFLE]),
        "transposed": blosc2_size(t, item_size, [F.SHUFFLE]),
        "transposed bitshuffle": blosc2_size(t, item_size, [F.BITSHUFFLE]),
    }
    sizes["best layout"] = min(sizes["blosc2"], sizes["transposed"])
    return sizes | {"gzip6_s": gz_s, "blosc2_s": b2_s}


def is_distribution(record_shape, item_size) -> bool:
    return len(record_shape) >= 2 or prod(record_shape) * item_size > 255


COLUMNS = ("gzip6", "zstd1", "blosc2", "blosc2 bitshuffle", "transposed", "transposed bitshuffle", "best layout")


def report(label: str, t: dict):
    raw = t["raw"]
    cells = "  ".join(f"{raw / t[c]:5.2f}" for c in COLUMNS)
    speed = f"gzip {raw / 2**20 / t['gzip6_s']:5.0f} MiB/s, blosc2 {raw / 2**20 / t['blosc2_s']:5.0f} MiB/s"
    print(f"{label:34s} {raw / 2**20:8.1f}  {cells}   {speed}")


def main():
    print(f"{'':34s} {'raw MiB':>8s}  " + "  ".join(f"{c[:5]:>5s}" for c in COLUMNS) + "   (ratios: raw / compressed)")
    grand = defaultdict(float)
    for dataset, day in PARTICLE_DATASETS:
        try:
            path = cdaweb_file(dataset, day)
        except Exception as error:
            print(f"skipped {dataset}: {error}", file=sys.stderr)
            continue
        per_file = defaultdict(float)
        for name, variable, raw, item_size, record_shape in raw_variables(path):
            if not is_distribution(record_shape, item_size):
                continue
            for key, value in measure(raw, item_size, record_shape).items():
                per_file[key] += value
        if per_file:
            report(dataset, per_file)
            for key, value in per_file.items():
                grand[key] += value
    report("ALL DISTRIBUTIONS", grand)


if __name__ == "__main__":
    main()
