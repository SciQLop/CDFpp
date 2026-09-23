"""End-to-end CDFpp benchmark on the corpus: for each codec, compress every variable with it, save
the whole file, reload it eagerly, check all values survived, and record file size, save and load
times. One CSV row per (file, codec)."""
import csv
import sys
import time
from pathlib import Path

import pycdfpp

from corpus import corpus

CT = pycdfpp.CompressionType
CODECS = [name for name in ("no_compression", "gzip_compression", "zstd_compression", "blosc2_compression")
          if hasattr(CT, name)]
FIELDS = ["source", "file", "original_bytes", "original_compression", "codec", "bytes", "save_s", "load_s"]


def original_compression(cdf) -> str:
    kinds = sorted({str(cdf[name].compression).split(".")[-1] for name in cdf} | {str(cdf.compression).split(".")[-1]})
    return "+".join(kinds)


def same_values(a, b) -> bool:
    # Bit-exact: lossless codecs must restore every byte, and NaN fill values defeat array_equal.
    return all(a[name].values_encoded.tobytes() == b[name].values_encoded.tobytes() for name in a)


def timed(function, repeats: int):
    """Best of `repeats` wall-clock runs, with the last result."""
    best, result = float("inf"), None
    for _ in range(repeats):
        start = time.perf_counter()
        result = function()
        best = min(best, time.perf_counter() - start)
    return best, result


def bench_file(source: str, path: Path):
    cdf = pycdfpp.load(str(path), lazy_load=False)
    before = original_compression(cdf)
    # simplify: best-of-3 only for small files, single run above 50 MiB; enough to rank codecs,
    # not a statistically rigorous timing study.
    repeats = 3 if path.stat().st_size < 50 * 2**20 else 1
    for codec in CODECS:
        for name in cdf:
            cdf[name].compression = getattr(CT, codec)
        save_s, saved = timed(lambda: bytes(memoryview(pycdfpp.save(cdf))), repeats)
        load_s, reloaded = timed(lambda: pycdfpp.load(saved, lazy_load=False), repeats)
        if not same_values(cdf, reloaded):
            raise RuntimeError(f"{path.name}: values changed through {codec}")
        yield dict(source=source, file=path.name, original_bytes=path.stat().st_size,
                   original_compression=before, codec=codec, bytes=len(saved),
                   save_s=f"{save_s:.6f}", load_s=f"{load_s:.6f}")


def main(output: Path):
    with output.open("w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=FIELDS)
        writer.writeheader()
        for source, path in corpus():
            print(f"end-to-end {path.name}", file=sys.stderr, flush=True)
            writer.writerows(bench_file(source, path))


if __name__ == "__main__":
    main(Path(sys.argv[1]) if len(sys.argv) > 1 else Path("end_to_end.csv"))
