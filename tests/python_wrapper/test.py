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
'bytes':{
    'shape':[10],
    'type':pycdfpp.CDF_BYTE,
    'values':np.ones(10),
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
'var_string_uchar':{
    'shape':[16],
    'type':pycdfpp.CDF_UCHAR,
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


class PycdfEncodingTest(unittest.TestCase):
    def test_can_load_and_repr_utf8(self):
        cdf = pycdfpp.load(f'{os.path.dirname(os.path.abspath(__file__))}/../resources/testutf8.cdf')
        str(cdf)
        self.assertEqual(list(cdf.attributes["utf8"]), ['ASCII: ABCDEFG', 'Latin1: ©æêü÷Æ¼®¢¥', 'Chinese: 社安', 'Other: ႡႢႣႤႥႦ'])

    def test_can_load_and_repr_latin1_with_hack(self):
        cdf = pycdfpp.load(f'{os.path.dirname(os.path.abspath(__file__))}/../resources/wi_l2-30min_sms-stics-afm-magnetosphere_00000000_v01.cdf')
        with self.assertRaises(UnicodeDecodeError):
            str(cdf)
        cdf = pycdfpp.load(f'{os.path.dirname(os.path.abspath(__file__))}/../resources/wi_l2-30min_sms-stics-afm-magnetosphere_00000000_v01.cdf', True)
        str(cdf)
        self.assertIn('4ïÂ°', cdf.attributes['TEXT'][0])


class PycdfDatetimeReprTest(unittest.TestCase):
    def test_can_repr_the_exact_expected_value_no_matter_what_TZ(self):
        backup_TZ=os.environ.get("TZ", None)
        for tz in ("UTC", "CEST", "JST", "Pacific/Niue"):
            os.environ["TZ"] = tz
            cdf = pycdfpp.load(f'{os.path.dirname(os.path.abspath(__file__))}/../resources/testutf8.cdf')
            self.assertEqual(str(cdf.attributes['epTestDate'][0][0]).strip(), '2004-05-13T15:08:11.022033')
            self.assertEqual(str(cdf.attributes['TestDate'][0][0]).strip(), '2002-04-25T00:00:00.000000')
            self.assertEqual(str(cdf.attributes['TestDate'][1][0]).strip(), '2008-02-04T06:08:10.012014')
        if backup_TZ is None:
            os.environ.pop("TZ")
        else:
            os.environ["TZ"] = backup_TZ

class PycdfTest(unittest.TestCase):
    def setUp(self):
        files = glob(f'{os.path.dirname(os.path.abspath(__file__))}/../resources/a_*.cdf')
        self.cdfs = list(map(pycdfpp.load, files)) + list(map(lambda f: pycdfpp.load(load_bytes(f)), files))
        for cdf in self.cdfs:
            self.assertIsNotNone(cdf)

    def tearDown(self):
        pass

    def test_cdflib_version(self):
        for cdf in self.cdfs:
            self.assertEqual(cdf.distribution_version, (3,8,0))

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

    def test_datetime_conversions(self):
        for cdf in self.cdfs:
            for f in (pycdfpp.to_datetime, pycdfpp.to_datetime64):
                for var in ('epoch', 'epoch16', 'tt2000'):
                    whole_var = f(cdf[var])
                    values = f(cdf[var].values)
                    one_by_one = [f(v) for v in cdf[var].values]
                    self.assertIsNotNone(whole_var)
                    self.assertIsNotNone(values)
                    self.assertIsNotNone(one_by_one)
                    self.assertTrue(np.all(whole_var==values))
                    self.assertTrue(np.all(whole_var==one_by_one))

                for attr in ('epoch', 'epoch16', 'tt2000'):
                    whole_attr = f(cdf.attributes[attr][0])
                    one_by_one = [f(v) for v in cdf.attributes[attr][0]]
                    self.assertIsNotNone(whole_attr)
                    self.assertIsNotNone(one_by_one)
                    self.assertTrue(np.all(whole_attr==one_by_one))

    def test_non_string_vars_implements_buffer_protocol(self):
        for cdf in self.cdfs:
            for name,var in cdf.items():
                if var.type not in (pycdfpp.CDF_CHAR, pycdfpp.CDF_UCHAR):
                    arr_from_buffer = np.array(var)
                    self.assertTrue(np.all(arr_from_buffer==var.values))

    def test_everything_have_repr(self):
        for cdf in self.cdfs:
            self.assertIsNotNone(str(cdf))
            for attr in cdf.attributes.items():
                self.assertIsNotNone(str(attr))
            for _,var in cdf.items():
                self.assertIsNotNone(str(var))
                for __,attr in var.attributes.items():
                    self.assertIsNotNone(str(attr))

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
