"""Writes time_special_values.cdf with NASA's CDF library (bundled in spacepy's wheel): one
variable per CDF time type holding fill, pad and NaN values next to ordinary dates, 16 records
each so that SIMD conversions see them too.

    pip install spacepy numpy
    python make_time_special_values_cdf.py
"""
import os

import numpy as np
from spacepy import pycdf

PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), "time_special_values.cdf")
RECORDS = 16

EPOCH = [-1e31, 0.0, np.nan, 63745056000068.5, 62167219200000.0, 62040988800123.25,
         np.inf, -np.inf]
EPOCH16 = [(-1e31, -1e31), (0.0, 0.0), (63745056000.0, 68500000123.0),
           (62167219200.0, 0.0), (62040988800.0, 123456789012.0), (np.nan, 0.0)]
TT2000 = [-9223372036854775808, -9223372036854775807, 631108869184000000, -946727959816000000,
          -1041335958816000000, 0]


def repeated(values):
    return (values * RECORDS)[:RECORDS]


if os.path.exists(PATH):
    os.remove(PATH)
with pycdf.CDF(PATH, "") as cdf:
    cdf.new("epoch", type=pycdf.const.CDF_EPOCH, recVary=True)
    cdf.raw_var("epoch")[...] = np.array(repeated(EPOCH))
    cdf.new("epoch16", type=pycdf.const.CDF_EPOCH16, recVary=True)
    cdf.raw_var("epoch16")[...] = np.array(repeated(EPOCH16))
    cdf.new("tt2000", type=pycdf.const.CDF_TIME_TT2000, recVary=True)
    cdf.raw_var("tt2000")[...] = np.array(repeated(TT2000), dtype=np.int64)
