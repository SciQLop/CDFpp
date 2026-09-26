"""Writes checksum.cdf with NASA's CDF library (bundled in spacepy's wheel): a small file whose
CDR has the MD5 checksum flag set, so the last 16 bytes are the checksum, not a record.

    pip install spacepy numpy
    python make_checksum_cdf.py
"""
import os

import numpy as np
from spacepy import pycdf

PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), "checksum.cdf")

if os.path.exists(PATH):
    os.remove(PATH)
with pycdf.CDF(PATH, "") as cdf:
    cdf.checksum(True)
    cdf["values"] = np.arange(10, dtype=np.float64)
