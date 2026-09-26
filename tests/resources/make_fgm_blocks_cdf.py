"""Writes fgm_blocks.cdf with NASA's CDF library (bundled in spacepy's wheel): the first 12288
records of MMS1 FGM survey B GSE (2019-12-31), gzip, 3 blocks of 4096 records like the archive
file. Decompressing its middle block with libdeflate given the rest of the array as output space
writes past the block's end: real data reproduces what synthetic data didn't.

    pip install spacepy numpy pycdfpp requests
    python make_fgm_blocks_cdf.py
"""
import os
import sys

import pycdfpp
from spacepy import pycdf

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..",
                                "benchmarks", "compression"))
from corpus import cdaweb_file  # noqa: E402

PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fgm_blocks.cdf")
RECORDS = 3 * 4096

source = pycdfpp.load(str(cdaweb_file("MMS1_FGM_SRVY_L2", "2020-01-01")))
b_gse = source["mms1_fgm_b_gse_srvy_l2"].values[:RECORDS]

if os.path.exists(PATH):
    os.remove(PATH)
with pycdf.CDF(PATH, "") as cdf:
    var = cdf.new("b_gse", type=pycdf.const.CDF_REAL4, dims=[4],
                  compress=pycdf.const.GZIP_COMPRESSION)
    var._call(pycdf.const.PUT_, pycdf.const.zVAR_BLOCKINGFACTOR_, 4096)
    var[...] = b_gse
