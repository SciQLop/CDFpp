#!env python3
from cdflib import cdfwrite, cdfepoch
from datetime import datetime, timedelta
import numpy as np
import math
import os


cd = cdfwrite.CDF("cd_with_time.cdf")
cd.write_globalattrs( {
    "epoch":{0:[cdflib.cdfepoch.compute_epoch([0,1,1,0,0,0,0,0,0,0]),'cdf_epoch']},
    "epoch16":{0:[
        cdflib.cdfepoch.compute_epoch16([0,1,1,0,0,0,0,0,0,0]),
        cdflib.cdfepoch.compute_epoch16([0,1,2,0,0,0,0,0,0,0]),
        cdflib.cdfepoch.compute_epoch16([0,1,1,0,0,0,0,0,0,1]),
        'cdf_epoch16'
        ]},
    "tt2000":{0:["0",'cdf_tt2000']}
    } )
