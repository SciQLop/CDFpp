#!env python3
from spacepy import pycdf
from datetime import datetime, timedelta
import numpy as np
import math
import os

if os.path.exists("a_cdf.cdf"):
    os.unlink("a_cdf.cdf")
cd = pycdf.CDF("a_cdf.cdf",'')
l=100
cd["var"] = np.cos(np.arange(0.,(l+1)/l*2.*math.pi,2.*math.pi/l))
print(len(cd["var"][:]))
cd["var"].attrs["var_attr"] = "a variable attribute"
cd["var"].attrs["DEPEND0"] = "epoch"
cd["epoch"] = [datetime(2019,10,1)+timedelta(seconds=5*i) for i in range(len(cd["var"]))]
cd["epoch"].attrs["epoch_attr"] = "a variable attribute"
cd["var2d"] = np.ones((3,4))
cd["var3d"] = np.ones((4,3,2))
cd["var3d"].attrs["var3d_attr_multi"] = [10,11]
print(cd["var3d"].attrs["var3d_attr_multi"])
cd.attrs["attr"] = "a cdf text attribute"
cd.attrs["attr_float"] = [[1.,2.,3.],[4.,5.,6.]]
cd.attrs["attr_int"] = [[1,2,3]]
cd.attrs["attr_multi"] = [[1, 2],[2.,3.],"hello"]
cd.close()
