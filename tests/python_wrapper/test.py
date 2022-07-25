#!/usr/bin/env python
# -*- coding: utf-8 -*-
import os
from datetime import datetime, timedelta
import numpy as np
import math
import unittest
from glob import glob
import pycdfpp

os.environ['TZ'] = 'UTC'

variables = {
'epoch':{
    'shape':[101],
    'type':pycdfpp.CDF_EPOCH,
    'values': [datetime(1970,1,1)+timedelta(days=180*i) for i in range(101)],
    'attributes':{'epoch_attr':["a variable attribute"]}
},
'tt2000':{
    'shape':[101],
    'type':pycdfpp.CDF_TIME_TT2000,
    'values': [datetime(1970,1,1)+timedelta(days=180*i) for i in range(101)],
    'attributes':{}
},
'epoch16':{
    'shape':[101],
    'type':pycdfpp.CDF_EPOCH16,
    'values': [datetime(1970,1,1)+timedelta(days=180*i) for i in range(101)],
    'attributes':{}
},
'var':{
    'shape':[101],
    'type':pycdfpp.CDF_DOUBLE,
    'values':np.cos(np.arange(0.,(101)/100*2.*math.pi,2.*math.pi/100)),
    'attributes':{'var_attr':["a variable attribute"], "DEPEND0":["epoch"]}
},
'zeros':{
    'shape':[2048],
    'type':pycdfpp.CDF_DOUBLE,
    'values':np.zeros(2048),
    'attributes':{'attr1':["attr1_value"]}
},
'var2d':{
    'shape':[3,4],
    'type':pycdfpp.CDF_DOUBLE,
    'values': np.ones((3,4)),
    'attributes':{'attr1':["attr1_value"], 'attr2':["attr2_value"]}
},
'var2d_counter':{
    'shape':[10,10],
    'type':pycdfpp.CDF_DOUBLE,
    'values':np.arange(100, dtype=np.float64).reshape(10,10),
    'attributes':{}
},
'var3d_counter':{
    'shape':[3,3,3],
    'type':pycdfpp.CDF_DOUBLE,
    'values': np.arange(3**3, dtype=np.float64).reshape(3,3,3),
    'attributes':{'attr1':["attr1_value"], 'attr2':["attr2_value"]}
},
'var3d':{
    'shape':[4,3,2],
    'type':pycdfpp.CDF_DOUBLE,
    'values': np.ones((4,3,2)),
    'attributes':{"var3d_attr_multi":[[10,11]]}
},
'empty_var_recvary_string':{
    'shape':[0,16],
    'type':pycdfpp.CDF_CHAR,
    'values': [],
    'attributes':{}
},
'var_recvary_string':{
    'shape':[3,3],
    'type':pycdfpp.CDF_CHAR,
    'values': ['001', '002', '003'],
    'attributes':{}
},
'var_string':{
    'shape':[16],
    'type':pycdfpp.CDF_CHAR,
    'values': 'This is a string',
    'attributes':{}
},
'var2d_string':{
    'shape':[2,18],
    'type':pycdfpp.CDF_CHAR,
    'values': ['This is a string 1','This is a string 2'],
    'attributes':{}
},
'var3d_string':{
    'shape':[2,2,9],
    'type':pycdfpp.CDF_CHAR,
    'values': [['value[00]','value[01]'],['value[10]','value[11]']],
    'attributes':{}
}
}

attributes = ['attr', 'attr_float', 'attr_int', 'attr_multi', 'empty']

def load_bytes(fname):
    with open(fname,'rb') as f:
        return f.read()


def compare_attributes(attrs, ref):
    for name,values in ref.items():
        if name not in attrs:
            print(f"No {name} in {attrs}")
            return False
        for index in range(len(values)):
            if values[index] != attrs[name][index]:
                print(f"No {values[index]} in {ref[name][index]}")
                return False
    return True


class PycdfTest(unittest.TestCase):
    def setUp(self):
        files = glob(f'{os.path.dirname(os.path.abspath(__file__))}/../resources/a_*.cdf')
        self.cdfs = list(map(pycdfpp.load, files)) + list(map(lambda f: pycdfpp.load(load_bytes(f)), files))
        for cdf in self.cdfs:
            self.assertIsNotNone(cdf)

    def tearDown(self):
        pass

    def test_has_all_expected_vars(self):
        for cdf in self.cdfs:
            for name in variables:
                self.assertTrue(name in cdf)

    def test_has_all_expected_attributes(self):
        for cdf in self.cdfs:
            for name in attributes:
                self.assertTrue(name in cdf.attributes)

    def test_access_attribute_data_outside_of_range(self):
        attr = self.cdfs[0].attributes['attr_int']
        with self.assertRaises(IndexError):
            attr[len(attr)]

    def test_vars_have_expected_type_and_shape(self):
        for cdf in self.cdfs:
            for name,var in cdf.items():
                self.assertTrue(name in variables)
                v_exp = variables[name]
                self.assertEqual(v_exp['shape'], var.shape)
                self.assertEqual(v_exp['type'], var.type)
                self.assertTrue(compare_attributes(var.attributes, v_exp['attributes']), f"Broken var: {name}")
                if var.type in [pycdfpp.CDF_EPOCH, pycdfpp.CDF_EPOCH16]:
                    self.assertTrue(np.all(v_exp['values'] == pycdfpp.to_datetime(var)), f"Broken var: {name}")
                elif var.type == pycdfpp.CDF_TIME_TT2000:
                    self.assertTrue(np.all(v_exp['values'][5:] == pycdfpp.to_datetime(var)[5:]), f"Broken var: {name}")
                elif var.type == pycdfpp.CDF_DOUBLE:
                    self.assertTrue(np.allclose(v_exp['values'], var.values, rtol=1e-10, atol=0.), f"Broken var: {name}, values: {var.values}")
                else:
                    self.assertTrue(np.all(v_exp['values']== var.values), f"Broken var: {name}, values: {var.values}")


if __name__ == '__main__':
    unittest.main()
