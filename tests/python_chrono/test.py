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

        def test_todt64_empty(self):
            self.assertEqual(pycdfpp.to_datetime64(pycdfpp.to_tt2000([])),
                np.array([], dtype="datetime64[ns]"))

class PycdfChronoErrors(unittest.TestCase):
    def test_invalid_input(self):
        with self.assertRaises(ValueError):
            pycdfpp.to_datetime64(["not a datetime"])


if __name__ == '__main__':
    unittest.main()
