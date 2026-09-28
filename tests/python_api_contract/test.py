#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""Behaviours of the Python API that mutation testing found unpinned: changing any of them
passed every other test. Each test here fails if the behaviour changes."""
import json
import os
import re
import unittest
import warnings

import numpy as np
import pycdfpp

os.environ['TZ'] = 'UTC'
D = pycdfpp.DataType


def reloaded(cdf):
    return pycdfpp.load(pycdfpp.save(cdf))


def entry_type(values, data_type=None):
    cdf = pycdfpp.CDF()
    cdf.add_attribute("a", [values], None if data_type is None else [data_type])
    return reloaded(cdf).attributes["a"].type(0)


class FillAndPadValues(unittest.TestCase):
    """ISTP fill and pad values, per CDF type: files written with other values mislead readers."""

    FILL = {D.CDF_INT1: np.int8(-128), D.CDF_BYTE: np.int8(-128), D.CDF_UINT1: np.uint8(255),
            D.CDF_INT2: np.int16(-32768), D.CDF_UINT2: np.uint16(65535),
            D.CDF_INT4: np.int32(-2147483648), D.CDF_UINT4: np.uint32(4294967295),
            D.CDF_INT8: np.int64(-9223372036854775808), D.CDF_FLOAT: np.float32(-1e31),
            D.CDF_REAL4: np.float32(-1e31), D.CDF_DOUBLE: np.float64(-1e31), D.CDF_REAL8: np.float64(-1e31)}
    PAD = {D.CDF_INT1: np.int8(-127), D.CDF_BYTE: np.int8(-127), D.CDF_UINT1: np.uint8(254),
           D.CDF_INT2: np.int16(-32767), D.CDF_UINT2: np.uint16(65534),
           D.CDF_INT4: np.int32(-2147483647), D.CDF_UINT4: np.uint32(4294967294),
           D.CDF_INT8: np.int64(-9223372036854775807), D.CDF_FLOAT: np.float32(-1e30),
           D.CDF_REAL4: np.float32(-1e30), D.CDF_DOUBLE: np.float64(-1e30), D.CDF_REAL8: np.float64(-1e30)}

    def check(self, function, table):
        for data_type, expected in table.items():
            with self.subTest(data_type=data_type):
                value = function(data_type)
                self.assertEqual(type(value), type(expected))
                self.assertEqual(value, expected)

    def test_numeric_fill_values(self):
        self.check(pycdfpp.default_fill_value, self.FILL)

    def test_numeric_pad_values(self):
        self.check(pycdfpp.default_pad_value, self.PAD)

    def test_time_values(self):
        self.assertEqual(pycdfpp.default_pad_value(D.CDF_TIME_TT2000).nseconds, -9223372036854775807)
        self.assertEqual(pycdfpp.default_fill_value(D.CDF_TIME_TT2000).nseconds, -9223372036854775808)
        self.assertEqual(pycdfpp.default_pad_value(D.CDF_EPOCH).mseconds, 0.0)
        self.assertEqual(pycdfpp.default_fill_value(D.CDF_EPOCH).mseconds, -1e31)
        pad16 = pycdfpp.default_pad_value(D.CDF_EPOCH16)
        self.assertEqual((pad16.seconds, pad16.picoseconds), (0.0, 0.0))
        fill16 = pycdfpp.default_fill_value(D.CDF_EPOCH16)
        self.assertEqual((fill16.seconds, fill16.picoseconds), (-1e31, -1e31))

    def test_strings_are_padded_with_spaces(self):
        for data_type in (D.CDF_CHAR, D.CDF_UCHAR):
            with self.subTest(data_type=data_type):
                self.assertIsNone(pycdfpp.default_fill_value(data_type))
                self.assertEqual(pycdfpp.default_pad_value(data_type), b" ")
        self.assertIsNone(pycdfpp.default_fill_value(D.CDF_NONE))
        self.assertIsNone(pycdfpp.default_pad_value(D.CDF_NONE))


class ListsOfIntegers(unittest.TestCase):
    """A list of Python integers gets the smallest CDF type holding every value."""

    def test_smallest_type_at_every_boundary(self):
        for values, data_type in (([0, 255], D.CDF_UINT1), ([256], D.CDF_UINT2), ([0, 65535], D.CDF_UINT2),
                                  ([65536], D.CDF_UINT4), ([2**32 - 1], D.CDF_UINT4), ([2**32], D.CDF_INT8),
                                  ([-128, 127], D.CDF_INT1), ([-129], D.CDF_INT2), ([-1, 128], D.CDF_INT2),
                                  ([-32768, 32767], D.CDF_INT2), ([-32769], D.CDF_INT4), ([-1, 32768], D.CDF_INT4),
                                  ([-2**31, 2**31 - 1], D.CDF_INT4), ([-2**31 - 1], D.CDF_INT8),
                                  ([-1, 2**31], D.CDF_INT8)):
            with self.subTest(values=values):
                self.assertEqual(entry_type(values), data_type)

    def test_numpy_integers_keep_their_type(self):
        for values, data_type in (([np.int16(1), np.int16(2)], D.CDF_INT2), ([np.uint32(1)], D.CDF_UINT4),
                                  ([np.int64(1)], D.CDF_INT8), ([np.uint64(1)], D.CDF_INT8)):
            with self.subTest(values=values):
                self.assertEqual(entry_type(values), data_type)

    def test_values_at_the_limits_of_a_given_type_fit(self):
        for values, data_type in (([-128, 127], D.CDF_INT1), ([0, 255], D.CDF_UINT1),
                                  ([-32768, 32767], D.CDF_INT2), ([0, 4294967295], D.CDF_UINT4)):
            with self.subTest(values=values, data_type=data_type):
                self.assertEqual(entry_type(np.array(values), data_type), data_type)
                cdf = pycdfpp.CDF()
                cdf.add_variable("v", np.array(values), data_type=data_type)
                self.assertEqual(reloaded(cdf)["v"].values.tolist(), values)
        self.assertEqual(entry_type(np.array([2**63 - 1], dtype=np.uint64)), D.CDF_INT8)

    def test_empty_arrays_take_the_given_type(self):
        cdf = pycdfpp.CDF()
        cdf.add_variable("v", np.array([], dtype=np.int32), data_type=D.CDF_FLOAT)
        self.assertEqual(reloaded(cdf)["v"].type, D.CDF_FLOAT)


class StringEntries(unittest.TestCase):

    def test_one_string_in_a_list_is_that_string(self):
        cdf = pycdfpp.CDF()
        cdf.add_variable("v", np.zeros(2)).add_attribute("x", ["abc"])
        self.assertEqual(reloaded(cdf)["v"].attributes["x"].value, "abc")

    def test_several_strings_in_one_entry_are_refused(self):
        with self.assertRaisesRegex(ValueError, "one string"):
            pycdfpp.CDF().add_variable("v", np.zeros(2)).add_attribute("x", ["a", "b"])

    def test_string_types(self):
        self.assertEqual(entry_type("text"), D.CDF_CHAR)
        self.assertEqual(entry_type("text", D.CDF_UCHAR), D.CDF_UCHAR)
        self.assertEqual(entry_type("text", D.CDF_CHAR), D.CDF_CHAR)
        with self.assertRaisesRegex(ValueError, "str"):
            entry_type("text", D.CDF_INT4)

    def test_latin1_is_read_as_utf8_by_default(self):
        cdf = pycdfpp.CDF()
        cdf.add_attribute("a", [np.bytes_("été".encode("latin-1"))])
        data = pycdfpp.save(cdf)
        self.assertEqual(pycdfpp.load(data).attributes["a"][0], "été")
        self.assertEqual(pycdfpp.load(data, iso_8859_1_to_utf8=True).attributes["a"][0], "été")


class Variables(unittest.TestCase):

    def test_typed_variable_without_values(self):
        """It has no record: its shape was (), a 0-d array read past its empty buffer."""
        var = pycdfpp.CDF().add_variable("v", data_type=D.CDF_FLOAT)
        self.assertEqual(var.shape, (0,))
        self.assertEqual(var.type, D.CDF_FLOAT)
        self.assertEqual(var.values.dtype, np.float32)
        self.assertEqual(var.values.size, 0)
        self.assertFalse(var.is_nrv)

    def test_none_data_type_means_inferred(self):
        cdf = pycdfpp.CDF()
        cdf.add_variable("v").set_values(np.arange(3, dtype=np.int16), data_type=D.CDF_NONE)
        self.assertEqual(cdf["v"].type, D.CDF_INT2)

    def test_borrowing_needs_an_array(self):
        with self.assertRaisesRegex(ValueError, "numpy array"):
            pycdfpp.CDF().add_variable("v", [1.5, 2.5], copy=False)

    def test_non_record_varying_values_are_one_record(self):
        for values, shape in ((np.array([5.0]), (1,)), (np.array([5.0, 6.0]), (1, 2)), (np.array(5.0), (1,)),
                              ([7], (1,)), ([7, 8], (1, 2))):
            with self.subTest(values=values):
                cdf = pycdfpp.CDF()
                cdf.add_variable("c", values, is_nrv=True)
                self.assertEqual(reloaded(cdf)["c"].shape, shape)

    def test_trailing_unit_dimensions_may_be_left_out(self):
        for record_shape, values in (((1,), np.arange(4.0)), ((2, 1), np.arange(8.0).reshape(4, 2)),
                                     ((1, 1), np.arange(4.0))):
            with self.subTest(record_shape=record_shape):
                cdf = pycdfpp.CDF()
                var = cdf.add_variable("v", np.zeros((0, *record_shape)))
                var.set_values(values)
                self.assertEqual(var.shape, (len(values), *record_shape))
                self.assertEqual(var.values.ravel().tolist(), values.ravel().tolist())
        cdf = pycdfpp.CDF()
        var = cdf.add_variable("v", np.zeros((0, 2)))
        with self.assertRaises(ValueError):
            var.set_values(np.arange(4.0))


class Attributes(unittest.TestCase):

    def test_values_are_copied_from_views(self):
        base = np.arange(6.0)
        cdf = pycdfpp.CDF()
        cdf.add_attribute("a", [base[1:4]])
        base[...] = -1
        self.assertEqual(list(cdf.attributes["a"][0]), [1.0, 2.0, 3.0])

    def test_copying_attributes(self):
        source = pycdfpp.CDF()
        source.add_attribute("g", [[1, 2], "txt"])
        var = source.add_variable("v", np.zeros(2))
        var.add_attribute("x", [1.5])
        target = pycdfpp.CDF()
        target.add_attribute(source.attributes["g"])
        target.add_variable("w", np.zeros(2)).add_attribute(source["v"].attributes["x"])
        self.assertEqual(target.attributes["g"], source.attributes["g"])
        self.assertEqual(target["w"].attributes["x"], source["v"].attributes["x"])
        target.add_attribute("h", [[0]])
        target.attributes["h"].set_values(source.attributes["g"])
        self.assertEqual(list(target.attributes["h"]), list(source.attributes["g"]))

    def test_keyword_arguments(self):
        cdf = pycdfpp.CDF()
        cdf.add_attribute(name="g", entries_values=[[1, 2]], entries_types=[D.CDF_INT4])
        self.assertEqual(cdf.attributes["g"].type(0), D.CDF_INT4)
        cdf.attributes["g"].set_values(entries_values=[[3]], entries_types=[D.CDF_INT2])
        self.assertEqual(cdf.attributes["g"].type(0), D.CDF_INT2)
        var = cdf.add_variable(name="v", values=np.zeros(2), data_type=D.CDF_DOUBLE, is_nrv=False,
                               compression=pycdfpp.CompressionType.no_compression, attributes={"a": [1]},
                               copy=True)
        var.add_attribute(name="x", values=[1], data_type=D.CDF_INT8)
        self.assertEqual(var.attributes["x"].type(), D.CDF_INT8)
        var.attributes["x"].set_value(value=[2], data_type=D.CDF_INT2)
        self.assertEqual(var.attributes["x"].type(), D.CDF_INT2)

    def test_too_many_positional_arguments(self):
        cdf = pycdfpp.CDF()
        with self.assertRaisesRegex(TypeError, "at most 7 positional arguments"):
            cdf.add_variable("v", None, None, False, pycdfpp.CompressionType.no_compression, None, True, 1)
        with self.assertRaisesRegex(TypeError, "unexpected keyword"):
            cdf.add_variable("v", colour="blue")


class Filter(unittest.TestCase):

    @staticmethod
    def cdf():
        cdf = pycdfpp.CDF()
        for name in ("B_GSE", "B_GSM", "Epoch"):
            cdf.add_variable(name, np.zeros(2))
        for name in ("Project", "Mission"):
            cdf.add_attribute(name, ["x"])
        return cdf

    def test_every_kind_of_criterion(self):
        for criterion in (["B_GSE", "B_GSM"], ("B_GSE", "B_GSM"), "B_", re.compile("B_"),
                          lambda var: var.name.startswith("B_")):
            with self.subTest(criterion=criterion):
                result = pycdfpp.filter_cdf(self.cdf(), variables=criterion, attributes=["Mission"])
                self.assertEqual(sorted(result.keys()), ["B_GSE", "B_GSM"])
                self.assertEqual(list(result.attributes.keys()), ["Mission"])
        self.assertEqual(sorted(pycdfpp.filter_cdf(self.cdf()).keys()), ["B_GSE", "B_GSM", "Epoch"])

    def test_unsupported_criterion(self):
        with self.assertRaisesRegex(TypeError, "criterion"):
            pycdfpp.filter_cdf(self.cdf(), variables=42)


class Saving(unittest.TestCase):

    def test_experimental_codecs_warn_the_caller(self):
        codec = pycdfpp.CompressionType.zstd_compression
        cdf = pycdfpp.CDF()
        try:
            cdf.add_variable("v", np.zeros(4), compression=codec)
        except Exception:
            self.skipTest("zstd not compiled in")
        with warnings.catch_warnings(record=True) as caught:
            warnings.simplefilter("always")
            pycdfpp.save(cdf)
        experimental = [w for w in caught if issubclass(w.category, pycdfpp.ExperimentalCompressionWarning)]
        self.assertEqual(len(experimental), 1)
        self.assertIn("zstd_compression", str(experimental[0].message))
        self.assertEqual(experimental[0].filename, __file__)

    def test_standard_codecs_dont_warn(self):
        cdf = pycdfpp.CDF()
        cdf.add_variable("v", np.zeros(4), compression=pycdfpp.CompressionType.gzip_compression)
        with warnings.catch_warnings():
            warnings.simplefilter("error")
            pycdfpp.save(cdf)


class Skeletons(unittest.TestCase):

    def test_structure(self):
        cdf = pycdfpp.CDF()
        cdf.add_attribute("g", [[1, 2], "txt"])
        cdf.add_variable("v", data_type=D.CDF_FLOAT).add_attribute("x", [1.5])
        self.assertEqual(json.loads(json.dumps(pycdfpp.to_dict_skeleton(cdf))), {
            "compression": "CompressionType.no_compression",
            "attributes": {"g": {"values": [[1, 2], "txt"], "types": ["DataType.CDF_UINT1", "DataType.CDF_CHAR"]}},
            "variables": {"v": {"attributes": {"x": {"values": [[1.5]], "types": ["DataType.CDF_DOUBLE"]}},
                                "type": "DataType.CDF_FLOAT", "shape": [0], "compression":
                                    "CompressionType.no_compression", "is_nrv": False}}})


if __name__ == '__main__':
    unittest.main()
