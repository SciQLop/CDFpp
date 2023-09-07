#!/usr/bin/env python
# -*- coding: utf-8 -*-
import os
from datetime import datetime, timedelta
from tempfile import NamedTemporaryFile
import numpy as np
import math
import unittest
from glob import glob
import pycdfpp

os.environ['TZ'] = 'UTC'

def make_cdf():
    cdf = pycdfpp.CDF()
    cdf.add_attribute("test_attribute", [[1,2,3], [datetime(2018,1,1), datetime(2018,1,2)], "hello\nworld"])
    cdf.add_variable("test_variable", attributes={"attr1":[1,2,3], "attr2":[datetime(2018,1,1), datetime(2018,1,2)] })
    cdf.add_attribute("tt2000_special_values", [[pycdfpp.tt2000_t(-9223372036854775808),pycdfpp.tt2000_t(-9223372036854775807),pycdfpp.tt2000_t(-9223372036854775805)]])
    cdf.add_variable("test_CDF_TIME_TT2000").set_values(np.empty((0,1), dtype=np.int64).astype("datetime64[ns]"), pycdfpp.DataType.CDF_TIME_TT2000)
    return cdf

class PycdfToSkeletonTest(unittest.TestCase):
    def test_can_extrac_skeleton(self):
        cdf = make_cdf()
        skel = pycdfpp.to_dict_skeleton(cdf)
        self.assertIsNotNone(skel)
        self.assertEqual(type(skel["attributes"]["tt2000_special_values"]["values"][0][0]), str)


if __name__ == '__main__':
    unittest.main()
