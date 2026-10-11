"""Writes dist_4d.cdf: a tiny particle distribution (3 records x 2 phi x 3 theta x 4 energy)
with ISTP DEPEND_1..3 and one FILLVAL cell, for the Explorer's reduction tests.
dist[r, p, t, e] = 1000*r + 100*p + 10*t + e, except dist[0, 0, 0, 0] which is FILLVAL.

    pip install pycdfpp numpy
    python make_dist_cdf.py
"""
import os
from datetime import datetime, timedelta

import numpy as np
import pycdfpp

PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), "dist_4d.cdf")
FILL = np.float32(-1e31)

r, p, t, e = np.meshgrid(np.arange(3), np.arange(2), np.arange(3), np.arange(4), indexing="ij")
dist = (1000 * r + 100 * p + 10 * t + e).astype(np.float32)
dist[0, 0, 0, 0] = FILL

cdf = pycdfpp.CDF()
cdf.add_variable("Epoch", values=[datetime(2020, 1, 1) + timedelta(seconds=i) for i in range(3)],
                 data_type=pycdfpp.DataType.CDF_TIME_TT2000)
cdf.add_variable("phi", values=np.array([0., 180.], dtype=np.float32), is_nrv=True)
cdf.add_variable("theta", values=np.array([30., 90., 150.], dtype=np.float32), is_nrv=True)
cdf.add_variable("energy", values=np.array([10., 100., 1000., 10000.], dtype=np.float32), is_nrv=True)
for name in ("phi", "theta", "energy"):
    cdf[name].add_attribute("VAR_TYPE", "support_data")
cdf["energy"].add_attribute("SCALETYP", "log")
cdf["energy"].add_attribute("UNITS", "eV")
cdf.add_variable("dist", values=dist)
for name, value in (("VAR_TYPE", "data"), ("DEPEND_0", "Epoch"), ("DEPEND_1", "phi"),
                    ("DEPEND_2", "theta"), ("DEPEND_3", "energy"), ("DISPLAY_TYPE", "spectrogram")):
    cdf["dist"].add_attribute(name, value)
cdf["dist"].add_attribute("FILLVAL", np.array([FILL], dtype=np.float32))

pycdfpp.save(cdf, PATH)
