#!/usr/bin/env python
# -*- coding: utf-8 -*-
import os
import unittest
import pycdfpp

variables = [
{
    'name':'epoch',
    'shape':[101]
},
{
    'name':'var',
    'shape':[101]
},
{
    'name':'var2d',
    'shape':[3,4]
},
{
    'name':'var2d_counter',
    'shape':[2,2]
},
{
    'name':'var3d',
    'shape':[4,3,2]
}
]

class PycdfTest(unittest.TestCase):
    def setUp(self):
        pass

    def tearDown(self):
        pass

    def test_loads_cdf(self):
        cdf = pycdfpp.load(f'{os.path.dirname(os.path.abspath(__file__))}/../resources/a_cdf.cdf')
        self.assertIsNotNone(cdf)
        for v in variables:
            self.assertTrue(v['name'] in cdf)


if __name__ == '__main__':
    unittest.main()
