#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""Every way of giving values to pycdfpp stores them exactly, or raises: never different values.

Values go through a save/load round trip, so what is checked is what a file holds."""
import gc
import os
import unittest
import warnings

import numpy as np
import pycdfpp

os.environ['TZ'] = 'UTC'
D = pycdfpp.DataType

NUMERIC_DTYPES = ("i1", "i2", "i4", "i8", "u1", "u2", "u4", "f4", "f8")
NUMERIC_TYPES = (D.CDF_INT1, D.CDF_INT2, D.CDF_INT4, D.CDF_INT8, D.CDF_UINT1, D.CDF_UINT2, D.CDF_UINT4,
                 D.CDF_BYTE, D.CDF_FLOAT, D.CDF_REAL4, D.CDF_DOUBLE, D.CDF_REAL8)
T0 = np.datetime64("2024-01-01T00:00:00", "ns")


def reloaded(cdf):
    return pycdfpp.load(pycdfpp.save(cdf))


def variable_values(values, **kwargs):
    cdf = pycdfpp.CDF()
    cdf.add_variable("v", values, **kwargs)
    cdf = reloaded(cdf)
    return cdf["v"].type, cdf["v"].values


def attribute_entry(values, data_type=None):
    cdf = pycdfpp.CDF()
    cdf.add_attribute("a", [values], None if data_type is None else [data_type])
    cdf = reloaded(cdf)
    return cdf.attributes["a"].type(0), np.asarray(cdf.attributes["a"][0])


def variable_attribute(values):
    cdf = pycdfpp.CDF()
    cdf.add_variable("v", np.zeros(2)).add_attribute("a", values)
    cdf = reloaded(cdf)
    return cdf["v"].attributes["a"].type(), np.asarray(cdf["v"].attributes["a"].value)


def layouts(values: np.ndarray):
    """The same values laid out in memory in every way numpy allows."""
    swapped = values.dtype.newbyteorder(">" if values.dtype.byteorder != ">" else "<")
    doubled = np.repeat(values, 2, axis=0)
    yield "strided", doubled[::2]
    yield "reversed", values[::-1].copy()[::-1]
    yield "byte-swapped", values.astype(swapped)
    yield "byte-swapped strided", doubled.astype(swapped)[::2]
    if values.ndim == 2:
        yield "Fortran order", np.asfortranarray(values)
        yield "transposed", np.ascontiguousarray(values.T).T


def same_values(got, expected):
    got, expected = np.asarray(got), np.asarray(expected)
    if got.shape != expected.shape:
        return False
    if expected.dtype.kind == "f":
        return np.array_equal(got.astype(np.float64), expected.astype(np.float64), equal_nan=True)
    return got.tolist() == expected.tolist()


class ArrayLayouts(unittest.TestCase):
    """Strided, Fortran-ordered and byte-swapped arrays hold the same values as C-ordered ones."""

    def test_numeric_variables(self):
        for dtype in NUMERIC_DTYPES:
            expected = np.arange(12, dtype=dtype).reshape(6, 2)
            for label, values in layouts(expected):
                for entry in ("add_variable", "set_values"):
                    with self.subTest(dtype=dtype, layout=label, entry=entry):
                        if entry == "add_variable":
                            _, got = variable_values(values)
                        else:
                            cdf = pycdfpp.CDF()
                            cdf.add_variable("v").set_values(values)
                            got = reloaded(cdf)["v"].values
                        self.assertTrue(same_values(got, expected), f"{got.tolist()}")

    def test_numeric_attributes(self):
        for dtype in NUMERIC_DTYPES:
            expected = np.arange(6, dtype=dtype)
            for label, values in layouts(expected):
                for entry, store in (("CDF", attribute_entry), ("variable", variable_attribute)):
                    with self.subTest(dtype=dtype, layout=label, entry=entry):
                        _, got = store(values)
                        self.assertTrue(same_values(got, expected), f"{got.tolist()}")

    def test_time_variables_and_attributes(self):
        expected = T0 + np.arange(6).astype("timedelta64[s]")
        for label, values in layouts(expected):
            with self.subTest(layout=label, entry="variable"):
                cdf = pycdfpp.CDF()
                cdf.add_variable("t", values)
                self.assertTrue(same_values(pycdfpp.to_datetime64(reloaded(cdf)["t"]), expected))
            with self.subTest(layout=label, entry="attribute"):
                cdf = pycdfpp.CDF()
                cdf.add_attribute("t", [values])
                got = pycdfpp.to_datetime64(reloaded(cdf).attributes["t"][0])
                self.assertTrue(same_values(got, expected))

    def test_string_variables(self):
        for kind in ("U", "S"):
            expected = np.array([["ab", "cd"], ["ef", "gh"], ["ij", "kl"]]).astype(kind)
            for label, values in layouts(expected):
                if "byte-swapped" in label and kind == "S":
                    continue
                with self.subTest(kind=kind, layout=label):
                    _, got = variable_values(values)
                    self.assertTrue(same_values(got, expected.astype("S")), f"{got.tolist()}")

    def test_time_conversions(self):
        dt64 = T0 + np.arange(6).astype("timedelta64[ms]")
        for label, values in layouts(dt64):
            for convert in (pycdfpp.to_tt2000, pycdfpp.to_epoch, pycdfpp.to_epoch16):
                with self.subTest(layout=label, conversion=convert.__name__):
                    self.assertTrue(same_values(pycdfpp.to_datetime64(convert(values)), dt64))
        tt2000 = pycdfpp.to_tt2000(dt64)
        for label, values in (("strided", np.repeat(tt2000, 2)[::2]), ("reversed", tt2000[::-1].copy()[::-1])):
            with self.subTest(layout=label, conversion="to_datetime64"):
                self.assertTrue(same_values(pycdfpp.to_datetime64(values), dt64))

    def test_borrowing_needs_a_plain_layout(self):
        for label, values in layouts(np.arange(12, dtype=np.float64).reshape(6, 2)):
            with self.subTest(layout=label):
                with self.assertRaises(ValueError):
                    pycdfpp.CDF().add_variable("v", values, copy=False)


class ExplicitTypes(unittest.TestCase):
    """Given a CDF type, values are converted to it only when none of them changes."""

    SOURCES = {
        "i1": [-128, -1, 0, 1, 127], "i2": [-1, 0, 300], "i4": [-1, 0, 70000], "i8": [-1, 0, 2**53 + 1],
        "u1": [0, 1, 255], "u2": [0, 1, 65535], "u4": [0, 1, 2**32 - 1], "u8": [0, 1, 2**63 + 1],
        "f4": [0.5, -1.5, 2.0], "f8": [0.1, -1.5, 1e300, float("nan")],
    }

    def check(self, store, dtype, values, data_type):
        source = np.array(values, dtype=dtype)
        with warnings.catch_warnings():
            warnings.simplefilter("ignore")
            try:
                stored_type, got = store(source, data_type)
            except ValueError:
                return "refused"
        self.assertEqual(stored_type, data_type)
        self.assertTrue(same_values(got, source), f"{dtype} {values} as {data_type} gave {got.tolist()}")
        return "exact"

    def test_variables_and_attributes(self):
        stores = {"variable": lambda values, data_type: variable_values(values, data_type=data_type),
                  "attribute": attribute_entry}
        for where, store in stores.items():
            for dtype, values in self.SOURCES.items():
                for data_type in NUMERIC_TYPES:
                    with self.subTest(where=where, dtype=dtype, data_type=data_type):
                        self.check(store, dtype, values, data_type)

    def test_conversions_that_keep_every_value_are_done(self):
        for where, store in (("variable", lambda v, t: variable_values(v, data_type=t)), ("attribute", attribute_entry)):
            for dtype, values, data_type in (("i2", [-1, 300], D.CDF_INT4), ("u1", [0, 255], D.CDF_INT2),
                                             ("i4", [-1, 100], D.CDF_INT1), ("f4", [0.5, -1.5], D.CDF_DOUBLE),
                                             ("f8", [1.0, -2.0], D.CDF_INT4), ("i4", [1, 2], D.CDF_FLOAT),
                                             ("u8", [0, 2**62], D.CDF_INT8), ("f8", [0.5, float("nan")], D.CDF_FLOAT)):
                with self.subTest(where=where, dtype=dtype, data_type=data_type):
                    self.assertEqual(self.check(store, dtype, values, data_type), "exact")

    def test_existing_variable_type(self):
        """New values keep an existing variable's type: converting them is asked with data_type.
        An int32 array of the same size used to be stored as its bits in a CDF_FLOAT variable."""
        cdf = pycdfpp.CDF()
        var = cdf.add_variable("v", np.array([1.5, 2.5], dtype=np.float32))
        with warnings.catch_warnings():
            warnings.simplefilter("ignore")
            for values in (np.array([3, 4], dtype=np.int32), np.array([3.0, 4.0])):
                with self.subTest(dtype=values.dtype):
                    with self.assertRaises(ValueError):
                        var.set_values(values)
            self.assertEqual(var.values.tolist(), [1.5, 2.5])
            var.set_values(np.array([3, 4], dtype=np.int32), data_type=D.CDF_FLOAT)
            self.assertEqual(var.values.tolist(), [3.0, 4.0])
            with self.assertRaises(ValueError):
                var.set_values(np.array([2**30 + 1], dtype=np.int32), data_type=D.CDF_FLOAT)
        self.assertEqual(var.values.tolist(), [3.0, 4.0])

    def test_lists_for_attributes(self):
        for values, data_type in (([1.5, 2.5], D.CDF_INT4), ([300], D.CDF_INT1), ([-1], D.CDF_UINT1)):
            with self.subTest(values=values, data_type=data_type):
                with self.assertRaises((ValueError, OverflowError)):
                    attribute_entry(values, data_type)
        self.assertEqual(attribute_entry([1.0, 2.0], D.CDF_INT4)[1].tolist(), [1, 2])

    def test_borrowing_refuses_other_types(self):
        for dtype, data_type in (("i4", D.CDF_FLOAT), ("f4", D.CDF_INT4), ("i1", D.CDF_UINT1),
                                 ("u8", D.CDF_INT8), ("f8", D.CDF_INT8)):
            with self.subTest(dtype=dtype, data_type=data_type):
                with self.assertRaises(ValueError):
                    pycdfpp.CDF().add_variable("v", np.ones(3, dtype=dtype), data_type=data_type, copy=False)


class InferredTypes(unittest.TestCase):
    """Without a CDF type, pycdfpp picks one that holds every value, or raises."""

    def test_unsigned_64_bits(self):
        self.assertEqual(variable_values(np.array([0, 2**62], dtype=np.uint64))[0], D.CDF_INT8)
        self.assertEqual(attribute_entry(np.array([0, 2**62], dtype=np.uint64))[0], D.CDF_INT8)
        for store in (variable_values, attribute_entry):
            with self.subTest(store=store.__name__):
                with self.assertRaises(ValueError):
                    store(np.array([2**63], dtype=np.uint64))

    def test_booleans_are_uint1(self):
        for label, store, values in (("bool array variable", variable_values, np.array([True, False])),
                                     ("bool list variable", variable_values, [True, False]),
                                     ("bool attribute", variable_attribute, True),
                                     ("np.bool_ attribute", variable_attribute, np.bool_(True)),
                                     ("bool list attribute", attribute_entry, [True, False]),
                                     ("bool array attribute", attribute_entry, np.array([True, False]))):
            with self.subTest(values=label):
                data_type, got = store(values)
                self.assertEqual(data_type, D.CDF_UINT1)
                self.assertTrue(same_values(got, np.asarray(values, dtype=np.uint8).reshape(got.shape)))

    def test_lists_of_numpy_scalars_keep_their_type(self):
        for values, data_type in (([np.float32(1.5), np.float32(2.5)], D.CDF_FLOAT),
                                  ([np.int16(-1), np.int16(2)], D.CDF_INT2),
                                  ([np.uint32(1), np.uint32(2)], D.CDF_UINT4),
                                  ([[np.float64(1.5)], [np.float64(2.5)]], D.CDF_DOUBLE)):
            for entry in ("add_variable", "set_values"):
                with self.subTest(values=values, entry=entry):
                    if entry == "add_variable":
                        got_type, got = variable_values(values)
                    else:
                        cdf = pycdfpp.CDF()
                        cdf.add_variable("v").set_values(values)
                        cdf = reloaded(cdf)
                        got_type, got = cdf["v"].type, cdf["v"].values
                    self.assertEqual(got_type, data_type)
                    self.assertTrue(same_values(got, np.array(values)))

    def test_lists_of_numpy_times_and_bytes(self):
        data_type, _ = variable_values([T0, T0 + np.timedelta64(1, "s")])
        self.assertEqual(data_type, D.CDF_TIME_TT2000)
        for values in ([b"ab", b"cd"], [np.bytes_(b"ab"), np.bytes_(b"cd")]):
            with self.subTest(values=values):
                _, got = variable_values(values)
                self.assertEqual(got.tolist(), [b"ab", b"cd"])

    def test_tuples_are_lists(self):
        self.assertEqual(attribute_entry((1, 2))[1].tolist(), attribute_entry([1, 2])[1].tolist())
        self.assertEqual(variable_attribute((1.5, 2.5))[1].tolist(), [1.5, 2.5])

    def test_zero_dimension_array(self):
        with self.assertRaisesRegex(ValueError, "dimension"):
            variable_values(np.array(1.5))
        cdf = pycdfpp.CDF()
        cdf.add_variable("c", np.array(1.5), is_nrv=True)
        self.assertEqual(reloaded(cdf)["c"].values.tolist(), [1.5])

    def test_unsupported_dtypes_are_refused(self):
        for values in (np.array([1.5], dtype=np.float16), np.array([1 + 1j]), np.array([1.5, "x"], dtype=object)):
            with self.subTest(dtype=values.dtype):
                with self.assertRaises(ValueError):
                    variable_values(values)


class Lifetimes(unittest.TestCase):
    """Objects taken from a CDF keep it alive, even when the CDF itself was never named."""

    @classmethod
    def setUpClass(cls):
        cdf = pycdfpp.CDF()
        cdf.add_attribute("a", [[1, 2], "text"])
        cdf.add_variable("v", np.arange(4.0), attributes={"x": [1.5]})
        cls.file = bytes(pycdfpp.save(cdf))

    def test_chained_access_on_a_loaded_file(self):
        attributes = pycdfpp.load(self.file).attributes
        gc.collect()
        self.assertEqual(list(attributes.keys()), ["a"])
        self.assertIn("a", attributes)
        self.assertEqual(pycdfpp.load(self.file).attributes["a"][1], "text")
        self.assertEqual(list(pycdfpp.load(self.file).attributes.keys()), ["a"])
        variable = pycdfpp.load(self.file)["v"]
        gc.collect()
        self.assertEqual(variable.values.tolist(), [0.0, 1.0, 2.0, 3.0])
        self.assertEqual(pycdfpp.load(self.file)["v"].attributes["x"].value, [1.5])

    def test_loading_needs_contiguous_bytes(self):
        self.assertEqual(list(pycdfpp.load(memoryview(self.file)).attributes.keys()), ["a"])
        strided = np.repeat(np.frombuffer(self.file, dtype=np.uint8), 2)[::2]
        with self.assertRaises(ValueError):
            pycdfpp.load(memoryview(strided))


if __name__ == '__main__':
    unittest.main()
