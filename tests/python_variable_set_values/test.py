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


if __name__ == '__main__':
    unittest.main()
