#!/usr/bin/env python
# -*- coding: utf-8 -*-
import os
from datetime import datetime, timedelta
import numpy as np
import math
import unittest
from glob import glob
import pycdfpp
from pycdfpp import tt2000_t, epoch, epoch16

os.environ['TZ'] = 'UTC'

variables = {
    'epoch': {
        'shape': (101,),
        'type': pycdfpp.DataType.CDF_EPOCH,
        'values': [datetime(1970, 1, 1)+timedelta(days=180*i) for i in range(101)],
        'attributes': {'epoch_attr': ["a variable attribute"]}
    },
    'tt2000': {
        'shape': (101,),
        'type': pycdfpp.DataType.CDF_TIME_TT2000,
        'values': [datetime(1970, 1, 1)+timedelta(days=180*i) for i in range(101)],
        'attributes': {}
    },
    'epoch16': {
        'shape': (101,),
        'type': pycdfpp.DataType.CDF_EPOCH16,
        'values': [datetime(1970, 1, 1)+timedelta(days=180*i) for i in range(101)],
        'attributes': {}
    },
    'var': {
        'shape': (101,),
        'type': pycdfpp.DataType.CDF_DOUBLE,
        'values': np.cos(np.arange(0., (101)/100*2.*math.pi, 2.*math.pi/100)),
        'attributes': {'var_attr': ["a variable attribute"], "DEPEND0": ["epoch"]}
    },
    'zeros': {
        'shape': (2048,),
        'type': pycdfpp.DataType.CDF_DOUBLE,
        'values': np.zeros(2048),
        'attributes': {'attr1': ["attr1_value"]}
    },
    'bytes': {
        'shape': (10,),
        'type': pycdfpp.DataType.CDF_BYTE,
        'values': np.ones(10),
        'attributes': {'attr1': ["attr1_value"]}
    },
    'var2d': {
        'shape': (3, 4),
        'type': pycdfpp.DataType.CDF_DOUBLE,
        'values': np.ones((3, 4)),
        'attributes': {'attr1': ["attr1_value"], 'attr2': ["attr2_value"]}
    },
    'var2d_counter': {
        'shape': (10, 10),
        'type': pycdfpp.DataType.CDF_DOUBLE,
        'values': np.arange(100, dtype=np.float64).reshape(10, 10),
        'attributes': {}
    },
    'var3d_counter': {
        'shape': (10,3,5),
        'type': pycdfpp.DataType.CDF_DOUBLE,
        'values': np.arange(10*3*5, dtype=np.float64).reshape(10,3,5),
        'attributes': {'attr1': ["attr1_value"], 'attr2': ["attr2_value"]}
    },
    'var5d_counter': {
        'shape': (6,5,4,3,2),
        'type': pycdfpp.DataType.CDF_DOUBLE,
        'values': np.arange(6*5*4*3*2, dtype=np.float64).reshape(6,5,4,3,2),
        'attributes': {}
    },
    'var3d': {
        'shape': (4, 3, 2),
        'type': pycdfpp.DataType.CDF_DOUBLE,
        'values': np.ones((4, 3, 2)),
        'attributes': {"var3d_attr_multi": [[10, 11]]}
    },
    'empty_var_recvary_string': {
        'shape': (0, 16),
        'type': pycdfpp.DataType.CDF_CHAR,
        'values': np.array([], dtype='|S16'),
        'attributes': {}
    },
    'var_recvary_string': {
        'shape': (3, 3),
        'type': pycdfpp.DataType.CDF_CHAR,
        'values': np.char.encode(['001', '002', '003']),
        'attributes': {}
    },
    'var_string': {
        'shape': (1, 16),
        'type': pycdfpp.DataType.CDF_CHAR,
        'values': np.char.encode([['This is a string']]),
        'attributes': {}
    },
    'var_string_uchar': {
        'shape': (1, 16),
        'type': pycdfpp.DataType.CDF_UCHAR,
        'values': np.char.encode([['This is a string']]),
        'attributes': {}
    },
    'var2d_string': {
        'shape': (1, 2, 18),
        'type': pycdfpp.DataType.CDF_CHAR,
        'values': np.char.encode(['This is a string 1', 'This is a string 2']),
        'attributes': {}
    },
    'var3d_string': {
        'shape': (1, 2, 2, 9),
        'type': pycdfpp.DataType.CDF_CHAR,
        'values': np.char.encode([['value[00]', 'value[01]'], ['value[10]', 'value[11]']]),
        'attributes': {}
    },
    'var4d_string': {
        'shape': (1, 3, 2, 2, 10),
        'type': pycdfpp.DataType.CDF_CHAR,
        'values': np.char.encode([
            [['value[000]','value[001]'],['value[010]','value[011]']],
            [['value[100]','value[101]'],['value[110]','value[111]']],
            [['value[200]','value[201]'],['value[210]','value[211]']]]),
        'attributes': {}
    }
}

attributes = {'attr': {"values": ["a cdf text attribute"], "types": [pycdfpp.DataType.CDF_CHAR]},
              'attr_float': {"values": [[1., 2., 3.], [4., 5., 6.]], "types": [pycdfpp.DataType.CDF_FLOAT, pycdfpp.DataType.CDF_FLOAT]},
              'attr_int': {"values": [[1, 2, 3]], "types": [pycdfpp.DataType.CDF_BYTE]},
              'attr_multi': {"values": [[1, 2], [2., 3.], "hello"], "types": [pycdfpp.DataType.CDF_BYTE, pycdfpp.DataType.CDF_FLOAT, pycdfpp.DataType.CDF_CHAR]},
              'empty': {"values": [], "types": []},
              'epoch': {"values": [
                  [epoch(62167219200000.0),
                   epoch(62182771200000.0),
                   epoch(62198323200000.0),
                   epoch(62213875200000.0),
                   epoch(62229427200000.0),
                   epoch(62244979200000.0),
                   epoch(62260531200000.0),
                   epoch(62276083200000.0),
                   epoch(62291635200000.0),
                   epoch(62307187200000.0),
                   epoch(62322739200000.0)]], "types": [pycdfpp.DataType.CDF_EPOCH]},
              'epoch16': {"values": [[
                  epoch16(62167219200.0, 0.0),
                  epoch16(62182771200.0, 0.0),
                  epoch16(62198323200.0, 0.0),
                  epoch16(62213875200.0, 0.0),
                  epoch16(62229427200.0, 0.0),
                  epoch16(62244979200.0, 0.0),
                  epoch16(62260531200.0, 0.0),
                  epoch16(62276083200.0, 0.0),
                  epoch16(62291635200.0, 0.0),
                  epoch16(62307187200.0, 0.0),
                  epoch16(62322739200.0, 0.0)
              ]], "types": [pycdfpp.DataType.CDF_EPOCH16]},
              'tt2000': {"values": [[
                  tt2000_t(-946727959814622001),
                  tt2000_t(-931175959348062000),
                  tt2000_t(-915623958881502000),
                  tt2000_t(-900071958414942001),
                  tt2000_t(-884519957948382000),
                  tt2000_t(-868967957816000000),
                  tt2000_t(-853415956816000000),
                  tt2000_t(-837863955816000000),
                  tt2000_t(-822311955816000000),
                  tt2000_t(-806759954816000000),
                  tt2000_t(-791207954816000000)]], "types": [pycdfpp.DataType.CDF_TIME_TT2000]}}


def load_bytes(fname):
    with open(fname, 'rb') as f:
        return f.read()


def compare_attributes(attrs, ref):
    for name, values in ref.items():
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
        cdf = pycdfpp.load(
            f'{os.path.dirname(os.path.abspath(__file__))}/../resources/testutf8.cdf')
        str(cdf)
        self.assertEqual(list(cdf.attributes["utf8"]), [
                         'ASCII: ABCDEFG', 'Latin1: ©æêü÷Æ¼®¢¥', 'Chinese: 社安', 'Other: ႡႢႣႤႥႦ'])

    def test_can_load_and_repr_latin1_with_hack(self):
        cdf = pycdfpp.load(
            f'{os.path.dirname(os.path.abspath(__file__))}/../resources/wi_l2-30min_sms-stics-afm-magnetosphere_00000000_v01.cdf')
        with self.assertRaises(UnicodeDecodeError):
            str(cdf)
        cdf = pycdfpp.load(
            f'{os.path.dirname(os.path.abspath(__file__))}/../resources/wi_l2-30min_sms-stics-afm-magnetosphere_00000000_v01.cdf', True)
        str(cdf)
        self.assertIn('4ïÂ°', cdf.attributes['TEXT'][0])


class PycdfLazyLoadingTest(unittest.TestCase):
    def test_lazy_in_memory_loading(self):
        files = glob(
            f'{os.path.dirname(os.path.abspath(__file__))}/../resources/a_*.cdf')
        for f in files:
            cdf = pycdfpp.load(load_bytes(f))
            data = [v.values for _, v in cdf.items()]
            self.assertTrue(all([d is not None for d in data]))


class PycdfDatetimeReprTest(unittest.TestCase):
    def test_can_repr_the_exact_expected_value_no_matter_what_TZ(self):
        backup_TZ = os.environ.get("TZ", None)
        for tz in ("UTC", "CEST", "JST", "Pacific/Niue"):
            os.environ["TZ"] = tz
            cdf = pycdfpp.load(
                f'{os.path.dirname(os.path.abspath(__file__))}/../resources/testutf8.cdf')
            self.assertEqual(
                str(cdf.attributes['epTestDate'][0][0]).strip(), '2004-05-13T15:08:11.022033044')
            self.assertEqual(
                str(cdf.attributes['TestDate'][0][0]).strip(), '2002-04-25T00:00:00.000000000')
            self.assertEqual(
                str(cdf.attributes['TestDate'][1][0]).strip(), '2008-02-04T06:08:10.012014016')
        if backup_TZ is None:
            os.environ.pop("TZ")
        else:
            os.environ["TZ"] = backup_TZ


class PycdfTest(unittest.TestCase):
    def setUp(self):
        files = glob(
            f'{os.path.dirname(os.path.abspath(__file__))}/../resources/a_*.cdf')
        self.cdfs = list(map(pycdfpp.load, files)) + list(map(lambda f: pycdfpp.load(load_bytes(f)), files))
        for cdf in self.cdfs:
            self.assertIsNotNone(cdf)

    def tearDown(self):
        pass

    def test_cdflib_version(self):
        for cdf in self.cdfs:
            self.assertEqual(cdf.distribution_version, (3, 9, 0))

    def test_has_all_expected_vars(self):
        for cdf in self.cdfs:
            for name in variables:
                self.assertTrue(name in cdf)

    def test_has_all_expected_attributes(self):
        for cdf in self.cdfs:
            for name, desc in attributes.items():
                self.assertTrue(name in cdf.attributes)
                attr = cdf.attributes[name]
                for i, t in enumerate(desc["types"]):
                    self.assertEqual(t, attr.type(i))
                for i, v in enumerate(desc["values"]):
                    self.assertEqual(v, attr[i])

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
                    self.assertTrue(np.all(whole_var == values))
                    self.assertTrue(np.all(whole_var == one_by_one))

                for attr in ('epoch', 'epoch16', 'tt2000'):
                    whole_attr = f(cdf.attributes[attr][0])
                    one_by_one = [f(v) for v in cdf.attributes[attr][0]]
                    self.assertIsNotNone(whole_attr)
                    self.assertIsNotNone(one_by_one)
                    self.assertTrue(np.all(whole_attr == one_by_one))

    def test_non_string_vars_implements_buffer_protocol(self):
        for cdf in self.cdfs:
            for name, var in cdf.items():
                if var.type not in (pycdfpp.DataType.CDF_CHAR, pycdfpp.DataType.CDF_UCHAR):
                    arr_from_buffer = np.array(var)
                    self.assertTrue(np.all(arr_from_buffer == var.values))

    def test_everything_have_repr(self):
        for cdf in self.cdfs:
            self.assertIsNotNone(str(cdf))
            for attr in cdf.attributes.items():
                self.assertIsNotNone(str(attr))
            for _, var in cdf.items():
                self.assertIsNotNone(str(var))
                for __, attr in var.attributes.items():
                    self.assertIsNotNone(str(attr))

    def test_vars_have_expected_type_and_shape(self):
        for cdf in self.cdfs:
            for name, var in cdf.items():
                self.assertTrue(name in variables)
                v_exp = variables[name]
                self.assertEqual(v_exp['shape'], var.shape)
                self.assertEqual(v_exp['type'], var.type)
                self.assertTrue(compare_attributes(
                    var.attributes, v_exp['attributes']), f"Broken var: {name}")
                if var.type in [pycdfpp.DataType.CDF_EPOCH, pycdfpp.DataType.CDF_EPOCH16]:
                    self.assertTrue(
                        np.all(v_exp['values'] == pycdfpp.to_datetime(var)), f"Broken var: {name}")
                elif var.type == pycdfpp.DataType.CDF_TIME_TT2000:
                    self.assertTrue(np.all(v_exp['values'][5:] == pycdfpp.to_datetime(var)[
                                    5:]), f"Broken var: {name}")
                elif var.type == pycdfpp.DataType.CDF_DOUBLE:
                    self.assertTrue(np.allclose(
                        v_exp['values'], var.values, rtol=1e-10, atol=0.), f"Broken var: {name}, values: {var.values}")
                else:
                    self.assertTrue(np.all(
                        v_exp['values'] == var.values), f"Broken var: {name}, values: {var.values}")


class PycdfNonRegression(unittest.TestCase):
    def test_ace_h0_mfi_bgsm_shape(self):
        cdf = pycdfpp.load(
            f'{os.path.dirname(os.path.abspath(__file__))}/../resources/ac_h0_mfi_00000000_v01.cdf')
        self.assertEqual(cdf['BGSM'].shape, (0, 3))

    def test_ace_h0_mfi_label_time_shape(self):
        cdf = pycdfpp.load(
            f'{os.path.dirname(os.path.abspath(__file__))}/../resources/ac_h0_mfi_00000000_v01.cdf')
        self.assertEqual(cdf['label_time'].shape, (1, 3, 27))

    def test_solo_l2_rpw_lfr_surv_swf_e_labels(self):
        cdf = pycdfpp.load(
            f'{os.path.dirname(os.path.abspath(__file__))}/../resources/solo_l2_rpw-lfr-surv-swf-e_00000000_v01.cdf')
        self.assertListEqual(cdf['VDC_LABEL'].values_encoded[0].tolist(), ["Vdc1", "Vdc2", "Vdc3"])

    def test_empty_NRV_char_var_to_buffer(self):
        cdf = pycdfpp.load(
            f'{os.path.dirname(os.path.abspath(__file__))}/../resources/uy_proton-distributions_swoops_00000000_v01.cdf')
        self.assertIsNotNone(memoryview(cdf['Vpar']))
        self.assertIsNotNone(memoryview(cdf['Vper']))
        self.assertIsNotNone(cdf['Vpar'].values_encoded)
        self.assertIsNotNone(cdf['Vper'].values_encoded)


if __name__ == '__main__':
    unittest.main()
