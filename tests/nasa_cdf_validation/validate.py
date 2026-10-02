#!/usr/bin/env python3
"""Resaves real CDF fixtures through CDFpp, then runs the NASA CDF reference
library's cdfvalidate tool against the output. Catches any regression where
CDFpp's own writer produces bytes the reference implementation doesn't
consider structurally valid -- a load()+compare round-trip through CDFpp's
own reader cannot catch this (both sides would agree with each other while
still disagreeing with the spec).

By default validates the same 4 local fixtures tests/simple_save's round-trip
SCENARIO uses, plus a few files built from scratch for writer features those
fixtures don't use. Pass --with-corpus to additionally fetch and validate the full
remote corpus tests/full_corpus/test.py already round-trips through CDFpp's
own reader (33 real files from many different missions/instruments) -- reuses
that file directly rather than duplicating its file list, so the corpus stays
a single source of truth. Needs network access; never run this mode locally
as part of routine testing (same reasoning as tests/full_corpus itself).

Requires cdfvalidate to be built (not necessarily installed) somewhere: pass its
path via CDFVALIDATE, and make sure its runtime library is findable (typically
via LD_LIBRARY_PATH pointing at the NASA CDF distribution's lib/ or src/lib/).
"""
import os
import subprocess
import sys
import tempfile

import numpy as np
import pycdfpp

FIXTURES = [
    "a_cdf.cdf",
    "a_compressed_cdf.cdf",
    "a_rle_compressed_cdf.cdf",
    "a_cdf_with_compressed_vars.cdf",
]


def _local_fixtures(data_path):
    for name in FIXTURES:
        yield name, pycdfpp.load(os.path.join(data_path, name))


def _built_fixtures():
    """Files built from scratch, for the writer features the fixtures above don't use."""
    empty_attribute = pycdfpp.CDF()
    empty_attribute.add_attribute("EMPTY", [])
    empty_attribute.declared_variable_attributes = ["UNUSED", "UNITS"]
    empty_attribute.add_variable("x", np.arange(3.), attributes={"UNITS": "nT"})
    yield "built_empty_attribute.cdf", empty_attribute
    sparse_records = pycdfpp.CDF()
    for flag in pycdfpp.SparseRecords:
        sparse_records.add_variable(flag.name, np.arange(10.), sparse_records=flag)
    yield "built_sparse_records.cdf", sparse_records
    pad_values = pycdfpp.CDF()
    pad_values.add_variable("float", np.empty((0, 3), dtype=np.float32), pad_value=np.float32(-1e-30))
    pad_values.add_variable("int", np.arange(10, dtype=np.int16), pad_value=-7)
    pad_values.add_variable("text", ["abc", "def"], pad_value="xyz")
    yield "built_pad_values.cdf", pad_values
    for majority in pycdfpp.Majority:
        for encoding in (pycdfpp.Encoding.network, pycdfpp.Encoding.IBMPC):
            layout = pycdfpp.CDF()
            layout.majority, layout.encoding = majority, encoding
            layout.checksum = pycdfpp.Checksum.md5_checksum
            layout.add_attribute("numbers", [[1, 2, 3], [1.5]])
            layout.add_variable("cube", np.arange(60, dtype=np.float32).reshape(3, 4, 5),
                                attributes={"VALIDMIN": [np.float32(0)]}, pad_value=np.float32(-1))
            yield f"built_{majority.name}_{encoding.name}_md5.cdf", layout


def _corpus_fixtures():
    sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "full_corpus"))
    import test as full_corpus  # tests/full_corpus/test.py
    for name, _variables, _attrs in full_corpus.files:
        yield name, full_corpus.fetch_cdf(name)


def _validate_one(name, cdf, cdfvalidate, out_dir, failures):
    if cdf is None:
        failures.append(f"{name}: CDFpp failed to load")
        return
    out_path = os.path.join(out_dir, f"resaved_{name}")
    if not pycdfpp.save(cdf, out_path):
        failures.append(f"{name}: CDFpp failed to save")
        return
    result = subprocess.run([cdfvalidate, out_path], capture_output=True, text=True)
    print(f"--- {name} ---")
    print(result.stdout.strip())
    if result.returncode != 0:
        failures.append(f"{name}: cdfvalidate exit {result.returncode}\n{result.stderr}")


def main():
    data_path = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
        os.path.dirname(__file__), "..", "resources")
    include_corpus = "--with-corpus" in sys.argv
    cdfvalidate = os.environ.get("CDFVALIDATE", "cdfvalidate")

    failures = []
    checked = 0
    with tempfile.TemporaryDirectory() as out_dir:
        for name, cdf in [*_local_fixtures(data_path), *_built_fixtures()]:
            _validate_one(name, cdf, cdfvalidate, out_dir, failures)
            checked += 1
        if include_corpus:
            for name, cdf in _corpus_fixtures():
                _validate_one(name, cdf, cdfvalidate, out_dir, failures)
                checked += 1

    if failures:
        print("\nFAILURES:")
        for f in failures:
            print(" -", f)
        sys.exit(1)
    print(f"\nAll {checked} CDFpp-saved fixtures passed NASA cdfvalidate.")


if __name__ == "__main__":
    main()
