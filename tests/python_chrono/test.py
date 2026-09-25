#!/usr/bin/env python
# -*- coding: utf-8 -*-
import os
from datetime import datetime, timedelta
import numpy as np
import unittest
import pycdfpp

os.environ['TZ'] = 'UTC'


def make_datetime64_values():
    return np.arange(1e18, 11e17, 1e16, dtype=np.int64).astype("datetime64[ns]")


def make_datetime64_n_values(count:int, start:int=1e18, stop:int=2e18):
    step = (stop - start) / count
    return np.arange(start, stop, step, dtype=np.int64).astype("datetime64[ns]")

def make_datetime_values(count:int=100):
    return [ datetime(2000, 1, 1, 12, 0,5,microsecond=10000) + timedelta(seconds=i) for i in range(count) ]


def make_list_of_mixed_types(count:int=100):
    ref = make_datetime_values(count)
    mixed = ref.copy()
    for i in range(count):
        if i % 4 == 0:
            mixed[i] = pycdfpp.to_tt2000(mixed[i])
        elif i % 4 == 1:
            mixed[i] = pycdfpp.to_epoch(mixed[i])
        elif i % 4 == 2:
            mixed[i] = pycdfpp.to_epoch16(mixed[i])
    return ref, mixed


class PycdfChrono(unittest.TestCase):
    def test_simple_dt_tt2000(self):
        ref = [datetime(2000, 1, 1, 0, 0, 0),
               datetime(2020, 5, 15, 12, 30, 45),
               datetime(1995, 7, 4, 18, 15, 30)]
        res = pycdfpp.to_tt2000(ref)
        self.assertListEqual(ref, pycdfpp.to_datetime(res))

    def test_simple_dt_dt64(self):
        ref = [datetime(2000, 1, 1, 0, 0, 0),
               datetime(2020, 5, 15, 12, 30, 45),
               datetime(1995, 7, 4, 18, 15, 30)]
        res = pycdfpp.to_datetime64(ref)
        self.assertListEqual(ref, pycdfpp.to_datetime(res))

    def test_mix_dt64(self):
        ref, mixed = make_list_of_mixed_types()
        res = pycdfpp.to_datetime64(mixed)
        self.assertListEqual(ref, pycdfpp.to_datetime(res))

    def test_mix_dt(self):
        ref, mixed = make_list_of_mixed_types()
        res = pycdfpp.to_datetime(mixed)
        self.assertListEqual(ref, res)

    def test_mix_tt2000(self):
        ref, mixed = make_list_of_mixed_types()
        res = pycdfpp.to_tt2000(mixed)
        self.assertListEqual(ref, pycdfpp.to_datetime(res))

    def test_simple_dt64_tt2000(self):
        ref = make_datetime64_values()
        self.assertTrue(np.all(ref == pycdfpp.to_datetime64(pycdfpp.to_tt2000(ref))))

    def test_simple_dt64_epoch(self):
        ref = make_datetime64_values()
        self.assertTrue(np.all(ref == pycdfpp.to_datetime64(pycdfpp.to_epoch(ref))))

    def test_simple_dt64_epoch16(self):
        ref = make_datetime64_values()
        self.assertTrue(np.all(ref == pycdfpp.to_datetime64(pycdfpp.to_epoch16(ref))))

    def test_dt64_tt2000_variable_size(self):
        # the underlying algorithm depends on the input size, so we need to test with different sizes
        for size in (1, 10, 100, 1000, 10000, 2**20, 2**24):
            ref = make_datetime64_n_values(int(size))
            self.assertTrue(np.all(ref == pycdfpp.to_datetime64(pycdfpp.to_tt2000(ref))))
        # it also depend on asumptions like the first value being after 2017 (the last leap second)
        for size in (1, 10, 100, 1000, 10000, 2**20, 2**24):
            ref = make_datetime64_n_values(int(size), start=1.5e18)
            self.assertTrue(np.all(ref == pycdfpp.to_datetime64(pycdfpp.to_tt2000(ref))))
        # and the array being sorted
        for size in (1, 10, 100, 1000, 10000, 2**20, 2**24):
            ref = make_datetime64_n_values(int(size))
            np.random.shuffle(ref)
            self.assertTrue(np.all(ref == pycdfpp.to_datetime64(pycdfpp.to_tt2000(ref))))

    def test_todt64_empty_list(self):
        result = pycdfpp.to_datetime64([])
        self.assertEqual(len(result), 0)

    def test_todt64_empty_tt2000_array(self):
        empty = pycdfpp.to_tt2000(np.array([], dtype="datetime64[ns]"))
        result = pycdfpp.to_datetime64(empty)
        self.assertEqual(len(result), 0)

    def test_todt64_empty_epoch_array(self):
        empty = pycdfpp.to_epoch(np.array([], dtype="datetime64[ns]"))
        result = pycdfpp.to_datetime64(empty)
        self.assertEqual(len(result), 0)

    def test_todt64_empty_epoch16_array(self):
        empty = pycdfpp.to_epoch16(np.array([], dtype="datetime64[ns]"))
        result = pycdfpp.to_datetime64(empty)
        self.assertEqual(len(result), 0)

    def test_todt64_empty_tt2000_variable(self):
        cdf = pycdfpp.CDF()
        cdf.add_variable("time").set_values(
            np.array([], dtype="datetime64[ns]"), pycdfpp.DataType.CDF_TIME_TT2000)
        result = pycdfpp.to_datetime64(cdf["time"])
        self.assertEqual(len(result), 0)

    def test_todt64_empty_epoch_variable(self):
        cdf = pycdfpp.CDF()
        cdf.add_variable("time").set_values(
            np.array([], dtype="datetime64[ns]"), pycdfpp.DataType.CDF_EPOCH)
        result = pycdfpp.to_datetime64(cdf["time"])
        self.assertEqual(len(result), 0)

    def test_todt64_empty_epoch16_variable(self):
        cdf = pycdfpp.CDF()
        cdf.add_variable("time").set_values(
            np.array([], dtype="datetime64[ns]"), pycdfpp.DataType.CDF_EPOCH16)
        result = pycdfpp.to_datetime64(cdf["time"])
        self.assertEqual(len(result), 0)

    def test_todatetime_empty_tt2000_variable(self):
        cdf = pycdfpp.CDF()
        cdf.add_variable("time").set_values(
            np.array([], dtype="datetime64[ns]"), pycdfpp.DataType.CDF_TIME_TT2000)
        result = pycdfpp.to_datetime(cdf["time"])
        self.assertEqual(len(result), 0)

    def test_todatetime_empty_epoch_variable(self):
        cdf = pycdfpp.CDF()
        cdf.add_variable("time").set_values(
            np.array([], dtype="datetime64[ns]"), pycdfpp.DataType.CDF_EPOCH)
        result = pycdfpp.to_datetime(cdf["time"])
        self.assertEqual(len(result), 0)

    def test_todatetime_empty_epoch16_variable(self):
        cdf = pycdfpp.CDF()
        cdf.add_variable("time").set_values(
            np.array([], dtype="datetime64[ns]"), pycdfpp.DataType.CDF_EPOCH16)
        result = pycdfpp.to_datetime(cdf["time"])
        self.assertEqual(len(result), 0)

class PycdfToTimeString(unittest.TestCase):
    def test_tt2000_iso_format(self):
        times = np.array(['2020-01-01T00:00:00', '2020-06-15T12:30:45'], dtype='datetime64[ns]')
        tt = pycdfpp.to_tt2000(times)
        result = pycdfpp.to_time_string(tt, '%Y-%m-%dT%H:%M:%SZ')
        self.assertEqual(result[0], b'2020-01-01T00:00:00.000000000Z')
        self.assertEqual(result[1], b'2020-06-15T12:30:45.000000000Z')
        self.assertEqual(result.dtype, np.dtype('S30'))

    def test_epoch_iso_format(self):
        times = np.array(['2020-01-01T00:00:00', '2020-06-15T12:30:45'], dtype='datetime64[ns]')
        ep = pycdfpp.to_epoch(times)
        result = pycdfpp.to_time_string(ep, '%Y-%m-%dT%H:%M:%SZ')
        self.assertTrue(result[0].startswith(b'2020-01-01T00:00:00'))
        self.assertTrue(result[1].startswith(b'2020-06-15T12:30:45'))

    def test_epoch16_iso_format(self):
        times = np.array(['2020-01-01T00:00:00', '2020-06-15T12:30:45'], dtype='datetime64[ns]')
        ep16 = pycdfpp.to_epoch16(times)
        result = pycdfpp.to_time_string(ep16, '%Y-%m-%dT%H:%M:%SZ')
        self.assertTrue(result[0].startswith(b'2020-01-01T00:00:00'))
        self.assertTrue(result[1].startswith(b'2020-06-15T12:30:45'))

    def test_custom_format(self):
        times = np.array(['2020-03-15T08:30:00'], dtype='datetime64[ns]')
        tt = pycdfpp.to_tt2000(times)
        result = pycdfpp.to_time_string(tt, '%Y-%jT%H:%M:%SZ')
        self.assertTrue(result[0].startswith(b'2020-075T08:30:00'))

    def test_date_only_format(self):
        times = np.array(['2020-03-15T00:00:00'], dtype='datetime64[ns]')
        tt = pycdfpp.to_tt2000(times)
        result = pycdfpp.to_time_string(tt, '%Y-%m-%d')
        self.assertEqual(result[0], b'2020-03-15')

    def test_variable_tt2000(self):
        times = np.array(['2020-01-01T00:00:00', '2020-01-01T01:00:00'], dtype='datetime64[ns]')
        cdf = pycdfpp.CDF()
        cdf.add_variable('Epoch', values=pycdfpp.to_tt2000(times),
                         data_type=pycdfpp.DataType.CDF_TIME_TT2000)
        result = pycdfpp.to_time_string(cdf['Epoch'], '%Y-%m-%dT%H:%M:%SZ')
        self.assertEqual(len(result), 2)
        self.assertEqual(result[0], b'2020-01-01T00:00:00.000000000Z')

    def test_variable_epoch(self):
        times = np.array(['2020-01-01T00:00:00', '2020-01-01T01:00:00'], dtype='datetime64[ns]')
        cdf = pycdfpp.CDF()
        cdf.add_variable('Epoch', values=pycdfpp.to_epoch(times),
                         data_type=pycdfpp.DataType.CDF_EPOCH)
        result = pycdfpp.to_time_string(cdf['Epoch'], '%Y-%m-%dT%H:%M:%SZ')
        self.assertEqual(len(result), 2)
        self.assertTrue(result[0].startswith(b'2020-01-01T00:00:00'))

    def test_empty_array(self):
        tt = pycdfpp.to_tt2000(np.array([], dtype='datetime64[ns]'))
        result = pycdfpp.to_time_string(tt, '%Y-%m-%dT%H:%M:%SZ')
        self.assertEqual(len(result), 0)

    def test_invalid_variable_type(self):
        cdf = pycdfpp.CDF()
        cdf.add_variable('data', values=np.array([1.0, 2.0]),
                         data_type=pycdfpp.DataType.CDF_DOUBLE)
        with self.assertRaises(Exception):
            pycdfpp.to_time_string(cdf['data'], '%Y-%m-%dT%H:%M:%SZ')

    def test_preserves_shape(self):
        times = np.arange('2020-01-01', '2020-01-02', dtype='datetime64[h]').astype('datetime64[ns]')
        tt = pycdfpp.to_tt2000(times)
        result = pycdfpp.to_time_string(tt, '%Y-%m-%dT%H:%M:%SZ')
        self.assertEqual(result.shape, tt.shape)

    def test_subsecond_precision_tt2000(self):
        times = np.array(['2020-01-01T00:00:00.123456789'], dtype='datetime64[ns]')
        tt = pycdfpp.to_tt2000(times)
        result = pycdfpp.to_time_string(tt, '%Y-%m-%dT%H:%M:%S')
        self.assertIn(b'123456789', result[0])


class PycdfChronoErrors(unittest.TestCase):
    def test_invalid_input(self):
        with self.assertRaises(ValueError):
            pycdfpp.to_datetime64(["not a datetime"])


class PycdfEpoch16Precision(unittest.TestCase):
    def test_epoch16_to_datetime64_is_exact(self):
        # EPOCH16 stores whole seconds and picoseconds exactly: nanoseconds must round-trip.
        ns = np.arange(0, 10_000, dtype=np.int64) * 16_666_666_666 + 1_000_000_000_000_000_000
        epochs16 = pycdfpp.to_epoch16(ns.astype("datetime64[ns]"))
        np.testing.assert_array_equal(pycdfpp.to_datetime64(epochs16).astype(np.int64), ns)


class PycdfMultiDimTimes(unittest.TestCase):
    """Time arrays with more than one dimension, like ISTP files storing Epoch as (N, 1)."""

    def _variable(self, shape, data_type):
        # Millisecond-aligned times are stored exactly by every CDF time type.
        values = make_datetime64_n_values(int(np.prod(shape))).reshape(shape)
        values = values.astype("datetime64[ms]").astype("datetime64[ns]")
        cdf = pycdfpp.CDF()
        cdf.add_variable("t", values=values, data_type=data_type)
        return cdf["t"], values

    def _expected(self, values):
        return values.astype("datetime64[us]").tolist()

    def test_to_datetime_2d_variables(self):
        for data_type in (pycdfpp.DataType.CDF_TIME_TT2000, pycdfpp.DataType.CDF_EPOCH,
                          pycdfpp.DataType.CDF_EPOCH16):
            for shape in ((50, 1), (20, 3)):
                with self.subTest(data_type=data_type, shape=shape):
                    var, values = self._variable(shape, data_type)
                    self.assertEqual(pycdfpp.to_datetime(var), self._expected(values))
                    self.assertEqual(pycdfpp.to_datetime(var.values), self._expected(values))

    def test_to_datetime_3d_array(self):
        var, values = self._variable((4, 3, 2), pycdfpp.DataType.CDF_TIME_TT2000)
        self.assertEqual(pycdfpp.to_datetime(var.values), self._expected(values))

    def test_to_datetime_2d_datetime64(self):
        values = make_datetime64_n_values(60).reshape((20, 3))
        self.assertEqual(pycdfpp.to_datetime(values), self._expected(values))

    def test_non_contiguous_arrays(self):
        var, values = self._variable((40, 3), pycdfpp.DataType.CDF_TIME_TT2000)
        strided = var.values[::3, 1]
        expected = values[::3, 1]
        self.assertEqual(pycdfpp.to_datetime(strided), self._expected(expected))
        np.testing.assert_array_equal(pycdfpp.to_datetime64(strided), expected)
        np.testing.assert_array_equal(pycdfpp.to_datetime64(var.values.T), values.T)


if __name__ == '__main__':
    unittest.main()
