#!env python3
from spacepy import pycdf
from datetime import datetime, timedelta
import numpy as np
import math
import os


def print_attr(attr):
    print(attr._name.decode(),":")
    i=0
    for val in attr:
        print("\t",pycdf.lib.cdftypenames[attr.type(i)],val)
        i+=1

def print_attrs(cd):
    for _,attr in cd.attrs.items():
        print_attr(attr)

def print_vars(cd):
    for vname,var in cd.items():
        print(vname,":",var)
        for aname,attr in var.attrs.items():
            print("\t",aname,":",attr)

def print_cdf(cd):
    print_vars(cd)
    print_attrs(cd)

def make_var(cdf_file, name, compress, values):
    if compress:
        cdf_file.new(name, data=values, compress=pycdf.const.GZIP_COMPRESSION, compress_param=9)
    else:
        cdf_file.new(name, data=values)

def add_varaibles(cd, compress=False):
    l=100
    for name,values in [('var', np.cos(np.arange(0.,(l+1)/l*2.*math.pi,2.*math.pi/l))),
                          ('epoch', [datetime(2019,10,1)+timedelta(seconds=5*i) for i in range(l+1)]),
                          ('var2d', np.ones((3,4))),
                          ('var3d', np.ones((4,3,2))),
                          ('var2d_counter', np.arange(100, dtype=np.float64).reshape(10,10))]:
        make_var(cd, name=name, compress=compress, values=values)

    cd["var"].attrs["var_attr"] = "a variable attribute"
    cd["var"].attrs["DEPEND0"] = "epoch"
    cd["epoch"].attrs["epoch_attr"] = "a variable attribute"
    cd["var3d"].attrs["var3d_attr_multi"] = [10,11]

def add_attributes(cd):
    cd.attrs["attr"] = "a cdf text attribute"
    cd.attrs["attr_float"] = [[1.,2.,3.],[4.,5.,6.]]
    cd.attrs["attr_int"] = [[1,2,3]]
    cd.attrs["attr_multi"] = [[1, 2],[2.,3.],"hello"]
    cd.attrs["empty"] = []


def make_cdf(name, compress_file=False, compress_var=False, compression_algo=pycdf.const.GZIP_COMPRESSION):
    if os.path.exists(name):
        os.unlink(name)
    cd = pycdf.CDF(name,'')
    add_varaibles(cd, compress=compress_var)
    add_attributes(cd)
    print_cdf(cd)
    if compress_file:
        cd.compress(pycdf.const.GZIP_COMPRESSION)
    cd.close()

make_cdf("a_cdf.cdf")
make_cdf("a_compressed_cdf.cdf", compress_file=True)
make_cdf("a_cdf_with_compressed_vars.cdf", compress_var=True)
make_cdf("a_rle_compressed_cdf.cdf", compress_file=True, compression_algo=pycdf.const.RLE_COMPRESSION)
