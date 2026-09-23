"""Per-variable codec sweep on the corpus: compresses each variable's raw bytes (exactly what CDFpp
hands to the codec) with gzip, zstd and a grid of blosc2 settings, and writes one CSV row per
(variable, config). Blosc2 is python-blosc2, which bundles the same C-blosc2 as CDFpp's wrap.

Shape hint: "elem" uses the CDF element size as blosc2 typesize; "record" uses the whole record
(flattened record shape x element size) when it fits blosc2's 255-byte typesize limit, so each
record component gets its own shuffled byte streams (e.g. Bx, By, Bz of a vector)."""
import csv
import sys
import time
import zlib
from compression import zstd
from dataclasses import dataclass
from math import prod
from pathlib import Path
from typing import Callable

import blosc2
import numpy as np
import pycdfpp

from corpus import corpus

Z, LZ4 = blosc2.Codec.ZSTD, blosc2.Codec.LZ4
NO, SH, BIT, DELTA = blosc2.Filter.NOFILTER, blosc2.Filter.SHUFFLE, blosc2.Filter.BITSHUFFLE, blosc2.Filter.DELTA


@dataclass(frozen=True)
class Blosc2Config:
    codec: blosc2.Codec
    clevel: int
    filters: tuple
    shape_hint: str  # "elem" | "record"


BLOSC2_CONFIGS = {
    "blosc2-zstd5-noshuffle": Blosc2Config(Z, 5, (NO,), "elem"),
    "blosc2-zstd5-shuffle": Blosc2Config(Z, 5, (SH,), "elem"),
    "blosc2-zstd5-shuffle-record": Blosc2Config(Z, 5, (SH,), "record"),
    "blosc2-zstd5-bitshuffle": Blosc2Config(Z, 5, (BIT,), "elem"),
    "blosc2-zstd5-bitshuffle-record": Blosc2Config(Z, 5, (BIT,), "record"),
    "blosc2-zstd5-delta-shuffle": Blosc2Config(Z, 5, (DELTA, SH), "elem"),
    "blosc2-zstd1-shuffle": Blosc2Config(Z, 1, (SH,), "elem"),
    "blosc2-zstd9-shuffle": Blosc2Config(Z, 9, (SH,), "elem"),
    "blosc2-lz4-5-shuffle": Blosc2Config(LZ4, 5, (SH,), "elem"),
    "blosc2-lz4-5-bitshuffle": Blosc2Config(LZ4, 5, (BIT,), "elem"),
}


def blosc2_typesize(config: Blosc2Config, item_size: int, record_bytes: int) -> int:
    return record_bytes if config.shape_hint == "record" and record_bytes <= 255 else item_size


def blosc2_codec(config: Blosc2Config) -> Callable:
    def codec(raw: bytes, item_size: int, record_bytes: int):
        cparams = blosc2.CParams(codec=config.codec, clevel=config.clevel, nthreads=1,
                                 typesize=blosc2_typesize(config, item_size, record_bytes),
                                 filters=list(config.filters), filters_meta=[0] * len(config.filters))
        return blosc2.compress2(raw, cparams=cparams), blosc2.decompress2
    return codec


# name -> codec(raw, item_size, record_bytes) -> (compressed, decompress)
CODECS: dict[str, Callable] = {
    "gzip6": lambda raw, *_: (zlib.compress(raw, 6), zlib.decompress),
    "zstd1": lambda raw, *_: (zstd.compress(raw, level=1), zstd.decompress),
    "zstd3": lambda raw, *_: (zstd.compress(raw, level=3), zstd.decompress),
} | {name: blosc2_codec(config) for name, config in BLOSC2_CONFIGS.items()}

FIELDS = ["source", "file", "variable", "cdf_type", "record_shape", "item_size", "raw_bytes",
          "config", "compressed_bytes", "compress_s", "decompress_s"]


def measure(codec: Callable, raw: bytes, item_size: int, record_bytes: int) -> tuple[int, float, float]:
    start = time.perf_counter()
    compressed, decompress = codec(raw, item_size, record_bytes)
    compressed_at = time.perf_counter()
    restored = decompress(compressed)
    decompressed_at = time.perf_counter()
    if bytes(restored) != raw:
        raise RuntimeError("round trip mismatch")
    return len(compressed), compressed_at - start, decompressed_at - compressed_at


def raw_variables(path: Path):
    cdf = pycdfpp.load(str(path))
    for name in cdf:
        variable = cdf[name]
        if variable.type in (pycdfpp.DataType.CDF_CHAR, pycdfpp.DataType.CDF_UCHAR):
            continue  # decoded to unicode by pycdfpp, so raw bytes are not reachable; negligible size
        values = np.ascontiguousarray(variable.values_encoded)
        if values.nbytes == 0:
            continue
        record_shape = tuple(variable.shape[1:])
        yield name, variable, values.view(np.uint8).tobytes(), values.itemsize, record_shape


def sweep_file(source: str, path: Path):
    for name, variable, raw, item_size, record_shape in raw_variables(path):
        record_bytes = prod(record_shape) * item_size
        for config, codec in CODECS.items():
            size, c_s, d_s = measure(codec, raw, item_size, record_bytes)
            yield dict(source=source, file=path.name, variable=name, cdf_type=str(variable.type).split(".")[-1],
                       record_shape="x".join(map(str, record_shape)), item_size=item_size,
                       raw_bytes=len(raw), config=config, compressed_bytes=size,
                       compress_s=f"{c_s:.6f}", decompress_s=f"{d_s:.6f}")


def main(output: Path):
    with output.open("w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=FIELDS)
        writer.writeheader()
        for source, path in corpus():
            print(f"sweeping {path.name}", file=sys.stderr, flush=True)
            writer.writerows(sweep_file(source, path))


if __name__ == "__main__":
    main(Path(sys.argv[1]) if len(sys.argv) > 1 else Path("sweep.csv"))
