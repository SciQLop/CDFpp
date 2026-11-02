#!/usr/bin/env python
"""Measures pycdfpp remote reads on NASA's public HelioCloud bucket.

For each transport and workload it reports wall time, requests and bytes fetched, against a
full download followed by a local load. It is not part of the test suite: it needs network
access, s3fs and obstore.

    PYTHONPATH=build uv run --no-project --with s3fs --with obstore --with numpy \\
        python benchmarks/remote/heliocloud.py
"""
import os
import tempfile
import time
import urllib.request

import obstore
import pycdfpp
import s3fs
from obstore.store import S3Store

BUCKET = "gov-nasa-hdrl-data1"
KEY = "spdf/cdaweb/data/mms/mms1/fgm/srvy/l2/2020/01/mms1_fgm_srvy_l2_20200101_v5.224.0.cdf"
URL = f"https://{BUCKET}.s3.amazonaws.com/{KEY}"
KiB = 1024


class Counting:
    def __init__(self):
        self.batches = 0
        self.requests = 0
        self.bytes = 0

    def count(self, ranges):
        self.batches += 1
        self.requests += len(ranges)
        self.bytes += sum(size for _, size in ranges)


class FsspecReader(Counting):
    def __init__(self, fs, path):
        super().__init__()
        self.fs, self.path = fs, path

    def size(self):
        return self.fs.size(self.path)

    def read_ranges(self, ranges):
        self.count(ranges)
        return self.fs.cat_ranges([self.path] * len(ranges), [o for o, _ in ranges],
                                  [o + s for o, s in ranges])


class ObstoreReader(Counting):
    def __init__(self, store, path):
        super().__init__()
        self.store, self.path = store, path

    def size(self):
        return obstore.head(self.store, self.path)["size"]

    def read_ranges(self, ranges):
        self.count(ranges)
        return obstore.get_ranges(self.store, self.path, starts=[o for o, _ in ranges],
                                  ends=[o + s for o, s in ranges])


class LocalReader(Counting):
    def __init__(self, path):
        super().__init__()
        with open(path, "rb") as f:
            self.data = f.read()

    def size(self):
        return len(self.data)

    def read_ranges(self, ranges):
        self.count(ranges)
        return [self.data[o:o + s] for o, s in ranges]


def round_trip_time():
    request = urllib.request.Request(URL, headers={"Range": "bytes=0-0"})
    start = time.perf_counter()
    with urllib.request.urlopen(request) as response:
        response.read()
    return time.perf_counter() - start


def variable_names(cdf, count):
    names = [name for name, var in cdf.items() if var.shape and var.shape[0] > 1]
    return names if count is None else names[:count]


def run_workload(cdf, count):
    for name in [] if count == 0 else variable_names(cdf, count):
        cdf[name].values


WORKLOADS = {"open": 0, "1 var": 1, "3 vars": 3, "all vars": None}


def timed(make_reader, count, **knobs):
    reader = make_reader()
    start = time.perf_counter()
    run_workload(pycdfpp.load(reader, **knobs), count)
    return time.perf_counter() - start, reader.requests, reader.bytes, reader.batches


def full_download(directory):
    path = os.path.join(directory, "full.cdf")
    start = time.perf_counter()
    urllib.request.urlretrieve(URL, path)
    return path, time.perf_counter() - start


def print_row(arm, workload, seconds, requests, nbytes, batches=1):
    print(f"| {arm:<34} | {workload:<8} | {seconds:8.2f} | {batches:7} | {requests:8} "
          f"| {nbytes / 1e6:9.2f} |")


def main():
    fs = s3fs.S3FileSystem(anon=True)
    store = S3Store(BUCKET, region="us-east-1", skip_signature=True)
    path = f"{BUCKET}/{KEY}"
    rtt = round_trip_time()
    with tempfile.TemporaryDirectory(dir="build") as directory:
        local_path, download_seconds = full_download(directory)
        size = os.path.getsize(local_path)
        print(f"file: {KEY} ({size / 1e6:.1f} MB), round trip {rtt * 1000:.0f} ms, "
              f"full download {download_seconds:.1f} s")
        print("| arm                                | workload |   wall s | batches | requests "
              "|   MB read |")
        print("|------------------------------------|----------|----------|---------|----------"
              "|-----------|")
        arms = {
            "fsspec (s3fs)": lambda: FsspecReader(fs, path),
            "obstore, bytes": lambda: ObstoreReader(store, KEY),
        }
        for workload, count in WORKLOADS.items():
            for arm, make_reader in arms.items():
                print_row(arm, workload, *timed(make_reader, count))
            seconds, requests, nbytes, batches = timed(lambda: LocalReader(local_path), count)
            print_row("local reader + RTT x batches", workload, seconds + rtt * batches,
                      requests, nbytes, batches)
            start = time.perf_counter()
            run_workload(pycdfpp.load(local_path), count)
            print_row("full download + local load", workload,
                      download_seconds + time.perf_counter() - start, 1, size)
        print()
        print("read_ahead sweep (fsspec):")
        for read_ahead in (64 * KiB, 256 * KiB, 1024 * KiB):
            for workload, count in WORKLOADS.items():
                print_row(f"fsspec, read_ahead={read_ahead // KiB} KiB", workload,
                          *timed(lambda: FsspecReader(fs, path), count, read_ahead=read_ahead))


if __name__ == "__main__":
    main()
