#!env python3
from spacepy import pycdf
import os

if os.path.exists("a_cdf.cdf"):
    os.unlink("a_cdf.cdf")
cd = pycdf.CDF("a_cdf.cdf",'')
cd["var"] = [1.,2.,3.]
cd.attrs["attr"] = "a cdf text attribute"
cd.attrs["attr_double"] = [1.,2.,3.]
cd.close()
