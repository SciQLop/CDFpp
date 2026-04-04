#!/usr/bin/env python
"""Reproducer tests for Windows heap corruption crashes.

These tests exercise code paths that crash on Windows but pass silently on
Linux.  The goal is to catch the issue via CI on windows-latest.

Symptoms on Windows:
  - Crash when the result of to_datetime64(variable) is freed / reassigned.
  - Crash when opening / repr'ing a CDF file twice in a row.
"""
import gc
import os
import tempfile
import unittest

import numpy as np
import pycdfpp


RESOURCES = os.path.join(os.path.dirname(__file__), '..', 'resources')


def _make_tt2000_cdf(n_records, compressed=False):
    """Create an in-memory CDF with a TT2000 Epoch variable of *n_records* records."""
    cdf = pycdfpp.CDF()
    times = np.arange(
        np.datetime64('2020-01-01T00:00:00', 'ns'),
        np.datetime64('2020-01-01T00:00:00', 'ns') + np.timedelta64(n_records, 'ms'),
        np.timedelta64(1, 'ms'),
    )
    tt = pycdfpp.to_tt2000(times)
    cdf.add_variable('Epoch', values=tt, data_type=pycdfpp.DataType.CDF_TIME_TT2000)
    if compressed:
        cdf['Epoch'].compression = pycdfpp.CompressionType.gzip_compression
    # Add a compressed data variable to mimic real MMS files
    data = np.random.randn(n_records).astype(np.float32)
    cdf.add_variable('data', values=data, data_type=pycdfpp.DataType.CDF_REAL4)
    return cdf


def _save_and_reload(cdf):
    """Save a CDF to a temp file, reload it, and return (loaded_cdf, path)."""
    fd, path = tempfile.mkstemp(suffix='.cdf')
    os.close(fd)
    pycdfpp.save(cdf, path)
    return pycdfpp.load(path), path


class TestToDatetime64VariableLifetime(unittest.TestCase):
    """to_datetime64(variable) result must survive deallocation without crash."""

    def _run_for_type(self, var_name, n_records):
        cdf, path = _save_and_reload(_make_tt2000_cdf(n_records))
        try:
            var = pycdfpp.to_datetime64(cdf[var_name])
            self.assertEqual(var.shape[0], n_records)
            # Reassign — the old array is freed here
            var = pycdfpp.to_datetime64(cdf[var_name])
            self.assertEqual(var.shape[0], n_records)
            del var
            gc.collect()
        finally:
            del cdf
            gc.collect()
            os.unlink(path)

    def test_tt2000_small(self):
        self._run_for_type('Epoch', 101)

    def test_tt2000_medium(self):
        self._run_for_type('Epoch', 10_000)

    def test_tt2000_large(self):
        """Large enough to potentially trigger the SIMD vectorized path."""
        self._run_for_type('Epoch', 100_000)

    def test_epoch_from_resource_file(self):
        path = os.path.join(RESOURCES, 'a_cdf.cdf')
        cdf = pycdfpp.load(path)
        for time_var in ('epoch', 'epoch16', 'tt2000'):
            var = pycdfpp.to_datetime64(cdf[time_var])
            var = pycdfpp.to_datetime64(cdf[time_var])
            del var
            gc.collect()

    def test_different_dest_variables(self):
        """Using different destination names — old arrays freed at scope exit."""
        cdf, path = _save_and_reload(_make_tt2000_cdf(50_000))
        try:
            a = pycdfpp.to_datetime64(cdf['Epoch'])
            b = pycdfpp.to_datetime64(cdf['Epoch'])
            self.assertTrue(np.array_equal(a, b))
            del a, b
            gc.collect()
        finally:
            del cdf
            gc.collect()
            os.unlink(path)

    def test_compressed_variable(self):
        cdf, path = _save_and_reload(_make_tt2000_cdf(10_000, compressed=True))
        try:
            var = pycdfpp.to_datetime64(cdf['Epoch'])
            var = pycdfpp.to_datetime64(cdf['Epoch'])
            del var
            gc.collect()
        finally:
            del cdf
            gc.collect()
            os.unlink(path)


class TestOpenReprCycle(unittest.TestCase):
    """Opening and repr'ing CDF files repeatedly must not crash."""

    def test_open_repr_cycle_resource_file(self):
        path = os.path.join(RESOURCES, 'a_cdf.cdf')
        for _ in range(5):
            cdf = pycdfpp.load(path)
            repr(cdf)
            del cdf
            gc.collect()

    def test_open_repr_two_different_files(self):
        paths = [
            os.path.join(RESOURCES, 'a_cdf.cdf'),
            os.path.join(RESOURCES, 'a_compressed_cdf.cdf'),
        ]
        for path in paths:
            cdf = pycdfpp.load(path)
            repr(cdf)
            del cdf
            gc.collect()
        for path in paths:
            cdf = pycdfpp.load(path)
            repr(cdf)
            del cdf
            gc.collect()

    def test_open_repr_synthetic(self):
        cdf_a, path_a = _save_and_reload(_make_tt2000_cdf(5_000))
        cdf_b, path_b = _save_and_reload(_make_tt2000_cdf(10_000))
        try:
            repr(cdf_a)
            repr(cdf_b)
            del cdf_a, cdf_b
            gc.collect()
            # Reopen both
            cdf_a = pycdfpp.load(path_a)
            cdf_b = pycdfpp.load(path_b)
            repr(cdf_a)
            repr(cdf_b)
        finally:
            del cdf_a, cdf_b
            gc.collect()
            os.unlink(path_a)
            os.unlink(path_b)

    def test_open_access_values_repr_cycle(self):
        """Access compressed variable values between repr cycles."""
        path = os.path.join(RESOURCES, 'a_cdf_with_compressed_vars.cdf')
        for _ in range(5):
            cdf = pycdfpp.load(path)
            repr(cdf)
            _ = cdf['var'].values
            _ = cdf['epoch'].values
            repr(cdf)
            del cdf
            gc.collect()

    def test_open_repr_then_to_datetime64(self):
        path = os.path.join(RESOURCES, 'a_cdf.cdf')
        cdf = pycdfpp.load(path)
        repr(cdf)
        var = pycdfpp.to_datetime64(cdf['tt2000'])
        del cdf
        gc.collect()
        # var should still be valid — it owns its own buffer
        self.assertEqual(len(var), 101)
        del var
        gc.collect()
        # Open a second file after everything is freed
        cdf2 = pycdfpp.load(path)
        repr(cdf2)
        var2 = pycdfpp.to_datetime64(cdf2['tt2000'])
        self.assertEqual(len(var2), 101)


if __name__ == '__main__':
    unittest.main()
