"""Writes col_major_strings.cdf with NASA's CDF library (bundled in spacepy's wheel): a column
major file with record varying 2D arrays, of strings and of numbers. Each record is stored in
column major order on its own.

    pip install spacepy numpy
    python make_col_major_strings_cdf.py
"""
import os

import numpy as np
from spacepy import pycdf

PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), "col_major_strings.cdf")
STRINGS = [[[f"r{r}[{i}{j}]" for j in range(3)] for i in range(2)] for r in range(4)]

if os.path.exists(PATH):
    os.remove(PATH)
with pycdf.CDF(PATH, "") as cdf:
    cdf.col_major(True)
    cdf.new("strings", data=STRINGS, type=pycdf.const.CDF_CHAR, recVary=True)
    cdf.new("numbers", data=np.arange(4 * 2 * 3, dtype=np.float64).reshape(4, 2, 3), recVary=True)
