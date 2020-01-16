#!/usr/bin/env python
# -*- coding: utf-8 -*-
import unittest
import pycdfpp

class PycdfTest(unittest.TestCase):
    def setUp(self):
        pass

    def tearDown(self):
        pass

    def test_loads_cdf(self):
        cdf = pycdfpp.load(f'{os.path.dirname(os.path.abspath(__file__))}/../resources/a_cdf.cdf')
        self.assertIsNotNone(cdf)
