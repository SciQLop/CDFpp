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


class PycdfVariableSetValues(unittest.TestCase):
    def test_can_create_an_empty_CDF_Variable(self):
        cdf = pycdfpp.CDF()
        cdf.add_variable("Empty_variable")
        self.assertIn("Empty_variable", cdf)
        self.assertEqual(cdf["Empty_variable"].compression, pycdfpp.CDF_compression_type.no_compression)
        self.assertEqual(cdf["Empty_variable"].shape, [])
        self.assertEqual(cdf["Empty_variable"].is_nrv, False)


    def test_can_create_a_CDF_Variable_with_values(self):
        cdf = pycdfpp.CDF()
        values = np.array([1,2,3,4], dtype=np.float64)
        cdf.add_variable("variable", values = values)
        self.assertIn("variable", cdf)
        self.assertEqual(cdf["variable"].compression, pycdfpp.CDF_compression_type.no_compression)
        self.assertEqual(cdf["variable"].shape, [4])
        self.assertEqual(cdf["variable"].is_nrv, False)
        self.assertTrue(np.all(cdf["variable"].values == values))
        self.assertEqual(cdf["variable"].type, pycdfpp.CDF_DOUBLE)

if __name__ == '__main__':
    unittest.main()
