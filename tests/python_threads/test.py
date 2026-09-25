#!/usr/bin/env python
"""Threads sharing one CDF. pycdfpp releases the GIL while loading, saving and converting,
and declares itself free-threading ready: on a free-threaded Python (3.14t) everything here
runs truly in parallel."""
import os
import sys
import sysconfig
import tempfile
import threading
import unittest

import numpy as np
import pycdfpp

THREADS = 8
ROUNDS = 10
RECORDS = 200_000
GZIP = pycdfpp.CompressionType.gzip_compression


def expected_values():
    return {f"var{i}": np.arange(RECORDS, dtype=np.float64) * (i + 1) for i in range(THREADS)}


def make_file(path):
    cdf = pycdfpp.CDF()
    times = np.datetime64("2020-01-01", "ns") + np.arange(RECORDS).astype("timedelta64[s]")
    cdf.add_variable("time", times, data_type=pycdfpp.DataType.CDF_TIME_TT2000, compression=GZIP)
    for name, values in expected_values().items():
        cdf.add_variable(name, values, compression=GZIP)
    pycdfpp.save(cdf, path)
    return times


def run_together(*targets):
    """Starts every target at the same time and re-raises the first failure."""
    barrier = threading.Barrier(len(targets))
    errors = []

    def run(target):
        barrier.wait()
        try:
            target()
        except BaseException as e:
            errors.append(e)

    threads = [threading.Thread(target=run, args=(t,)) for t in targets]
    for t in threads:
        t.start()
    for t in threads:
        t.join()
    if errors:
        raise errors[0]


class SharedCdf(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        cls.path = os.path.join(cls.tmp.name, "shared.cdf")
        cls.times = make_file(cls.path)
        cls.expected = expected_values()

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def check(self, cdf, name):
        np.testing.assert_array_equal(np.array(cdf[name].values), self.expected[name])

    def test_gil_stays_disabled_on_free_threaded_python(self):
        if not sysconfig.get_config_var("Py_GIL_DISABLED"):
            self.skipTest("not a free-threaded Python")
        self.assertFalse(sys._is_gil_enabled())

    def test_threads_read_different_variables(self):
        for _ in range(ROUNDS):
            cdf = pycdfpp.load(self.path)
            run_together(*(lambda n=name: self.check(cdf, n) for name in self.expected))

    def test_threads_read_the_same_variable(self):
        for _ in range(ROUNDS):
            cdf = pycdfpp.load(self.path)
            run_together(*(lambda: self.check(cdf, "var0") for _ in range(THREADS)))

    def test_time_conversion_while_reading(self):
        def convert(cdf):
            np.testing.assert_array_equal(pycdfpp.to_datetime64(cdf["time"]), self.times)

        for _ in range(ROUNDS):
            cdf = pycdfpp.load(self.path)
            run_together(*(lambda: convert(cdf) for _ in range(THREADS // 2)),
                         *(lambda n=name: self.check(cdf, n)
                           for name in list(self.expected)[:THREADS // 2]))

    def test_save_while_reading(self):
        def save(cdf):
            saved = pycdfpp.load(bytes(pycdfpp.save(cdf)))
            for name in self.expected:
                self.check(saved, name)

        for _ in range(ROUNDS // 2):
            cdf = pycdfpp.load(self.path)
            run_together(*(lambda: save(cdf) for _ in range(2)),
                         *(lambda n=name: self.check(cdf, n) for name in self.expected))


if __name__ == "__main__":
    unittest.main()
