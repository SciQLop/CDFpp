#!/usr/bin/env python
# -*- coding: utf-8 -*-
import os
import sys
from datetime import datetime, timedelta
from contextlib import contextmanager
import types
import tempfile
import hashlib
import shutil
import numpy as np
import math
import unittest
import warnings
from glob import glob
import pycdfpp

os.environ['TZ'] = 'UTC'

print(f"Running tests with pycdfpp version: {pycdfpp.__version__} from {pycdfpp.__file__}")

def make_cdf():
    cdf = pycdfpp.CDF()
    cdf.add_attribute("test_attribute", [[1, 2, 3], [datetime(2018, 1, 1), datetime(2018, 1, 2)], "hello\nworld"])
    cdf.add_variable("test_variable",
                     attributes={"attr1": [1, 2, 3], "attr2": [datetime(2018, 1, 1), datetime(2018, 1, 2)]})
    cdf.add_variable("utf8", ['ASCII: ABCDEFG', 'Latin1: ©æêü÷Æ¼®¢¥', 'Chinese: 社安', 'Other: ႡႢႣႤႥႦ'])
    cdf.add_variable("utf8_arr", np.array(['ASCII: ABCDEFG', 'Latin1: ©æêü÷Æ¼®¢¥', 'Chinese: 社安', 'Other: ႡႢႣႤႥႦ']))
    cdf.add_variable("test_CDF_TIME_TT2000").set_values(
        np.arange(1e18, 11e17, 1e16, dtype=np.int64).astype("datetime64[ns]"), pycdfpp.DataType.CDF_TIME_TT2000)
    cdf.add_variable("test_CDF_TIME_TT2000_2").set_values(
        np.arange(1e12, 11e11, 1e10, dtype=np.int64).astype("datetime64[ms]"), pycdfpp.DataType.CDF_TIME_TT2000)
    cdf.add_variable("test_integer_attributes",
                     [[1, 2, 3]],
                     attributes={
                         "PyInts_u8": [1, 2, 3],
                         "PyInts_i8": [-1, 2, -3],
                         "PyInts_u16": [1, 2, 32000],
                         "PyInts_i16": [-1, 2, -30000],
                         "PyInts_u32": [1, 2, 32000000],
                         "PyInts_i32": [-1, 2, -30000000],
                         "PyInts_i64": [-1, 2, -30000000000],
                         "numpy_u8": [np.uint8(1), np.uint8(2), np.uint8(3)],
                         "numpy_i8": [np.int8(1), np.int8(2), np.int8(3)],
                         "numpy_u16": [np.uint16(1), np.uint16(2), np.uint16(3)],
                         "numpy_i16": [np.int16(1), np.int16(2), np.int16(3)],
                         "numpy_u32": [np.uint32(1), np.uint32(2), np.uint32(3)],
                         "numpy_i32": [np.int32(1), np.int32(2), np.int32(3)],
                         "numpy_i64": [np.int64(1), np.int64(2), np.int64(3)],
                         "numpy_mixed_i16": [np.uint8(1), np.int16(-2), np.int16(3)]
                     }
                     )

    cdf.add_variable("tt2000_special_values",
                     np.arange(1e18, 11e17, 1e16, dtype=np.int64).astype("datetime64[ns]"),
                     data_type=pycdfpp.DataType.CDF_TIME_TT2000,
                     attributes={"FILLVAL": [pycdfpp.default_fill_value(pycdfpp.DataType.CDF_TIME_TT2000)]})
    cdf.add_variable("epoch_special_values",
                     np.arange(1e18, 11e17, 1e16, dtype=np.int64).astype("datetime64[ns]"),
                     data_type=pycdfpp.DataType.CDF_EPOCH,
                     attributes={"FILLVAL": [pycdfpp.default_fill_value(pycdfpp.DataType.CDF_EPOCH)]})
    cdf.add_variable("epoch16_special_values",
                     np.arange(1e18, 11e17, 1e16, dtype=np.int64).astype("datetime64[ns]"),
                     data_type=pycdfpp.DataType.CDF_EPOCH16,
                     attributes={"FILLVAL": [pycdfpp.default_fill_value(pycdfpp.DataType.CDF_EPOCH16)]})
    return cdf


@contextmanager
def temporary_file(suffix=""):
    """A path for a new file, in a temporary directory. NamedTemporaryFile can't be used for
    saving: on Windows, its file is open, so it can't be opened again for writing."""
    with tempfile.TemporaryDirectory() as tmp:
        yield types.SimpleNamespace(name=os.path.join(tmp, "file" + suffix))


class PycdfCreateCDFTest(unittest.TestCase):
    def test_can_create_an_empty_CDF_object(self):
        cdf = pycdfpp.CDF()
        self.assertIsNotNone(cdf)

    def test_compare_identical_cdfs(self):
        self.assertEqual(make_cdf(), make_cdf())
        self.assertEqual(pycdfpp.CDF(), pycdfpp.CDF())

    def test_compare_differents_cdfs(self):
        self.assertNotEqual(make_cdf(), pycdfpp.CDF())

    def test_in_memory_save_load_empty_CDF_object(self):
        cdf = pycdfpp.CDF()
        self.assertIsNotNone(pycdfpp.load(pycdfpp.save(cdf)))

    def test_overwrite_attribute(self):
        cdf = make_cdf()
        self.assertEqual(cdf.attributes["test_attribute"][0], [1, 2, 3])
        self.assertEqual(cdf.attributes["test_attribute"][2], "hello\nworld")
        cdf.attributes["test_attribute"].set_values(
            ["hello\nworld", [datetime(2018, 1, 1), datetime(2018, 1, 2)], [1, 2, 3]])
        self.assertEqual(cdf.attributes["test_attribute"][0], "hello\nworld")
        self.assertEqual(cdf.attributes["test_attribute"][2], [1, 2, 3])

        cdf["test_variable"].attributes["attr1"].set_value([3, 2, 1])
        self.assertEqual(cdf["test_variable"].attributes["attr1"][0], [3, 2, 1])
        self.assertEqual(cdf["test_variable"].attributes["attr1"].value, [3, 2, 1])

    def test_inter_attributes_fits_min_integer_size(self):
        cdf = make_cdf()
        self.assertEqual(cdf["test_integer_attributes"].attributes["PyInts_u8"].type(), pycdfpp.DataType.CDF_UINT1)
        self.assertEqual(cdf["test_integer_attributes"].attributes["PyInts_i8"].type(), pycdfpp.DataType.CDF_INT1)
        self.assertEqual(cdf["test_integer_attributes"].attributes["PyInts_u16"].type(), pycdfpp.DataType.CDF_UINT2)
        self.assertEqual(cdf["test_integer_attributes"].attributes["PyInts_i16"].type(), pycdfpp.DataType.CDF_INT2)
        self.assertEqual(cdf["test_integer_attributes"].attributes["PyInts_u32"].type(), pycdfpp.DataType.CDF_UINT4)
        self.assertEqual(cdf["test_integer_attributes"].attributes["PyInts_i32"].type(), pycdfpp.DataType.CDF_INT4)
        self.assertEqual(cdf["test_integer_attributes"].attributes["PyInts_i64"].type(), pycdfpp.DataType.CDF_INT8)

    def test_inter_attributes_respect_numpy_types(self):
        cdf = make_cdf()
        self.assertEqual(cdf["test_integer_attributes"].attributes["numpy_u8"].type(), pycdfpp.DataType.CDF_UINT1)
        self.assertEqual(cdf["test_integer_attributes"].attributes["numpy_i8"].type(), pycdfpp.DataType.CDF_INT1)
        self.assertEqual(cdf["test_integer_attributes"].attributes["numpy_u16"].type(), pycdfpp.DataType.CDF_UINT2)
        self.assertEqual(cdf["test_integer_attributes"].attributes["numpy_i16"].type(), pycdfpp.DataType.CDF_INT2)
        self.assertEqual(cdf["test_integer_attributes"].attributes["numpy_u32"].type(), pycdfpp.DataType.CDF_UINT4)
        self.assertEqual(cdf["test_integer_attributes"].attributes["numpy_i32"].type(), pycdfpp.DataType.CDF_INT4)
        self.assertEqual(cdf["test_integer_attributes"].attributes["numpy_i64"].type(), pycdfpp.DataType.CDF_INT8)
        self.assertEqual(cdf["test_integer_attributes"].attributes["numpy_mixed_i16"].type(),
                         pycdfpp.DataType.CDF_INT2)

    def test_default_fill_values(self):
        # https://spdf.gsfc.nasa.gov/istp_guide/vattributes.html#FILLVAL
        cdf = make_cdf()
        self.assertEqual(cdf["tt2000_special_values"].attributes["FILLVAL"][0][0],
                         pycdfpp.tt2000_t(-9223372036854775808))
        self.assertEqual(str(cdf["tt2000_special_values"].attributes["FILLVAL"][0][0]), '9999-12-31T23:59:59.999999999')
        self.assertEqual(cdf["epoch_special_values"].attributes["FILLVAL"][0][0], pycdfpp.epoch(-1e31))
        self.assertEqual(str(cdf["epoch_special_values"].attributes["FILLVAL"][0][0]), '9999-12-31T23:59:59.999')

    def test_can_create_CDF_attributes(self):
        cdf = pycdfpp.CDF()
        cdf.add_attribute("test_attribute", [[1, 2, 3], [datetime(2018, 1, 1), datetime(2018, 1, 2)], "hello\nworld"])

    def test_can_create_a_CDF_attribute_with_no_entry(self):
        cdf = pycdfpp.CDF()
        cdf.add_attribute("EMPTY", [])
        cdf.add_attribute("EMPTY_TOO", [], [])
        for reloaded in (cdf, pycdfpp.load(pycdfpp.save(cdf))):
            for name in ("EMPTY", "EMPTY_TOO"):
                self.assertEqual(len(reloaded.attributes[name]), 0)

    def test_can_create_CDF_attributes_with_given_type(self):
        cdf = pycdfpp.CDF()
        cdf.add_attribute("ints", [[1, 2, 3], [128, 256, 512]],
                          [pycdfpp.DataType.CDF_UINT1, pycdfpp.DataType.CDF_UINT2])

    def test_can_create_CDF_attributes_tt2000_special_values(self):
        cdf = pycdfpp.CDF()
        cdf.add_attribute("tt2000", [[pycdfpp.tt2000_t(-9223372036854775808), pycdfpp.tt2000_t(-9223372036854775807),
                                      pycdfpp.tt2000_t(-9223372036854775805)]])
        self.assertEqual(str(cdf.attributes["tt2000"][0][0]), '9999-12-31T23:59:59.999999999')
        self.assertEqual(str(cdf.attributes["tt2000"][0][1]), '0000-01-01T00:00:00.000000000')
        self.assertEqual(str(cdf.attributes["tt2000"][0][2]), '9999-12-31T23:59:59.999999999')

    def test_can_create_variable_and_attributes_at_once(self):
        cdf = pycdfpp.CDF()
        cdf.add_variable("test_variable",
                         attributes={"attr1": [1, 2, 3], "attr2": [datetime(2018, 1, 1), datetime(2018, 1, 2)]})
        self.assertListEqual(cdf["test_variable"].attributes["attr1"][0], [1, 2, 3])
        self.assertListEqual(cdf["test_variable"].attributes["attr2"][0],
                             [pycdfpp.to_tt2000(datetime(2018, 1, 1)), pycdfpp.to_tt2000(datetime(2018, 1, 2))])

    def test_add_variable_rejects_unexpected_kwargs(self):
        cdf = pycdfpp.CDF()
        with self.assertRaises(TypeError):
            cdf.add_variable("var", values=[1, 2, 3], ttributes={"FIELDNAM": "test"})

    def test_add_cdf_attribute_rejects_unexpected_kwargs(self):
        cdf = pycdfpp.CDF()
        with self.assertRaises(TypeError):
            cdf.add_attribute("attr", entries_values=[[1, 2]], entry_types=[None])

    def test_add_variable_attribute_rejects_unexpected_kwargs(self):
        cdf = pycdfpp.CDF()
        cdf.add_variable("var", values=[1, 2, 3])
        with self.assertRaises(TypeError):
            cdf["var"].add_attribute("attr", values=[1, 2], dtype=pycdfpp.DataType.CDF_INT4)

    def test_can_create_an_empty_variable(self):
        cdf = pycdfpp.CDF()
        cdf.add_variable("test_variable", values=np.ones((0, 100, 10), dtype=np.float32))
        self.assertEqual(cdf["test_variable"].shape, (0, 100, 10))

    def test_can_create_an_nd_string_variable(self):
        cdf = pycdfpp.CDF()
        cdf.add_variable("test_1d_variable", values=["hello", "world"])
        cdf.add_variable("test_2d_variable", values=[["hello", "world"], ["12345", "67890"]])
        cdf.add_variable("utf8", ['ASCII: ABCDEFG', 'Latin1: ©æêü÷Æ¼®¢¥', 'Chinese: 社安', 'Other: ႡႢႣႤႥႦ'])
        cdf.add_variable("utf8_arr",
                         np.array(['ASCII: ABCDEFG', 'Latin1: ©æêü÷Æ¼®¢¥', 'Chinese: 社安', 'Other: ႡႢႣႤႥႦ']))
        self.assertTrue(pycdfpp.load(pycdfpp.save(cdf)) == cdf)

    def test_can_save_an_empty_CDF_object(self):
        with temporary_file() as f:
            self.assertTrue(pycdfpp.save(pycdfpp.CDF(), f.name))

    def test_can_save_a_CDF_object_with_several_numeric_1d_variables(self):
        with temporary_file() as f:
            cdf = pycdfpp.CDF()
            for dtype in (np.float64, np.float32, np.int8, np.int16, np.int32, np.int64, np.uint8, np.uint16,
                          np.uint32):
                cdf.add_variable(f"test_{dtype}", np.ones((10), dtype=dtype))
            self.assertTrue(pycdfpp.save(cdf, f.name))

    def test_can_save_a_CDF_object_with_several_numeric_nd_variables(self):
        with temporary_file() as f:
            cdf = pycdfpp.CDF()
            for dtype in (np.float64, np.float32, np.int8, np.int16, np.int32, np.int64, np.uint8, np.uint16,
                          np.uint32):
                cdf.add_variable(f"test_{dtype}").set_values(np.ones((3, 2, 1), dtype=dtype))
            self.assertTrue(pycdfpp.save(cdf, f.name))

    def test_can_save_a_CDF_object_with_several_datetime_variables(self):
        with temporary_file() as f:
            cdf = pycdfpp.CDF()
            for cdf_type in (pycdfpp.DataType.CDF_TIME_TT2000, pycdfpp.DataType.CDF_EPOCH,
                             pycdfpp.DataType.CDF_EPOCH16):
                cdf.add_variable(f"test_{cdf_type}").set_values(
                    np.arange(1e18, 11e17, 1e16, dtype=np.int64).astype("datetime64[ns]"), cdf_type)
            self.assertTrue(pycdfpp.save(cdf, f.name))

    def test_can_save_a_GZ_compressed_CDF(self):
        with temporary_file() as f:
            cdf = pycdfpp.CDF()
            for dtype in (np.float64, np.float32, np.int8, np.int16, np.int32, np.int64, np.uint8, np.uint16,
                          np.uint32):
                cdf.add_variable(f"test_{dtype}").set_values(np.ones((3, 2, 1), dtype=dtype))
            cdf.compression = pycdfpp.CompressionType.gzip_compression
            self.assertTrue(pycdfpp.save(cdf, f.name))

    def test_can_save_a_cdf_with_an_empty_var(self):
        # https://github.com/SciQLop/CDFpp/issues/25
        with temporary_file() as f:
            cdf = pycdfpp.CDF()
            cdf.add_variable("test", data_type=pycdfpp.DataType.CDF_TIME_TT2000)
            self.assertTrue(pycdfpp.save(cdf, f.name))

    def test_can_save_a_cdf_with_an_empty_compressed_var(self):
        # https://github.com/SciQLop/CDFpp/issues/25
        cdf = pycdfpp.CDF()
        cdf.add_variable("test", data_type=pycdfpp.DataType.CDF_TIME_TT2000, compression=pycdfpp.CompressionType.gzip_compression)
        reloaded_cdf = pycdfpp.load(pycdfpp.save(cdf))
        self.assertEqual(reloaded_cdf["test"].shape, (0,))
        self.assertEqual(reloaded_cdf["test"].compression, pycdfpp.CompressionType.gzip_compression)


GZIP = pycdfpp.CompressionType.gzip_compression


def compressible_values():
    return np.random.default_rng(0).integers(0, 50, 200_000).astype(np.int32)


def gzip_cdf(file_level=None, variable_level=None):
    cdf = pycdfpp.CDF()
    cdf.add_variable("x", compressible_values(), compression=GZIP)
    if file_level is not None:
        cdf.compression = GZIP
        cdf.compression_level = file_level
    if variable_level is not None:
        cdf["x"].compression_level = variable_level
    return cdf


class PycdfGzipLevelTest(unittest.TestCase):
    def test_the_default_level_is_6(self):
        cdf = gzip_cdf()
        self.assertEqual(cdf.compression_level, 6)
        self.assertEqual(cdf["x"].compression_level, 6)
        self.assertEqual(pycdfpp.load(pycdfpp.save(cdf))["x"].compression_level, 6)

    def test_variable_levels_round_trip(self):
        for level in range(1, 10):
            with self.subTest(level=level):
                reloaded = pycdfpp.load(pycdfpp.save(gzip_cdf(variable_level=level)))
                self.assertEqual(reloaded["x"].compression_level, level)
                self.assertTrue(np.array_equal(reloaded["x"].values, compressible_values()))

    def test_file_levels_round_trip(self):
        for level in range(1, 10):
            with self.subTest(level=level):
                reloaded = pycdfpp.load(pycdfpp.save(gzip_cdf(file_level=level)))
                self.assertEqual(reloaded.compression_level, level)
                self.assertTrue(np.array_equal(reloaded["x"].values, compressible_values()))

    def test_add_variable_takes_a_level(self):
        cdf = pycdfpp.CDF()
        cdf.add_variable("x", compressible_values(), compression=GZIP, compression_level=2)
        self.assertEqual(pycdfpp.load(pycdfpp.save(cdf))["x"].compression_level, 2)

    def test_the_level_drives_the_compressor(self):
        for scope in ("variable_level", "file_level"):
            with self.subTest(scope=scope):
                sizes = [len(bytes(pycdfpp.save(gzip_cdf(**{scope: level})))) for level in (1, 9)]
                self.assertGreater(sizes[0], sizes[1])

    def test_levels_written_by_the_nasa_library_survive_a_round_trip(self):
        resources = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "resources")
        file_level = pycdfpp.load(os.path.join(resources, "a_compressed_cdf.cdf"))
        variables_level = pycdfpp.load(os.path.join(resources, "a_cdf_with_compressed_vars.cdf"))
        for cdf in (file_level, pycdfpp.load(pycdfpp.save(file_level))):
            self.assertEqual(cdf.compression_level, 5)
        for cdf in (variables_level, pycdfpp.load(pycdfpp.save(variables_level))):
            self.assertEqual({cdf[name].compression_level for name in cdf
                              if cdf[name].compression == GZIP}, {9})

    def test_levels_outside_1_to_9_are_rejected(self):
        cdf = gzip_cdf()
        for level in (0, 10, -1):
            with self.subTest(level=level):
                with self.assertRaises(ValueError):
                    cdf.compression_level = level
                with self.assertRaises(ValueError):
                    cdf["x"].compression_level = level


RESOURCES = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "resources")


class PycdfSparseRecordsTest(unittest.TestCase):
    # Flags set by tests/resources/make_sparse_records.c
    NASA_FLAGS = {"prev": "prev_sparse_records", "prev_with_fillval": "prev_sparse_records",
                  "no_sparse_gap": "no_sparse_records"}

    def test_the_default_is_no_sparse_records(self):
        cdf = pycdfpp.CDF()
        cdf.add_variable("x", np.arange(10.))
        self.assertEqual(cdf["x"].sparse_records, pycdfpp.SparseRecords.no_sparse_records)

    def test_sparse_records_round_trip(self):
        for flag in pycdfpp.SparseRecords:
            with self.subTest(flag=flag):
                cdf = pycdfpp.CDF()
                cdf.add_variable("x", np.arange(10.), sparse_records=flag)
                cdf.add_variable("y", np.arange(10.))
                cdf["y"].sparse_records = flag
                reloaded = pycdfpp.load(pycdfpp.save(cdf))
                for name in ("x", "y"):
                    self.assertEqual(reloaded[name].sparse_records, flag)
                    self.assertTrue(np.array_equal(reloaded[name].values, np.arange(10.)))

    def test_flags_written_by_the_nasa_library_survive_a_round_trip(self):
        original = pycdfpp.load(os.path.join(RESOURCES, "sparse_records.cdf"))
        reloaded = pycdfpp.load(pycdfpp.save(original))
        for cdf in (original, reloaded):
            for name in cdf:
                with self.subTest(name=name):
                    expected = getattr(pycdfpp.SparseRecords, self.NASA_FLAGS.get(name, "pad_sparse_records"))
                    self.assertEqual(cdf[name].sparse_records, expected)
                    self.assertTrue(np.array_equal(reloaded[name].values, original[name].values))


class PycdfPadValueTest(unittest.TestCase):
    D = pycdfpp.DataType
    # (values of an empty variable, data type, pad value)
    CASES = [
        (np.empty((0, 3), dtype=np.float32), D.CDF_REAL4, np.float32(-1e-30)),
        (np.empty((0,), dtype=np.float64), D.CDF_DOUBLE, -1.5),
        (np.empty((0,), dtype=np.int32), D.CDF_INT4, 7),
        (np.empty((0,), dtype=np.uint8), D.CDF_UINT1, 12),
        (np.empty((0,), dtype="datetime64[ns]"), D.CDF_TIME_TT2000, datetime(2020, 1, 1)),
        (np.array(["abc"]), D.CDF_CHAR, "xyz"),
    ]

    def test_there_is_no_pad_value_by_default(self):
        cdf = pycdfpp.CDF()
        cdf.add_variable("x", np.arange(10.))
        for c in (cdf, pycdfpp.load(pycdfpp.save(cdf))):
            self.assertIsNone(c["x"].pad_value)

    def test_pad_values_round_trip_like_attributes(self):
        for values, data_type, pad in self.CASES:
            with self.subTest(data_type=data_type):
                cdf = pycdfpp.CDF()
                var = cdf.add_variable("x", values, data_type, pad_value=pad)
                var.add_attribute("SAME_AS_PAD", pad, data_type)
                for c in (cdf, pycdfpp.load(pycdfpp.save(cdf))):
                    self.assertEqual(c["x"].pad_value, c["x"].attributes["SAME_AS_PAD"].value)

    def test_the_pad_value_can_be_changed_and_removed(self):
        cdf = pycdfpp.CDF()
        cdf.add_variable("x", np.arange(10.))
        cdf["x"].pad_value = 3.
        self.assertEqual(pycdfpp.load(pycdfpp.save(cdf))["x"].pad_value, [3.])
        cdf["x"].pad_value = None
        self.assertIsNone(pycdfpp.load(pycdfpp.save(cdf))["x"].pad_value)

    def test_a_pad_value_that_no_longer_fits_the_variable_is_refused_at_save(self):
        cdf = pycdfpp.CDF()
        cdf.add_variable("x", np.arange(10, dtype=np.int16), pad_value=7)
        cdf["x"].set_values(np.arange(10.), force=True)
        with self.assertRaises(ValueError):
            pycdfpp.save(cdf)

    def test_pad_values_written_by_the_nasa_library_survive_a_round_trip(self):
        # Set by tests/resources/make_sparse_records.c; NASA's library writes the default pad
        # value of the type for the other variables.
        explicit = {"pad_explicit": [1.5], "pad_with_fillval": [1.5], "prev": [7],
                    "compressed_pad": [2.5]}
        original = pycdfpp.load(os.path.join(RESOURCES, "sparse_records.cdf"))
        reloaded = pycdfpp.load(pycdfpp.save(original))
        for name in original:
            with self.subTest(name=name):
                self.assertIsNotNone(original[name].pad_value)
                self.assertEqual(original[name].pad_value,
                                 explicit.get(name, original[name].pad_value))
                self.assertEqual(reloaded[name].pad_value, original[name].pad_value)


def resaved(cdf):
    return pycdfpp.load(bytes(pycdfpp.save(cdf)))


def same_values(values, other):
    # Fill values are often NaN, which never equals itself
    if values.dtype.kind in "fc":
        return np.array_equal(values, other, equal_nan=True)
    return np.array_equal(values, other)


def assert_same_attributes(test, attributes, other):
    test.assertEqual(sorted(attributes), sorted(other))
    for name in attributes:
        test.assertEqual(str(attributes[name]), str(other[name]))


def assert_same_content(test, cdf, other):
    test.assertEqual(sorted(cdf), sorted(other))
    for name in cdf:
        with test.subTest(variable=name):
            test.assertEqual(cdf[name].type, other[name].type)
            test.assertEqual(cdf[name].shape, other[name].shape)
            test.assertTrue(same_values(cdf[name].values, other[name].values))
            assert_same_attributes(test, cdf[name].attributes, other[name].attributes)
            for attr in cdf[name].attributes:
                test.assertEqual(cdf[name].attributes[attr].type(), other[name].attributes[attr].type())
            test.assertEqual(str(cdf[name].pad_value), str(other[name].pad_value))
    assert_same_attributes(test, cdf.attributes, other.attributes)


class PycdfMajorityTest(unittest.TestCase):
    # The same variables and values, written by NASA's library in both majorities
    ROW = os.path.join(RESOURCES, "a_cdf.cdf")
    COLUMN = os.path.join(RESOURCES, "a_col_major_cdf.cdf")

    def test_the_majority_can_be_set(self):
        cdf = pycdfpp.CDF()
        cdf.majority = pycdfpp.Majority.column
        self.assertEqual(cdf.majority, pycdfpp.Majority.column)
        self.assertEqual(resaved(cdf).majority, pycdfpp.Majority.column)

    def test_column_major_files_round_trip(self):
        column = pycdfpp.load(self.COLUMN)
        reloaded = resaved(column)
        self.assertEqual(reloaded.majority, pycdfpp.Majority.column)
        assert_same_content(self, column, reloaded)

    def test_a_row_major_file_saved_as_column_major_matches_nasa_s_column_major_file(self):
        cdf = pycdfpp.load(self.ROW)
        cdf.majority = pycdfpp.Majority.column
        reloaded = resaved(cdf)
        self.assertEqual(reloaded.majority, pycdfpp.Majority.column)
        assert_same_content(self, pycdfpp.load(self.COLUMN), reloaded)

    def test_record_varying_string_arrays_match_nasa_s_column_major_file(self):
        nasa = pycdfpp.load(os.path.join(RESOURCES, "col_major_strings.cdf"))
        cdf = pycdfpp.CDF()
        cdf.majority = pycdfpp.Majority.column
        cdf.add_variable("strings", nasa["strings"].values, nasa["strings"].type)
        cdf.add_variable("numbers", nasa["numbers"].values)
        saved = bytes(pycdfpp.save(cdf))
        self.assertIn(b"".join(np.asfortranarray(r).tobytes(order="F")
                               for r in nasa["numbers"].values), saved)
        self.assertIn(b"r0[00]r0[10]r0[01]r0[11]r0[02]r0[12]r1[00]", saved)
        for name in ("strings", "numbers"):
            self.assertEqual(pycdfpp.load(saved)[name].values.tolist(), nasa[name].values.tolist())

    def test_records_are_stored_in_column_major_order(self):
        values = np.arange(2 * 3 * 4 * 5, dtype=np.float64).reshape(2, 3, 4, 5)
        cdf = pycdfpp.CDF()
        cdf.add_variable("x", values)
        cdf.majority = pycdfpp.Majority.column
        column_records = np.stack([np.asfortranarray(record).ravel(order="K") for record in values])
        self.assertIn(column_records.tobytes(), bytes(pycdfpp.save(cdf)))
        self.assertTrue(np.array_equal(resaved(cdf)["x"].values, values))

    def test_variables_bigger_than_a_conversion_chunk(self):
        # Several 1 MiB chunks uncompressed, several 256 KiB blocks compressed
        values = np.random.default_rng(0).random((20_000, 3, 5))
        for compression in (pycdfpp.CompressionType.no_compression, GZIP):
            with self.subTest(compression=compression):
                cdf = pycdfpp.CDF()
                cdf.add_variable("x", values, compression=compression)
                cdf.majority = pycdfpp.Majority.column
                cdf.encoding = pycdfpp.Encoding.network
                saved = bytes(pycdfpp.save(cdf))
                if compression == pycdfpp.CompressionType.no_compression:
                    self.assertIn(np.stack([r.ravel(order="F") for r in values]).astype(">f8").tobytes(),
                                  saved)
                self.assertTrue(np.array_equal(pycdfpp.load(saved)["x"].values, values))

    def test_converting_leaves_the_cdf_and_borrowed_arrays_unchanged(self):
        values = np.arange(24, dtype=np.int32).reshape(2, 3, 4)
        borrowed = values.copy()
        cdf = pycdfpp.CDF()
        cdf.add_variable("x", values)
        cdf.add_variable("borrowed", borrowed, copy=False)
        cdf.majority = pycdfpp.Majority.column
        cdf.encoding = pycdfpp.Encoding.network
        reloaded = resaved(cdf)
        for c in (cdf, reloaded):
            for name in ("x", "borrowed"):
                self.assertTrue(np.array_equal(c[name].values, values))
        self.assertTrue(np.array_equal(borrowed, values))

    def test_column_major_and_network_together(self):
        cdf = pycdfpp.load(self.ROW)
        cdf.majority = pycdfpp.Majority.column
        cdf.encoding = pycdfpp.Encoding.network
        reloaded = resaved(cdf)
        self.assertEqual((reloaded.majority, reloaded.encoding),
                         (pycdfpp.Majority.column, pycdfpp.Encoding.network))
        assert_same_content(self, pycdfpp.load(self.COLUMN), reloaded)


class PycdfEncodingTest(unittest.TestCase):
    HOST = pycdfpp.Encoding.IBMPC if sys.byteorder == "little" else pycdfpp.Encoding.network
    # Written by NASA's library in network encoding
    NETWORK_FILES = ["ac_h2_sis_20101105_v06.cdf", "ac_h0_mfi_00000000_v01.cdf",
                     "thg_l2_mag_mek_00000000_v01.cdf"]

    def test_a_new_cdf_uses_the_host_encoding(self):
        self.assertEqual(pycdfpp.CDF().encoding, self.HOST)

    def test_files_keep_their_encoding(self):
        for name in self.NETWORK_FILES:
            with self.subTest(file=name):
                original = pycdfpp.load(os.path.join(RESOURCES, name))
                self.assertEqual(original.encoding, pycdfpp.Encoding.network)
                reloaded = resaved(original)
                self.assertEqual(reloaded.encoding, pycdfpp.Encoding.network)
                assert_same_content(self, original, reloaded)

    def test_any_file_can_be_saved_in_another_byte_order(self):
        for encoding in (pycdfpp.Encoding.network, pycdfpp.Encoding.IBMPC):
            for name in ["a_cdf.cdf", "sparse_records.cdf", "a_compressed_cdf.cdf",
                         "a_cdf_with_compressed_vars.cdf"] + self.NETWORK_FILES:
                with self.subTest(file=name, encoding=encoding):
                    original = pycdfpp.load(os.path.join(RESOURCES, name))
                    original.encoding = encoding
                    reloaded = resaved(original)
                    self.assertEqual(reloaded.encoding, encoding)
                    assert_same_content(self, original, reloaded)

    def test_values_are_stored_in_the_file_byte_order(self):
        cdf = pycdfpp.CDF()
        cdf.add_variable("x", np.arange(100, dtype=np.float64))
        cdf.encoding = pycdfpp.Encoding.network
        saved = bytes(pycdfpp.save(cdf))
        self.assertIn(np.arange(100, dtype=">f8").tobytes(), saved)
        self.assertTrue(np.array_equal(cdf["x"].values, np.arange(100, dtype=np.float64)))

    def test_encodings_without_ieee_floats_are_refused(self):
        cdf = pycdfpp.CDF()
        for name in ("VAX", "ALPHAVMSd", "ALPHAVMSg", "IA64VMSd", "IA64VMSg"):
            with self.subTest(encoding=name), self.assertRaises(ValueError):
                cdf.encoding = getattr(pycdfpp.Encoding, name)


def has_valid_md5(data: bytes):
    return hashlib.md5(data[:-16]).digest() == data[-16:]


class PycdfChecksumTest(unittest.TestCase):
    MD5 = pycdfpp.Checksum.md5_checksum

    def md5_cdf(self):
        cdf = pycdfpp.CDF()
        cdf.add_variable("values", np.arange(10, dtype=np.float64))
        cdf.checksum = self.MD5
        return cdf

    def test_there_is_no_checksum_by_default(self):
        cdf = pycdfpp.CDF()
        self.assertEqual(cdf.checksum, pycdfpp.Checksum.no_checksum)
        self.assertFalse(has_valid_md5(bytes(pycdfpp.save(cdf))))

    def test_files_written_by_the_nasa_library_keep_their_checksum(self):
        # Made by tests/resources/make_checksum_cdf.py
        original = pycdfpp.load(os.path.join(RESOURCES, "checksum.cdf"))
        self.assertEqual(original.checksum, self.MD5)
        saved = bytes(pycdfpp.save(original))
        self.assertTrue(has_valid_md5(saved))
        reloaded = pycdfpp.load(saved)
        self.assertEqual(reloaded.checksum, self.MD5)
        assert_same_content(self, original, reloaded)

    def test_the_file_ends_with_the_md5_of_the_rest(self):
        with open(os.path.join(RESOURCES, "checksum.cdf"), "rb") as nasa_file:
            self.assertTrue(has_valid_md5(nasa_file.read()))
        for compression in (pycdfpp.CompressionType.no_compression, GZIP):
            for encoding in (pycdfpp.Encoding.IBMPC, pycdfpp.Encoding.network):
                with self.subTest(compression=compression, encoding=encoding):
                    cdf = self.md5_cdf()
                    cdf.compression = compression
                    cdf.encoding = encoding
                    saved = bytes(pycdfpp.save(cdf))
                    self.assertTrue(has_valid_md5(saved))
                    self.assertTrue(np.array_equal(pycdfpp.load(saved)["values"].values,
                                                   np.arange(10, dtype=np.float64)))

    def test_files_saved_to_disk_have_the_checksum_too(self):
        with tempfile.TemporaryDirectory() as folder:
            path = os.path.join(folder, "md5.cdf")
            self.assertTrue(pycdfpp.save(self.md5_cdf(), path))
            with open(path, "rb") as f:
                saved = f.read()
        self.assertTrue(has_valid_md5(saved))
        self.assertEqual(saved, bytes(pycdfpp.save(self.md5_cdf())))

    def test_the_checksum_can_be_removed(self):
        cdf = pycdfpp.load(os.path.join(RESOURCES, "checksum.cdf"))
        cdf.checksum = pycdfpp.Checksum.no_checksum
        saved = bytes(pycdfpp.save(cdf))
        self.assertFalse(has_valid_md5(saved))
        self.assertEqual(pycdfpp.load(saved).checksum, pycdfpp.Checksum.no_checksum)


EXPERIMENTAL_CODECS = [getattr(pycdfpp.CompressionType, name)
                       for name in ("zstd_compression", "blosc2_compression")
                       if hasattr(pycdfpp.CompressionType, name)]


def cdf_with_one_variable(compression):
    cdf = pycdfpp.CDF()
    cdf.add_variable("x", np.arange(100.), compression=compression)
    return cdf


@unittest.skipUnless(EXPERIMENTAL_CODECS, "pycdfpp built without experimental codecs")
class PycdfExperimentalCodecsTest(unittest.TestCase):
    def test_saving_variables_with_an_experimental_codec_warns(self):
        for codec in EXPERIMENTAL_CODECS:
            with self.subTest(codec=codec), self.assertWarns(pycdfpp.ExperimentalCompressionWarning):
                pycdfpp.save(cdf_with_one_variable(codec))

    def test_saving_a_whole_file_with_an_experimental_codec_warns(self):
        for codec in EXPERIMENTAL_CODECS:
            cdf = cdf_with_one_variable(pycdfpp.CompressionType.no_compression)
            cdf.compression = codec
            with self.subTest(codec=codec), self.assertWarns(pycdfpp.ExperimentalCompressionWarning):
                pycdfpp.save(cdf)

    def test_experimental_codecs_round_trip(self):
        for codec in EXPERIMENTAL_CODECS:
            with self.subTest(codec=codec), warnings.catch_warnings():
                warnings.simplefilter("ignore", pycdfpp.ExperimentalCompressionWarning)
                reloaded = pycdfpp.load(bytes(pycdfpp.save(cdf_with_one_variable(codec))))
                self.assertEqual(reloaded["x"].compression, codec)
                self.assertTrue(np.array_equal(reloaded["x"].values, np.arange(100.)))

    def test_saving_to_a_file_warns_too(self):
        with temporary_file(suffix=".cdf") as f, self.assertWarns(pycdfpp.ExperimentalCompressionWarning):
            self.assertTrue(pycdfpp.save(cdf_with_one_variable(EXPERIMENTAL_CODECS[0]), f.name))


class PycdfValuesAreCopiedTest(unittest.TestCase):
    """A variable owns its values: changing the source array afterwards doesn't change it,
    whether the source is an array, a view or another variable's values."""

    def test_views_and_variable_values_are_copied(self):
        base = np.arange(120, dtype=np.float64).reshape(40, 3)
        other = pycdfpp.CDF()
        other.add_variable("x", base.copy())
        for label, source in (("array", base.copy()), ("view", base[10:30]),
                              ("reshaped view", base.reshape(20, 6)),
                              ("other variable", other["x"].values)):
            with self.subTest(source=label):
                expected = source.copy()
                cdf = pycdfpp.CDF()
                cdf.add_variable("y", source)
                source[...] = -1
                self.assertTrue(np.array_equal(cdf["y"].values, expected))


class PycdfZeroCopyTest(unittest.TestCase):
    """With copy=False a variable borrows the array's memory until its values are read or
    changed: saving writes straight from the array."""

    def borrowing_cdf(self, values, compression=pycdfpp.CompressionType.no_compression):
        cdf = pycdfpp.CDF()
        cdf.add_variable("x", values, compression=compression, copy=False)
        return cdf

    def test_saved_file_holds_the_array_values(self):
        values = np.arange(3000, dtype=np.float64).reshape(1000, 3)
        for compression in (pycdfpp.CompressionType.no_compression,
                            pycdfpp.CompressionType.gzip_compression):
            with self.subTest(compression=compression):
                saved = pycdfpp.load(pycdfpp.save(self.borrowing_cdf(values, compression)))
                self.assertTrue(np.array_equal(saved["x"].values, values))

    def test_array_is_borrowed_until_saved(self):
        values = np.zeros((10, 2), dtype=np.int32)
        cdf = self.borrowing_cdf(values)
        values[3] = 7
        saved = pycdfpp.load(pycdfpp.save(cdf))
        self.assertEqual(saved["x"].values[3].tolist(), [7, 7])

    def test_variable_keeps_the_array_alive(self):
        cdf = self.borrowing_cdf(np.arange(100, dtype=np.uint16))
        saved = pycdfpp.load(pycdfpp.save(cdf))
        self.assertTrue(np.array_equal(saved["x"].values, np.arange(100, dtype=np.uint16)))

    def test_array_is_released_with_the_variable(self):
        values = np.arange(100, dtype=np.float32)
        refs = sys.getrefcount(values)
        cdf = self.borrowing_cdf(values)
        self.assertGreater(sys.getrefcount(values), refs)
        del cdf
        self.assertEqual(sys.getrefcount(values), refs)

    def test_reading_values_copies_them_and_releases_the_array(self):
        values = np.arange(100, dtype=np.float32)
        refs = sys.getrefcount(values)
        cdf = self.borrowing_cdf(values)
        cdf["x"].values[0] = 42
        self.assertEqual(values[0], 0)
        self.assertEqual(sys.getrefcount(values), refs)

    def test_set_values_can_borrow(self):
        values = np.zeros(5, dtype=np.float64)
        cdf = pycdfpp.CDF()
        cdf.add_variable("x").set_values(values, copy=False)
        values[1] = 3
        saved = pycdfpp.load(pycdfpp.save(cdf))
        self.assertEqual(saved["x"].values[1], 3)

    def test_borrowed_and_copied_variables_compare_equal(self):
        values = np.arange(12, dtype=np.int64).reshape(4, 3)
        copied = pycdfpp.CDF()
        copied.add_variable("x", values)
        self.assertEqual(self.borrowing_cdf(values)["x"], copied["x"])

    def test_arrays_that_need_a_copy_are_rejected(self):
        base = np.arange(40, dtype=np.float64).reshape(20, 2)
        for label, values in (("strided view", base[:, 0]),
                              ("byte-swapped", base.astype(">f8")),
                              ("datetime64", np.arange(3).astype("datetime64[ns]")),
                              ("strings", np.array(["a", "b"]))):
            with self.subTest(values=label):
                with self.assertRaises(ValueError):
                    self.borrowing_cdf(values)


class PycdfCompressedBlocksTest(unittest.TestCase):
    """Big compressed variables are written as many blocks, so readers can decompress them on
    several threads, and read part of a variable without decompressing all of it."""

    def test_big_compressed_variables_are_split_into_blocks(self):
        import pycdfpp.debug
        values = np.arange(1_000_000, dtype=np.float64)
        for codec in [pycdfpp.CompressionType.gzip_compression,
                      pycdfpp.CompressionType.rle_compression] + EXPERIMENTAL_CODECS:
            with self.subTest(codec=codec), temporary_file(suffix=".cdf") as f, \
                    warnings.catch_warnings():
                warnings.simplefilter("ignore", pycdfpp.ExperimentalCompressionWarning)
                cdf = pycdfpp.CDF()
                cdf.add_variable("x", values, compression=codec)
                self.assertTrue(pycdfpp.save(cdf, f.name))

                records = pycdfpp.debug.for_each_record(f.name)
                blocking_factor = next(fields["BlockingFactor"] for _, kind, fields in records
                                       if kind == "zVDR")
                blocks = sum(kind == "CVVR" for _, kind, _ in records)
                self.assertLessEqual(blocking_factor * values.itemsize, 256 * 1024)
                self.assertEqual(blocks, -(-len(values) // blocking_factor))
                # NASA's library reports files with more than 10 entries in a VXR as corrupted.
                for _, kind, fields in records:
                    if kind == "VXR":
                        self.assertLessEqual(fields["Nentries"], 10)
                self.assertTrue(np.array_equal(pycdfpp.load(f.name)["x"].values, values))


class PycdfStandardCodecsTest(unittest.TestCase):
    def test_saving_with_standard_codecs_does_not_warn(self):
        for codec in (pycdfpp.CompressionType.no_compression, pycdfpp.CompressionType.gzip_compression,
                      pycdfpp.CompressionType.rle_compression):
            with self.subTest(codec=codec), warnings.catch_warnings():
                warnings.simplefilter("error")
                pycdfpp.save(cdf_with_one_variable(codec))


class PycdfSaveOverSourceTest(unittest.TestCase):
    RESOURCES = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "resources")

    def test_saving_a_lazily_loaded_cdf_over_its_file_keeps_the_data(self):
        for fixture in ("a_cdf.cdf", "a_cdf_with_compressed_vars.cdf"):
            with self.subTest(fixture=fixture), tempfile.TemporaryDirectory() as tmp:
                path = os.path.join(tmp, fixture)
                shutil.copyfile(os.path.join(self.RESOURCES, fixture), path)
                reference = pycdfpp.load(path, lazy_load=False)
                cdf = pycdfpp.load(path)
                cdf.add_attribute("edited", ["yes"])
                self.assertTrue(pycdfpp.save(cdf, path))
                reloaded = pycdfpp.load(path, lazy_load=False)
                self.assertEqual(reloaded.attributes["edited"][0], "yes")
                for name in reference:
                    np.testing.assert_array_equal(reloaded[name].values, reference[name].values)

    def test_saving_to_an_unwritable_path_raises(self):
        with tempfile.TemporaryDirectory() as tmp:
            with self.assertRaises(OSError):
                pycdfpp.save(pycdfpp.CDF(), os.path.join(tmp, "missing_dir", "out.cdf"))


if __name__ == '__main__':
    unittest.main()
