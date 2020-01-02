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
cd["var"].attrs["var_attr"] = "a variable attribute"
cd["var"].attrs["DEPEND0"] = "epoch"
cd["epoch"] = [datetime(2019,10,1)+timedelta(seconds=5*i) for i in range(len(cd["var"]))]
cd["epoch"].attrs["epoch_attr"] = "a variable attribute"
cd["var2d"] = np.ones((3,4))
cd["var3d"] = np.ones((4,3,2))
cd["var3d"].attrs["var3d_attr_multi"] = [10,11]
cd.attrs["attr"] = "a cdf text attribute"
cd.attrs["attr_float"] = [[1.,2.,3.],[4.,5.,6.]]
cd.attrs["attr_int"] = [[1,2,3]]
cd.attrs["attr_multi"] = [[1, 2],[2.,3.],"hello"]
cd.attrs["empty"] = []
for vname,var in cd.items():
    print(vname,":",var)
    for aname,attr in var.attrs.items():
        print("\t",aname,":",attr)
for aname,attr in cd.attrs.items():
    print(aname,":")
    i=0
    for val in attr:
        print("\t",pycdf.lib.cdftypenames[attr.type(i)],val)
        i+=1
cd.close()
