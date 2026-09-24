"""Census of the CDF files on SPDF, from its public file list (pub/catalogs/filelist.gz, ~1.2 GB).

Each line is "<ISO timestamp> GMT <bytes> <path>". Keeps the .cdf files under pub/data and writes
their size by mission, instrument and dataset directory, so codec gains measured on sample files
can be projected onto the whole archive. The file list is cached next to the corpus.
"""
import gzip
import json
import re
import sys
from collections import Counter, defaultdict
from pathlib import Path

import requests

from corpus import CACHE_DIR

FILELIST_URL = "https://spdf.gsfc.nasa.gov/pub/catalogs/filelist.gz"
YEAR_DIR = re.compile(r"^(19|20)\d\d$|^\d{2}$|^\d{4}\d{2}$")


SPACECRAFT = ((r"^mms/mms[1-4]/", "mms/mms*/"), (r"^cluster/c[1-4]/", "cluster/c*/"),
              (r"^rbsp/rbsp[ab]/", "rbsp/rbsp*/"), (r"^themis/th[a-e]/", "themis/th*/"),
              (r"^twins/twins[12]/", "twins/twins*/"), (r"^stereo/(ahead|behind)/", "stereo/*/"))


def download(url: str, dest: Path) -> Path:
    if not dest.exists():
        with requests.get(url, stream=True, timeout=600) as response:
            response.raise_for_status()
            dest.parent.mkdir(parents=True, exist_ok=True)
            partial = dest.with_suffix(".part")
            with partial.open("wb") as f:
                for block in response.iter_content(1 << 20):
                    f.write(block)
            partial.rename(dest)
    return dest


def filelist() -> Path:
    return download(FILELIST_URL, CACHE_DIR / "filelist.gz")


def family(dataset: str) -> str:
    """Same dataset on sibling spacecraft (mms1..4, c1..4, rbspa/b, ...) is one family."""
    for pattern, replacement in SPACECRAFT:
        dataset = re.sub(pattern, replacement, dataset)
    return dataset


def cdf_entries(path: Path):
    """(bytes, path parts under pub/data, modification year) for every .cdf file."""
    with gzip.open(path, "rt", errors="replace") as lines:
        for line in lines:
            fields = line.rstrip("\n").split(None, 3)
            if len(fields) == 4 and fields[3].startswith("pub/data/") and fields[3].lower().endswith(".cdf"):
                yield int(fields[2]), fields[3].split("/")[2:], int(fields[0][:4])


def dataset_dir(parts) -> str:
    """The directory holding a dataset: the file's directory without trailing year/month levels."""
    dirs = parts[:-1]
    while dirs and YEAR_DIR.match(dirs[-1]):
        dirs = dirs[:-1]
    return "/".join(dirs)


def census(path: Path) -> dict:
    by = {key: defaultdict(lambda: [0, 0]) for key in ("mission", "instrument", "family", "dataset")}
    by_year, biggest = Counter(), []
    for size, parts, year in cdf_entries(path):
        dataset = dataset_dir(parts)
        keys = dict(mission=parts[0], instrument="/".join(parts[:2]), family=family(dataset), dataset=dataset)
        for key, value in keys.items():
            by[key][value][0] += 1
            by[key][value][1] += size
        by_year[year] += size
        biggest.append((size, "/".join(parts)))
        if len(biggest) > 20000:
            biggest = sorted(biggest, reverse=True)[:200]
    ranked = {key: sorted(([k, n, b] for k, (n, b) in table.items()), key=lambda r: -r[2])
              for key, table in by.items()}
    total = [sum(r[1] for r in ranked["mission"]), sum(r[2] for r in ranked["mission"])]
    return dict(total_files=total[0], total_bytes=total[1], by_year=dict(sorted(by_year.items())),
                biggest_files=sorted(biggest, reverse=True)[:200], **ranked)


def median_file(files):
    return sorted(files)[len(files) // 2]


def pick_samples(path: Path, families, max_bytes: int) -> dict:
    """Two typical files per family: the median-sized one in each half of the family's file list
    (sorted by path, so the halves are different spacecraft or years)."""
    files = defaultdict(list)
    for size, parts, _ in cdf_entries(path):
        key = family(dataset_dir(parts))
        if key in families and 0 < size <= max_bytes:
            files[key].append(("/".join(parts), size))
    picks = {}
    for key, entries in files.items():
        entries.sort()
        halves = (entries[: len(entries) // 2], entries[len(entries) // 2:])
        chosen = {median_file([(size, p) for p, size in half]) for half in halves if half}
        picks[key] = sorted(p for _, p in chosen)
    return picks


if __name__ == "__main__":
    output = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("census.json")
    output.write_text(json.dumps(census(filelist()), indent=0))
