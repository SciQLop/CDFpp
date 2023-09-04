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
    cdf.add_variable("utf8", ['ASCII: ABCDEFG', 'Latin1: ©æêü÷Æ¼®¢¥', 'Chinese: 社安', 'Other: ႡႢႣႤႥႦ'])
    cdf.add_variable("utf8_arr", np.array(['ASCII: ABCDEFG', 'Latin1: ©æêü÷Æ¼®¢¥', 'Chinese: 社安', 'Other: ႡႢႣႤႥႦ']))
    cdf.add_variable("test_CDF_TIME_TT2000").set_values(np.arange(1e18,11e17,1e16, dtype=np.int64).astype("datetime64[ns]"), pycdfpp.DataType.CDF_TIME_TT2000)
    cdf.add_variable("test_CDF_TIME_TT2000_2").set_values(np.arange(1e12,11e11,1e10, dtype=np.int64).astype("datetime64[ms]"), pycdfpp.DataType.CDF_TIME_TT2000)
    return cdf

class PycdfCreateCDFTest(unittest.TestCase):
    def test_can_create_an_empty_CDF_object(self):
        cdf = pycdfpp.CDF()
        self.assertIsNotNone(cdf)

    def test_compare_identical_cdfs(self):
        self.assertEqual(make_cdf(),make_cdf())
        self.assertEqual(pycdfpp.CDF(),pycdfpp.CDF())

    def test_compare_differents_cdfs(self):
        self.assertEqual(make_cdf(),pycdfpp.CDF())

    def test_inmemory_save_load_empty_CDF_object(self):
        cdf = pycdfpp.CDF()
        self.assertIsNotNone(pycdfpp.load(pycdfpp.save(cdf)))

    def test_can_create_CDF_attributes(self):
        cdf = pycdfpp.CDF()
        cdf.add_attribute("test_attribute", [[1,2,3], [datetime(2018,1,1), datetime(2018,1,2)], "hello\nworld"])

    def test_can_create_variable_and_attributes_at_once(self):
        cdf = pycdfpp.CDF()
        cdf.add_variable("test_variable", attributes={"attr1":[1,2,3], "attr2":[datetime(2018,1,1), datetime(2018,1,2)] })
        self.assertListEqual(cdf["test_variable"].attributes["attr1"][0], [1,2,3])
        self.assertListEqual(cdf["test_variable"].attributes["attr2"][0], [pycdfpp.to_tt2000(datetime(2018,1,1)), pycdfpp.to_tt2000(datetime(2018,1,2))])

    def test_can_create_an_empty_variable(self):
        cdf = pycdfpp.CDF()
        cdf.add_variable("test_variable", values=np.ones((0,100,10), dtype=np.float32))
        self.assertEqual(cdf["test_variable"].shape, (0,100,10))

    def test_can_create_an_nd_string_variable(self):
        cdf = pycdfpp.CDF()
        cdf.add_variable("test_1d_variable", values=["hello", "world"])
        cdf.add_variable("test_2d_variable", values=[["hello", "world"], ["12345", "67890"]])
        cdf.add_variable("utf8", ['ASCII: ABCDEFG', 'Latin1: ©æêü÷Æ¼®¢¥', 'Chinese: 社安', 'Other: ႡႢႣႤႥႦ'])
        cdf.add_variable("utf8_arr", np.array(['ASCII: ABCDEFG', 'Latin1: ©æêü÷Æ¼®¢¥', 'Chinese: 社安', 'Other: ႡႢႣႤႥႦ']))
        self.assertTrue(pycdfpp.load(pycdfpp.save(cdf)) == cdf)

    def test_can_save_an_empty_CDF_object(self):
        with NamedTemporaryFile() as f:
            self.assertTrue(pycdfpp.save(pycdfpp.CDF(),f.name))

    def test_can_save_a_CDF_object_with_several_numeric_1d_variables(self):
        with NamedTemporaryFile() as f:
            cdf = pycdfpp.CDF()
            for dtype in (np.float64, np.float32, np.int8, np.int16, np.int32, np.int64, np.uint8, np.uint16, np.uint32):
                cdf.add_variable(f"test_{dtype}", np.ones((10),dtype=dtype))
            self.assertTrue(pycdfpp.save(cdf,f.name))

    def test_can_save_a_CDF_object_with_several_numeric_nd_variables(self):
        with NamedTemporaryFile() as f:
            cdf = pycdfpp.CDF()
            for dtype in (np.float64, np.float32, np.int8, np.int16, np.int32, np.int64, np.uint8, np.uint16, np.uint32):
                cdf.add_variable(f"test_{dtype}").set_values(np.ones((3,2,1),dtype=dtype))
            self.assertTrue(pycdfpp.save(cdf,f.name))

    def test_can_save_a_CDF_object_with_several_datetime_variables(self):
        with NamedTemporaryFile() as f:
            cdf = pycdfpp.CDF()
            for cdf_type in (pycdfpp.DataType.CDF_TIME_TT2000, pycdfpp.DataType.CDF_EPOCH, pycdfpp.DataType.CDF_EPOCH16):
                cdf.add_variable(f"test_{cdf_type}").set_values(np.arange(1e18,11e17,1e16, dtype=np.int64).astype("datetime64[ns]"), cdf_type)
            self.assertTrue(pycdfpp.save(cdf,f.name))

    def test_can_save_a_GZ_compressed_CDF(self):
        with NamedTemporaryFile() as f:
            cdf = pycdfpp.CDF()
            for dtype in (np.float64, np.float32, np.int8, np.int16, np.int32, np.int64, np.uint8, np.uint16, np.uint32):
                cdf.add_variable(f"test_{dtype}").set_values(np.ones((3,2,1),dtype=dtype))
            cdf.compression = pycdfpp.CompressionType.gzip_compression
            self.assertTrue(pycdfpp.save(cdf,f.name))

if __name__ == '__main__':
    unittest.main()
