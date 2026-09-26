"""Writes many_compressed_blocks.cdf with NASA's CDF library (bundled in spacepy's wheel): one
gzip variable split into 1000 compressed blocks, as mission archives do. Values come in runs of
5 (250-record blocks hold 50 runs), so every block ends in a repeated sequence and the next one
starts with a new value: libdeflate copies repeats in wide chunks and can write past the end of
a block, which then shows. Each run's value is a hash of its index, so misplaced blocks show too.

    pip install spacepy numpy
    python make_many_blocks_cdf.py
"""
import os

import numpy as np
from spacepy import pycdf

PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), "many_compressed_blocks.cdf")
RECORDS = 250_000
RECORDS_PER_BLOCK = 250

if os.path.exists(PATH):
    os.remove(PATH)
with pycdf.CDF(PATH, "") as cdf:
    var = cdf.new("values", type=pycdf.const.CDF_INT4, compress=pycdf.const.GZIP_COMPRESSION)
    var._call(pycdf.const.PUT_, pycdf.const.zVAR_BLOCKINGFACTOR_, RECORDS_PER_BLOCK)
    index = np.arange(RECORDS, dtype=np.uint64)
    var[...] = (((index // 5) * 2654435761) % 65521).astype(np.int32)
