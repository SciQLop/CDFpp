#!/usr/bin/env python
# -*- coding: utf-8 -*-
import os
import unittest
import pycdfpp

variables = {
'epoch':{
    'shape':[101],
    'type':pycdfpp.CDF_EPOCH
},
'tt2000':{
    'shape':[101],
    'type':pycdfpp.CDF_TIME_TT2000
},
'epoch16':{
    'shape':[101],
    'type':pycdfpp.CDF_EPOCH16
},
'var':{
    'shape':[101],
    'type':pycdfpp.CDF_DOUBLE
},
'var2d':{
    'shape':[3,4],
    'type':pycdfpp.CDF_DOUBLE
},
'var2d_counter':{
    'shape':[10,10],
    'type':pycdfpp.CDF_DOUBLE
},
'var3d':{
    'shape':[4,3,2],
    'type':pycdfpp.CDF_DOUBLE
},
'var_string':{
    'shape':[16],
    'type':pycdfpp.CDF_CHAR
},
'var2d_string':{
    'shape':[2,18],
    'type':pycdfpp.CDF_CHAR
}
}

attributes = ['attr', 'attr_float', 'attr_int', 'attr_multi', 'empty']

class PycdfTest(unittest.TestCase):
    def setUp(self):
        self.cdf = pycdfpp.load(f'{os.path.dirname(os.path.abspath(__file__))}/../resources/a_cdf.cdf')
        self.assertIsNotNone(self.cdf)

    def tearDown(self):
        pass

    def test_has_all_expected_vars(self):
        for name in variables:
            self.assertTrue(name in self.cdf)

    def test_has_all_expected_attributes(self):
        for name in attributes:
            self.assertTrue(name in self.cdf.attributes)

    def test_access_attribute_data_outside_of_range(self):
        attr = self.cdf.attributes['attr_int']
        with self.assertRaises(IndexError):
            attr[len(attr)]

    def test_vars_have_expected_type_and_shape(self):
        for name,var in self.cdf.items():
            self.assertTrue(name in variables)
            v_exp = variables[name]
            self.assertEqual(v_exp['shape'], var.shape)
            self.assertEqual(v_exp['type'], var.type)


if __name__ == '__main__':
    unittest.main()
