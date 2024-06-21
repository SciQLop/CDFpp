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


def make_datetime64_values():
    return np.arange(1e18, 11e17, 1e16, dtype=np.int64).astype("datetime64[ns]")


def make_datetime_values():
    return [ datetime(2000, 1, 1, 12, 0,5,microsecond=10000) + timedelta(seconds=i) for i in range(100) ]


class PycdfVariableSetValues(unittest.TestCase):
    def test_can_create_an_empty_CDF_Variable(self):
        cdf = pycdfpp.CDF()
        cdf.add_variable("Empty_variable")
        self.assertIn("Empty_variable", cdf)
        self.assertEqual(cdf["Empty_variable"].compression,
                         pycdfpp.CompressionType.no_compression)
        self.assertEqual(cdf["Empty_variable"].shape, tuple())
        self.assertEqual(cdf["Empty_variable"].is_nrv, False)

    def test_can_create_a_CDF_Variable_with_values(self):
        cdf = pycdfpp.CDF()
        values = np.array([1, 2, 3, 4], dtype=np.float64)
        cdf.add_variable("variable", values=values)
        self.assertIn("variable", cdf)
        self.assertEqual(cdf["variable"].compression,
                         pycdfpp.CompressionType.no_compression)
        self.assertEqual(cdf["variable"].shape, (4,))
        self.assertEqual(cdf["variable"].is_nrv, False)
        self.assertTrue(np.all(cdf["variable"].values == values))
        self.assertEqual(cdf["variable"].type, pycdfpp.DataType.CDF_DOUBLE)

    def test_setting_different_type_values_should_raise(self):
        cdf = pycdfpp.CDF()
        cdf.add_variable("variable", values=np.empty(
            shape=[0], dtype=np.float64))
        self.assertIn("variable", cdf)
        with self.assertRaises(ValueError):
            cdf["variable"].set_values(
                np.ones(shape=(100, 2), dtype=np.float32))

    def test_setting_compatible_type_values_should_not_raise(self):
        cdf = pycdfpp.CDF()
        cdf.add_variable("variable", values=np.empty(
            shape=[0], dtype=np.float64), data_type=pycdfpp.DataType.CDF_REAL8)
        self.assertIn("variable", cdf)
        cdf["variable"].set_values(np.ones(100, dtype=np.float64))

    def test_setting_different_shape_values_should_raise_1D(self):
        cdf = pycdfpp.CDF()
        cdf.add_variable("variable", values=np.empty(
            shape=[0], dtype=np.float64))
        self.assertIn("variable", cdf)
        with self.assertRaises(ValueError):
            cdf["variable"].set_values(
                np.ones(shape=(100, 2), dtype=np.float64))

    def test_setting_different_shape_values_should_raise_3D(self):
        cdf = pycdfpp.CDF()
        cdf.add_variable("variable", values=np.empty(
            shape=[0, 10, 2], dtype=np.float64))
        self.assertIn("variable", cdf)
        with self.assertRaises(ValueError):
            cdf["variable"].set_values(np.ones(100, dtype=np.float64))

    def test_setting_datetime64_ns_values(self):
        cdf = pycdfpp.CDF()
        values = make_datetime64_values()
        cdf.add_variable("datetime64[ns]", values=values)
        self.assertIn("datetime64[ns]", cdf)
        self.assertTrue(np.all(pycdfpp.to_datetime64(
            cdf["datetime64[ns]"]) == values))

    def test_setting_CDF_EPOCH_with_datetime64_ns_values(self):
        cdf = pycdfpp.CDF()
        values = make_datetime64_values()
        cdf.add_variable("datetime64[ns]", values=values, data_type=pycdfpp.DataType.CDF_EPOCH)
        self.assertIn("datetime64[ns]", cdf)
        self.assertTrue(np.all(pycdfpp.to_datetime64(
            cdf["datetime64[ns]"]) == values))

    def test_setting_CDF_EPOCH16_with_datetime64_ns_values(self):
        cdf = pycdfpp.CDF()
        values = make_datetime64_values()
        cdf.add_variable("datetime64[ns]", values=values, data_type=pycdfpp.DataType.CDF_EPOCH16)
        self.assertIn("datetime64[ns]", cdf)
        self.assertTrue(np.all(pycdfpp.to_datetime64(
            cdf["datetime64[ns]"]) == values))

    def test_setting_datetime64_ms_values(self):
        cdf = pycdfpp.CDF()
        values = make_datetime64_values()
        cdf.add_variable("datetime64[ns]", values=values.astype("datetime64[ms]"))
        self.assertIn("datetime64[ns]", cdf)
        self.assertTrue(np.all(pycdfpp.to_datetime64(
            cdf["datetime64[ns]"]) == values))

    def test_setting_datetime_values(self):
        cdf = pycdfpp.CDF()
        values = make_datetime_values()
        cdf.add_variable("datetime", values=values)
        self.assertIn("datetime", cdf)
        self.assertTrue(np.all(pycdfpp.to_datetime(cdf["datetime"]) == values))

    def test_setting_CDF_EPOCH_with_datetime_values(self):
        cdf = pycdfpp.CDF()
        values = make_datetime_values()
        cdf.add_variable("datetime", values=values, data_type=pycdfpp.DataType.CDF_EPOCH)
        self.assertIn("datetime", cdf)
        self.assertTrue(np.all(pycdfpp.to_datetime(cdf["datetime"]) == values))

    def test_setting_CDF_EPOCH16_with_datetime_values(self):
        cdf = pycdfpp.CDF()
        values = make_datetime_values()
        cdf.add_variable("datetime", values=values, data_type=pycdfpp.DataType.CDF_EPOCH16)
        self.assertIn("datetime", cdf)
        self.assertTrue(np.all(pycdfpp.to_datetime(cdf["datetime"]) == values))

    def test_creating_a_variable_with_a_list_of_int_takes_the_smallest_dtype(self):
        cdf = pycdfpp.CDF()
        cdf.add_variable("uint8", values=[1,2,3,255])
        self.assertEqual(cdf["uint8"].values.dtype, np.uint8)
        cdf.add_variable("uint16", values=[1,2,3,256])
        self.assertEqual(cdf["uint16"].values.dtype, np.uint16)
        cdf.add_variable("uint32", values=[1,2,3,65536])
        self.assertEqual(cdf["uint32"].values.dtype, np.uint32)
        cdf.add_variable("int8", values=[1,2,3,-128])
        self.assertEqual(cdf["int8"].values.dtype, np.int8)
        cdf.add_variable("int16", values=[1,2,3,-129])
        self.assertEqual(cdf["int16"].values.dtype, np.int16)
        cdf.add_variable("int32", values=[1,2,3,-32769])
        self.assertEqual(cdf["int32"].values.dtype, np.int32)
        cdf.add_variable("int64", values=[1,2,3,-4294967296])
        self.assertEqual(cdf["int64"].values.dtype, np.int64)
        cdf.add_variable("float", values=[1,2,3,1e-38])
        self.assertEqual(cdf["float"].values.dtype, np.float64)

    def test_setting_values_with_a_list_of_int_converts_to_variable_dtype(self):
        cdf = pycdfpp.CDF()
        cdf.add_variable("uint32", values = [1,2,3,4], data_type=pycdfpp.DataType.CDF_UINT4)
        cdf["uint32"].set_values([1,2,3,4])
        self.assertEqual(pycdfpp.DataType.CDF_UINT4, cdf["uint32"].type)
        cdf.add_variable("uint8", values = [1,2,3,4], data_type=pycdfpp.DataType.CDF_UINT1)
        self.assertEqual(pycdfpp.DataType.CDF_UINT1, cdf["uint8"].type)
        with self.assertRaises(ValueError):
            cdf["uint8"].set_values([100000,2,3,4])

    def test_setting_nrv_variable_skipping_record_dimension(self):
        cdf = pycdfpp.CDF()
        cdf.add_variable("nrv_var", values = np.empty(shape=[0,2,2]), is_nrv=True)
        self.assertTrue(cdf["nrv_var"].is_nrv)
        cdf["nrv_var"].set_values(np.ones(shape=[2,2]))

if __name__ == '__main__':
    unittest.main()
