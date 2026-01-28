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


def make_datetime_values():
    return [ datetime(2000, 1, 1, 12, 0,5,microsecond=10000) + timedelta(seconds=i) for i in range(100) ]


class PycdfChrono(unittest.TestCase):
    def test_simple_dt_tt2000(self):
        ref = [datetime(2000, 1, 1, 0, 0, 0),
               datetime(2020, 5, 15, 12, 30, 45),
               datetime(1995, 7, 4, 18, 15, 30)]
        res = pycdfpp.to_tt2000(ref)
        self.assertListEqual(ref, pycdfpp.to_datetime(res))

    def test_simple_dt64_tt2000(self):
        ref = make_datetime64_values()
        self.assertTrue(np.all(ref == pycdfpp.to_datetime64(pycdfpp.to_tt2000(ref))))


if __name__ == '__main__':
    unittest.main()
