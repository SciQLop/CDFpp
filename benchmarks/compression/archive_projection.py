"""Projects codec gains onto the whole SPDF CDF archive.

    python archive_projection.py measure census.json samples.csv   # download + benchmark samples
    python archive_projection.py variables census.json samples.csv   # per-variable codec sweep
    python archive_projection.py project census.json samples.csv > projection.json

`measure` takes the largest dataset families of the census (archive_census.py), downloads two
typical files of each from SPDF and runs the end_to_end.py round trip on them, plus an estimate of
blosc2 with the transposed layout of particles.py. It appends to samples.csv and skips files
already there, so it can be interrupted. `project` scales each family's archive bytes by its
samples' size ratio; families without samples get the mean ratio over sampled families.
`variables` compresses every variable of the samples with each codec and filter (samples.vars.csv),
so `project` can also report a writer choosing the best codec per variable.
"""
import csv
import json
import sys
from collections import defaultdict
from math import prod
from pathlib import Path

import blosc2

from archive_census import CACHE_DIR, download, filelist, pick_samples
from end_to_end import FIELDS, bench_file
from particles import blosc2_size, cdfpp_typesize, is_distribution, transposed
from sweep import CODECS, raw_variables
from sweep import measure as measure_codec

SPDF_DATA = "https://spdf.gsfc.nasa.gov/pub/data/"
TOP_FAMILIES = 40
MAX_SAMPLE_BYTES = 1536 * 2**20
BLOCK_BYTES = 256 * 2**20  # stays under blosc2's 2 GiB buffer limit, like CDFpp's capped VVRs
TRANSPOSED = "blosc2_transposed_estimate"
VARIABLE_CODECS = ("gzip6", "zstd3", "blosc2-zstd5-shuffle-record", "blosc2-zstd5-noshuffle",
                   "blosc2-zstd5-bitshuffle")
VAR_FIELDS = ["source", "file", "variable", "raw_bytes", "config", "compressed_bytes", "compress_s", "decompress_s"]
BLOSC2_FILTERS = ("blosc2-zstd5-shuffle-record", "blosc2-zstd5-noshuffle", "blosc2-zstd5-bitshuffle")
# Writer strategies: the best config per variable among these (raw storage is always allowed).
# The gzip_or_blosc2 ones are what the spec allows with one new cType, then with a transposition flag.
STRATEGIES = {
    "best_gzip_or_raw": ("gzip6",),
    "best_zstd_or_raw": ("zstd3",),
    "blosc2_shipped": ("blosc2-zstd5-shuffle-record",),
    "best_blosc2_filter": BLOSC2_FILTERS,
    "best_blosc2_filter_or_transposed": BLOSC2_FILTERS + ("blosc2-transposed",),
    "best_gzip_or_blosc2": ("gzip6",) + BLOSC2_FILTERS,
    "best_gzip_or_blosc2_or_transposed": ("gzip6",) + BLOSC2_FILTERS + ("blosc2-transposed",),
    "best_of_all": ("gzip6", "zstd3") + BLOSC2_FILTERS + ("blosc2-transposed",),
}


def record_blocks(raw: bytes, record_bytes: int):
    step = max(1, BLOCK_BYTES // record_bytes) * record_bytes
    return (raw[i:i + step] for i in range(0, len(raw), step))


def transposed_saving(path: Path) -> int:
    """Bytes blosc2 saves when distribution variables are stored transposed ([N, K] -> [K, N])."""
    saving = 0
    for _, _, raw, item_size, record_shape in raw_variables(path):
        if not is_distribution(record_shape, item_size):
            continue
        k = prod(record_shape)
        for block in record_blocks(raw, k * item_size):
            n = len(block) // (k * item_size)
            as_shipped = blosc2_size(block, cdfpp_typesize(k * item_size, item_size), [blosc2.Filter.SHUFFLE])
            saving += max(0, as_shipped - blosc2_size(transposed(block, n, k, item_size), item_size,
                                                      [blosc2.Filter.SHUFFLE]))
    return saving


def with_transposed_estimate(rows: list) -> list:
    b2 = next(r for r in rows if r["codec"] == "blosc2_compression")
    return rows + [b2 | dict(codec=TRANSPOSED, bytes=b2["bytes"] - transposed_saving(CACHE_DIR / b2["path"]),
                             save_s="", load_s="")]


def top_families(census: dict, n: int = TOP_FAMILIES) -> list:
    return [name for name, _, _ in census["family"][:n]]


def samples(census: dict, picks_file: Path) -> dict:
    if not picks_file.exists():
        picks_file.write_text(json.dumps(pick_samples(filelist(), set(top_families(census)), MAX_SAMPLE_BYTES),
                                         indent=1))
    return json.loads(picks_file.read_text())


def measure(census: dict, output: Path):
    done = {r["file"] for r in csv.DictReader(output.open())} if output.exists() else set()
    fields = FIELDS + ["path"]
    with output.open("a", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fields, extrasaction="ignore")
        if not done:
            writer.writeheader()
        for family, paths in samples(census, output.with_suffix(".picks.json")).items():
            for path in paths:
                name = path.rsplit("/", 1)[-1]
                if name in done:
                    continue
                print(f"{family}: {name}", file=sys.stderr, flush=True)
                try:
                    local = download(SPDF_DATA + path, CACHE_DIR / "archive" / path)
                    rows = [r | dict(path=str(local.relative_to(CACHE_DIR))) for r in bench_file(family, local)]
                    writer.writerows(with_transposed_estimate(rows))
                    f.flush()
                except Exception as error:  # one broken file must not stop an hours-long run
                    print(f"  skipped: {error}", file=sys.stderr, flush=True)


def transposed_codec(raw: bytes, item_size: int, record_bytes: int):
    block = transposed(raw, len(raw) // record_bytes, record_bytes // item_size, item_size)
    cparams = blosc2.CParams(codec=blosc2.Codec.ZSTD, clevel=5, nthreads=1, typesize=item_size,
                             filters=[blosc2.Filter.SHUFFLE], filters_meta=[0])
    return blosc2.compress2(block, cparams=cparams), lambda c: transposed(
        blosc2.decompress2(c), record_bytes // item_size, len(raw) // record_bytes, item_size)


def variable_rows(family: str, path: Path):
    for name, _, raw, item_size, record_shape in raw_variables(path):
        record_bytes = prod(record_shape) * item_size
        codecs = {c: CODECS[c] for c in VARIABLE_CODECS}
        if is_distribution(record_shape, item_size):
            codecs["blosc2-transposed"] = transposed_codec
        for config, codec in codecs.items():
            size = c_s = d_s = 0
            for block in record_blocks(raw, record_bytes):
                b, c, d = measure_codec(codec, block, item_size, record_bytes)
                size, c_s, d_s = size + b, c_s + c, d_s + d
            yield dict(source=family, file=path.name, variable=name, raw_bytes=len(raw), config=config,
                       compressed_bytes=size, compress_s=f"{c_s:.6f}", decompress_s=f"{d_s:.6f}")


def sweep_variables(samples_file: Path):
    output = samples_file.with_suffix(".vars.csv")
    done = {r["file"] for r in csv.DictReader(output.open())} if output.exists() else set()
    files = {(r["source"], r["path"]) for r in csv.DictReader(samples_file.open())}
    with output.open("a", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=VAR_FIELDS)
        if not done:
            writer.writeheader()
        for family, path in sorted(files):
            if Path(path).name not in done:
                print(f"{family}: {Path(path).name}", file=sys.stderr, flush=True)
                writer.writerows(variable_rows(family, CACHE_DIR / path))
                f.flush()


def strategy_rows(var_rows: list, file_rows: list) -> list:
    """Per file, the bytes each writer strategy needs, as rows shaped like samples.csv rows."""
    sizes = defaultdict(dict)
    for r in var_rows:
        sizes[(r["file"], r["variable"])][r["config"]] = int(r["compressed_bytes"])
        sizes[(r["file"], r["variable"])]["raw"] = int(r["raw_bytes"])
    per_file = defaultdict(lambda: defaultdict(int))
    for (file, _), configs in sizes.items():
        for strategy, allowed in STRATEGIES.items():
            per_file[file][strategy] += min([configs["raw"]] + [configs[c] for c in allowed if c in configs])
    originals = {r["file"]: r for r in file_rows if r["codec"] == "no_compression"}
    return [originals[file] | dict(codec=strategy, bytes=size)
            for file, strategies in per_file.items() if file in originals for strategy, size in strategies.items()]


def ratios(rows) -> dict:
    """{family: {codec: codec bytes / original bytes}}, size-weighted over the family's samples."""
    sums = defaultdict(lambda: defaultdict(float))
    for r in rows:
        sums[r["source"]][r["codec"]] += int(r["bytes"])
        if r["codec"] == "no_compression":
            sums[r["source"]]["original"] += int(r["original_bytes"])
            sums[r["source"]]["uncompressed_originals"] += int(r["original_bytes"]) * (
                    r["original_compression"] == "no_compression")
    return {fam: {codec: size / s["original"] for codec, size in s.items()} for fam, s in sums.items()}


def project(census: dict, rows: list) -> dict:
    per_family = ratios(rows)
    # Unsampled families get the mean over families, not the byte-weighted pool, which the huge
    # MMS FPI burst families would dominate.
    pooled = {c: sum(r[c] for r in per_family.values()) / len(per_family) for c in next(iter(per_family.values()))}
    codecs = [c for c in pooled if c not in ("original", "uncompressed_originals")]
    families = []
    for name, files, size in census["family"]:
        ratio = per_family.get(name, pooled)
        families.append(dict(family=name, files=files, archive_bytes=size, sampled=name in per_family,
                             **{c: round(size * ratio[c]) for c in codecs + ["uncompressed_originals"]}))
    total = {c: sum(f[c] for f in families) for c in ["archive_bytes", "uncompressed_originals"] + codecs}
    sampled = {c: sum(f[c] for f in families if f["sampled"]) for c in ["archive_bytes"] + codecs}
    return dict(total=total, sampled_only=sampled, pooled_ratio=pooled,
                families=[f for f in families if f["sampled"]])


if __name__ == "__main__":
    command, census_file, samples_file = sys.argv[1:4]
    census = json.loads(Path(census_file).read_text())
    if command == "measure":
        measure(census, Path(samples_file))
    elif command == "variables":
        sweep_variables(Path(samples_file))
    else:
        rows = list(csv.DictReader(Path(samples_file).open()))
        var_file = Path(samples_file).with_suffix(".vars.csv")
        if var_file.exists():
            rows += strategy_rows(list(csv.DictReader(var_file.open())), rows)
        print(json.dumps(project(census, rows), indent=1))
