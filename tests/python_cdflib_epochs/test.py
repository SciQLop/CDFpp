"""pycdfpp.cdflib.epochs.CDFepoch against the real cdflib.epochs.CDFepoch."""
import unittest

import numpy as np
from ddt import data, ddt, unpack

import cdflib
from pycdfpp.cdflib.epochs import CDFepoch

REF = cdflib.cdfepoch
RNG = np.random.default_rng(42)

LEAP = REF.compute_tt2000([2016, 12, 31, 23, 59, 60, 500, 0, 0])
TT2000_FILL = -9223372036854775808
TT2000_PAD = -9223372036854775807
SECOND = 1_000_000_000


def tt2000_samples():
    named = [REF.compute_tt2000(c) for c in (
        [2000, 1, 1, 12, 0, 0, 0, 0, 0], [2000, 1, 1, 11, 59, 59, 999, 999, 999],
        [1970, 1, 1, 0, 0, 0, 0, 0, 0], [1969, 12, 31, 23, 59, 59, 999, 999, 999],
        [1965, 3, 4, 5, 6, 7, 8, 9, 10], [1950, 3, 4, 5, 6, 7, 8, 9, 10],
        [1972, 1, 1, 0, 0, 0, 0, 0, 0], [1982, 6, 30, 23, 59, 60, 123, 456, 789],
        [2016, 12, 31, 23, 59, 59, 999, 999, 999], [2016, 12, 31, 23, 59, 60, 0, 0, 0],
        [2016, 12, 31, 23, 59, 60, 999, 999, 999], [2017, 1, 1, 0, 0, 0, 0, 0, 0],
        [2250, 6, 7, 8, 9, 10, 11, 12, 13])]
    around_leap = np.arange(LEAP - 3 * SECOND, LEAP + 3 * SECOND, SECOND // 4)
    spread = RNG.integers(-2_900_000_000_000_000_000, 2_900_000_000_000_000_000, 1000 - 24 - len(named) - 2)
    return np.concatenate([named, around_leap, spread, [TT2000_FILL, TT2000_PAD]]).astype(np.int64)


def epoch_samples():
    named = [REF.compute_epoch(c) for c in (
        [2020, 1, 1, 0, 0, 0, 0], [1970, 1, 1, 0, 0, 0, 0], [1969, 12, 31, 23, 59, 59, 999],
        [2000, 1, 1, 12, 0, 0, 0], [2016, 12, 31, 23, 59, 59, 999], [5, 3, 4, 5, 6, 7, 8],
        [1500, 3, 4, 5, 6, 7, 8], [3000, 3, 4, 5, 6, 7, 8])]
    fractional = [63745056000000.123, 63745056000999.5]
    spread = RNG.integers(53_000_000_000_000, 69_000_000_000_000, 1000 - len(named) - 2 - 2)
    return np.concatenate([named, fractional, spread, [0.0, -1e31]]).astype(np.float64)


def epoch16_samples():
    named = [REF.compute_epoch16(c) for c in (
        [2020, 1, 1, 0, 0, 0, 123, 456, 789, 999], [1970, 1, 1, 0, 0, 0, 0, 0, 0, 0],
        [1969, 12, 31, 23, 59, 59, 999, 999, 999, 999], [2000, 1, 1, 12, 0, 0, 0, 0, 0, 1],
        [5, 3, 4, 5, 6, 7, 8, 9, 10, 11], [3000, 3, 4, 5, 6, 7, 8, 9, 10, 11])]
    count = 1000 - len(named) - 2
    seconds = RNG.integers(53_000_000_000, 69_000_000_000, count).astype(np.float64)
    picoseconds = RNG.integers(0, 10**12, count).astype(np.float64)
    spread = seconds + 1j * picoseconds
    return np.concatenate([named, spread, [0j, complex(-1e31, -1e31)]]).astype(np.complex128)


SAMPLES = {"tt2000": tt2000_samples(), "epoch": epoch_samples(), "epoch16": epoch16_samples()}
PYTHON_SCALAR = {"tt2000": int, "epoch": float, "epoch16": complex}
EPOCH_1678, EPOCH_2262 = REF.compute_epoch([[1678, 1, 1, 0, 0, 0, 0], [2262, 1, 1, 0, 0, 0, 0]])
NOT_DATETIME64 = {"tt2000": lambda v: np.zeros(v.shape, dtype=bool),
                  "epoch": lambda v: (v == -1e31) | ((v != 0) & (v < EPOCH_1678)) | (v > EPOCH_2262) | (v % 1 != 0),
                  "epoch16": lambda v: ((v.real > 0) & (v.real < EPOCH_1678 / 1000)) | (v.real > EPOCH_2262 / 1000)}
YEAR_0 = {"tt2000": lambda v: v == TT2000_PAD, "epoch": lambda v: v == 0.0, "epoch16": lambda v: v == 0j}


def forms(kind, keep=None):
    """The same values as a single Python scalar, a short list and a 1000 element array."""
    values = SAMPLES[kind] if keep is None else SAMPLES[kind][~keep(SAMPLES[kind])]
    return [("scalar", PYTHON_SCALAR[kind](values[0])), ("list", [PYTHON_SCALAR[kind](v) for v in values[:5]]),
            ("array", values)]


KINDS = ("tt2000", "epoch", "epoch16")
ALL_FORMS = [(kind, label, values) for kind in KINDS for label, values in forms(kind)]
DATETIME64_FORMS = [(kind, label, values) for kind in KINDS for label, values in forms(kind, NOT_DATETIME64[kind])]
UNIXTIME_FORMS = [(kind, label, values) for kind in KINDS for label, values in forms(kind, YEAR_0[kind])]


class SameAs(unittest.TestCase):
    def assertSame(self, ours, theirs):
        self.assertIs(type(ours), type(theirs))
        if isinstance(theirs, list):
            self.assertEqual(len(ours), len(theirs))
            for o, t in zip(ours, theirs):
                self.assertSame(o, t)
        elif isinstance(theirs, (np.ndarray, np.generic)):
            self.assertEqual(ours.shape, theirs.shape)
            self.assertEqual(ours.dtype, theirs.dtype)
            np.testing.assert_array_equal(ours, theirs)
        else:
            self.assertEqual(ours, theirs)

    def assertSameCall(self, method, *args):
        self.assertSame(getattr(CDFepoch, method)(*args), getattr(REF, method)(*args))


@ddt
class Breakdown(SameAs):
    @data(*ALL_FORMS)
    @unpack
    def test_breakdown(self, kind, label, values):
        self.assertSameCall("breakdown", values)
        self.assertSameCall("breakdown_" + kind, values)


@ddt
class Encode(SameAs):
    @data(*[(kind, label, values, iso) for kind, label, values in ALL_FORMS for iso in (True, False)
            if not (kind == "tt2000" and label == "array")])
    @unpack
    def test_encode(self, kind, label, values, iso):
        self.assertSameCall("encode", values, iso)
        self.assertSameCall("encode_" + kind, values, iso)

    @data(True, False)
    def test_tt2000_array_without_fill(self, iso):
        values = SAMPLES["tt2000"][:-2]
        self.assertSameCall("encode", values, iso)
        self.assertSameCall("parse", REF.encode(values, iso))

    def test_default_is_iso(self):
        self.assertEqual(CDFepoch.encode(SAMPLES["epoch"][0]), REF.encode(SAMPLES["epoch"][0]))

    def test_unknown_type(self):
        with self.assertRaises(TypeError):
            CDFepoch.encode(np.array([1], dtype=np.int32))


@ddt
class Parse(SameAs):
    @data(*[(kind, label, values, iso) for kind, label, values in ALL_FORMS for iso in (True, False)
            if not (kind == "tt2000" and label == "array")])
    @unpack
    def test_parse_what_encode_emits(self, kind, label, values, iso):
        text = REF.encode(values, iso)
        self.assertSameCall("parse", text)

    def test_parse_tuple(self):
        text = tuple(REF.encode(SAMPLES["epoch"][:3]))
        self.assertSameCall("parse", text)

    def test_bad_input(self):
        for value, error in ((12, TypeError), ([1, 2], TypeError), ("nonsense", ValueError)):
            with self.subTest(value=value):
                with self.assertRaises(error):
                    CDFepoch.parse(value)


@ddt
class ToDatetime(SameAs):
    @data(*DATETIME64_FORMS)
    @unpack
    def test_to_datetime(self, kind, label, values):
        self.assertSameCall("to_datetime", values)

    def test_epoch_keeps_sub_millisecond(self):
        result = CDFepoch.to_datetime(63745056000000.123)
        exact_double = np.timedelta64(round((63745056000000.123 - 63745056000000.0) * 1e6), "ns")
        np.testing.assert_array_equal(result, np.array([np.datetime64("2020-01-01", "ns") + exact_double]))

    def test_epoch_fill_is_nat(self):
        result = CDFepoch.to_datetime(np.array([63745056000000.0, -1e31]))
        np.testing.assert_array_equal(result, np.array(["2020-01-01", "NaT"], dtype="datetime64[ns]"))

    def test_dates_outside_datetime64_raise_like_cdflib(self):
        cases = {"tt2000 after 2262": REF.compute_tt2000([2280, 1, 1, 0, 0, 0, 0, 0, 0]),
                 "epoch 1500 and 3000": REF.compute_epoch([[1500, 3, 4, 5, 6, 7, 8], [3000, 1, 1, 0, 0, 0, 0]]),
                 "epoch 3000 next to fill": np.array([REF.compute_epoch([3000, 1, 1, 0, 0, 0, 0]), -1e31]),
                 "epoch16 year 5": REF.compute_epoch16([5, 3, 4, 5, 6, 7, 8, 9, 10, 11])}
        for label, values in cases.items():
            with self.subTest(label):
                with self.assertRaises(OverflowError):
                    REF.to_datetime(values)
                with self.assertRaises(OverflowError):
                    CDFepoch.to_datetime(values)

    def test_fill_and_pad_of_every_type_are_nat(self):
        for values in ([TT2000_FILL, TT2000_PAD], [-1e31, 0.0, np.nan], [complex(-1e31, -1e31), 0j]):
            with self.subTest(values=values):
                self.assertTrue(np.isnat(CDFepoch.to_datetime(values)).all())

    def test_tt2000_fill_and_pad_are_nat(self):
        result = CDFepoch.to_datetime([TT2000_FILL, TT2000_PAD])
        self.assertTrue(np.isnat(result).all())


@ddt
class Unixtime(SameAs):
    @data(*UNIXTIME_FORMS)
    @unpack
    def test_unixtime(self, kind, label, values):
        self.assertSameCall("unixtime", values)

    @data(*KINDS)
    def test_year_0_raises_like_datetime(self, kind):
        pad = SAMPLES[kind][YEAR_0[kind](SAMPLES[kind])]
        with self.assertRaises(ValueError):
            REF.unixtime(pad)
        with self.assertRaises(ValueError):
            CDFepoch.unixtime(pad)


TIMESTAMPS = [("scalar", 1483228799.5), ("list", [0.0, -0.25, 1e9 + 0.123456]),
              ("array", np.concatenate([RNG.uniform(-2e9, 4e9, 998), [np.nan, 1483228800.0]])),
              # NaN filled with a CDF_EPOCH16 fill value makes complex timestamps: cdflib takes the real part
              ("complex", np.array([0j, 15552000.5 + 0j, 1e9 - 1e31j]))]


@ddt
class FromTimestamp(SameAs):
    @data(*[(method, label, values) for method in ("timestamp_to_cdfepoch", "timestamp_to_cdfepoch16",
                                                    "timestamp_to_tt2000") for label, values in TIMESTAMPS])
    @unpack
    def test_timestamp(self, method, label, values):
        self.assertSameCall(method, values)


COMPONENTS = {
    "tt2000": [[2016, 12, 31, 23, 59, 60, 500, 0, 0], [2000, 1, 1, 12, 0, 0, 0, 0, 0],
               [1965, 3, 4, 5, 6, 7, 8, 9, 10], [9999, 12, 31, 23, 59, 59, 999, 999, 999],
               [0, 1, 1, 0, 0, 0, 0, 0, 0], [2020, 0, 45, 1, 2, 3, 4, 5, 6], [2280, 1, 1, 0, 0, 0, 0, 0, 0],
               [1707, 9, 23, 0, 0, 0, 0, 0, 0]],
    "epoch": [[2020, 1, 1, 1, 1, 1, 111], [1970, 1, 1, 0, 0, 0, 0], [5, 3, 4, 5, 6, 7, 8],
              [2020, 0, 45, 1, 2, 3, 4], [0, 1, 1, 0, 0, 0, 0]],
    "epoch16": [[2017, 1, 1, 1, 1, 1, 123, 456, 789, 999], [1970, 1, 1, 0, 0, 0, 0, 0, 0, 0],
                [9999, 12, 31, 23, 59, 59, 999, 999, 999, 999], [2020, 0, 45, 1, 2, 3, 4, 5, 6, 7],
                [0, 1, 1, 0, 0, 0, 0, 0, 0, 0]],
}
NON_BINARY_FRACTIONS = [[2020, 3, 4.1], [2020, 3, 4, 6.7], [2020, 3, 4, 6, 30.3], [2020, 3, 4, 6, 30, 15.1],
                        [2020, 3, 4, 6, 30, 15, 1.9], [2020, 3, 4, 6, 30, 15, 1, 2.3], [2020, 3, 4, 6, 30, 15, 1, 2, 3.3]]
PARTIAL = {"tt2000": [[2020, 3, 4.5], [2020, 3, 4, 6.25], [2020, 3, 4, 6, 30.5], [2020, 3, 4, 6, 30, 15.125],
                      [2020, 3, 4, 6, 30, 15, 1.5], [2020, 3, 4, 6, 30, 15, 1, 2.5]],
           "epoch": [[2020, 3, 4.5], [2020, 3, 4, 6.25], [2020, 3, 4, 6, 30.5], [2020, 3, 4, 6, 30, 15.125]],
           "epoch16": [[2020, 3, 4.5], [2020, 3, 4, 6.25], [2020, 3, 4, 6, 30.5], [2020, 3, 4, 6, 30, 15.125],
                       [2020, 3, 4, 6, 30, 15, 1.5], [2020, 3, 4, 6, 30, 15, 1, 2.5],
                       [2020, 3, 4, 6, 30, 15, 1, 2, 3.5]]}
NOT_IN_CDFLIB_COMPUTE_EPOCH = ([9999, 12, 31, 23, 59, 59, 999],)


@ddt
class Compute(SameAs):
    @data(*KINDS)
    def test_compute_single(self, kind):
        for components in COMPONENTS[kind]:
            with self.subTest(components=components):
                self.assertSameCall("compute", components)
                self.assertSameCall("compute_" + kind, components)
                self.assertSameCall("compute_" + kind, np.array(components))

    @data(*KINDS)
    def test_compute_many(self, kind):
        self.assertSameCall("compute", COMPONENTS[kind])
        self.assertSameCall("compute_" + kind, COMPONENTS[kind])
        self.assertSameCall("compute", np.array(COMPONENTS[kind]))

    @data(*KINDS)
    def test_round_trip_through_breakdown(self, kind):
        values = SAMPLES[kind][:-2]
        components = REF.breakdown(values)
        self.assertSameCall("compute_" + kind, components)

    @data(*KINDS)
    def test_fraction_of_last_component(self, kind):
        size = {"tt2000": 9, "epoch": 7, "epoch16": 10}[kind]
        for components in PARTIAL[kind] + [c for c in NON_BINARY_FRACTIONS if len(c) < size]:
            with self.subTest(components=components):
                self.assertSameCall("compute_" + kind, components)
        self.assertSameCall("compute_" + kind, [c + [0] * (len(PARTIAL[kind][-1]) - len(c))
                                                for c in PARTIAL[kind]])

    def test_errors(self):
        cases = (("compute", 5, TypeError), ("compute", [2020, 1, 1, 0], TypeError),
                 ("compute_tt2000", 5, TypeError), ("compute_tt2000", [2020, 1], ValueError),
                 ("compute_epoch", [2020, 1], ValueError), ("compute_epoch16", [2020, 1], ValueError),
                 ("compute_epoch", [-1, 1, 1, 0, 0, 0, 0], ValueError),
                 ("compute_epoch16", [-1, 1, 1, 0, 0, 0, 0, 0, 0, 0], ValueError))
        for method, value, error in cases:
            with self.subTest(method=method, value=value):
                for reference in (REF, CDFepoch):
                    function = getattr(reference, method)
                    with self.assertRaises(error):
                        function(value)

    def test_epoch_fill_components_give_fill(self):
        self.assertEqual(CDFepoch.compute_epoch([9999, 12, 31, 23, 59, 59, 999]), -1e31)
        np.testing.assert_array_equal(
            CDFepoch.compute_epoch([[9999, 12, 31, 23, 59, 59, 999], [2020, 1, 1, 0, 0, 0, 0]]),
            [-1e31, 63745056000000.0])

    def test_epoch_out_of_range_components_give_one_value_each(self):
        np.testing.assert_array_equal(
            CDFepoch.compute_epoch([[2020, 1, 1, 25, 0, 0, 0], [2020, 1, 1, 0, 0, 0, 0]]),
            [REF.compute_epoch([2020, 1, 2, 1, 0, 0, 0]), 63745056000000.0])


class TimestampDifferences(unittest.TestCase):
    def test_unixtime_of_the_tt2000_fill_gives_the_fill(self):
        """unixtime of the TT2000 fill value is 253402300800.0: cdflib raises AttributeError converting it back."""
        unix_fill = REF.unixtime(TT2000_FILL)
        with self.assertRaises(AttributeError):
            REF.timestamp_to_tt2000(unix_fill)
        np.testing.assert_array_equal(CDFepoch.timestamp_to_tt2000([0.0, unix_fill]),
                                      [REF.timestamp_to_tt2000(0.0)[0], TT2000_FILL])


class EncodeDifferences(unittest.TestCase):
    def test_tt2000_list_with_fill_encodes_each_value(self):
        self.assertEqual(CDFepoch.encode_tt2000([0, TT2000_FILL, TT2000_PAD]),
                         ["2000-01-01T11:58:55.816000000", "9999-12-31T23:59:59.999999999",
                          "0000-01-01T00:00:00.000000000"])


class FirstLeapSecond(unittest.TestCase):
    """cdflib breaks 1972-06-30T23:59:60 down as 23:59:59, like the second before it."""
    VALUE = REF.compute_tt2000([1972, 6, 30, 23, 59, 60, 123, 456, 789])

    def test_breakdown(self):
        np.testing.assert_array_equal(CDFepoch.breakdown_tt2000(self.VALUE), [1972, 6, 30, 23, 59, 60, 123, 456, 789])

    def test_encode(self):
        self.assertEqual(CDFepoch.encode_tt2000(self.VALUE), "1972-06-30T23:59:60.123456789")

    def test_to_datetime_and_unixtime_like_other_leap_seconds(self):
        np.testing.assert_array_equal(CDFepoch.to_datetime(self.VALUE),
                                      np.array(["1972-07-01T00:00:00.123456789"], dtype="datetime64[ns]"))
        self.assertEqual(CDFepoch.unixtime(self.VALUE), 78796800.123456)

    def test_second_before_is_unchanged(self):
        before = self.VALUE - SECOND
        np.testing.assert_array_equal(CDFepoch.breakdown_tt2000(before), REF.breakdown_tt2000(before))


RANGES = {
    "tt2000": (lambda v: np.sort(v[:-2]), lambda v: (int(v[10]), [2016, 12, 31, 23, 59, 59, 0, 0, 0])),
    "epoch": (lambda v: np.sort(v[:-2]), lambda v: (float(v[10]), [2016, 12, 31, 23, 59, 59, 0])),
    "epoch16": (lambda v: v[:-2][np.lexsort((v[:-2].imag, v[:-2].real))],
                lambda v: (complex(v[10]), [2016, 12, 31, 23, 59, 59, 0, 0, 0, 0])),
}


@ddt
class EpochRange(SameAs):
    @data(*KINDS)
    def test_ranges(self, kind):
        sort, bounds = RANGES[kind]
        values = sort(SAMPLES[kind])
        value_bound, list_bound = bounds(values)
        for start, end in ((None, None), (value_bound, None), (None, value_bound), (list_bound, None),
                           (value_bound, list_bound), (values[0], values[0]), (values[-1], None)):
            with self.subTest(start=start, end=end):
                self.assertSameCall("findepochrange", values, start, end)
                self.assertSameCall("epochrange_" + kind, values, start, end)

    def test_epoch16_out_of_range_is_none(self):
        values = RANGES["epoch16"][0](SAMPLES["epoch16"])
        self.assertSameCall("epochrange_epoch16", values, complex(values[-1].real + 1, 0), None)
        self.assertSameCall("epochrange_epoch16", values, None, complex(values[0].real - 1, 0))

    def test_errors(self):
        cases = (("epochrange_tt2000", ([1.5], None, None), ValueError),
                 ("epochrange_tt2000", ([1, 2], 3, 1), ValueError),
                 ("epochrange_tt2000", ([1, 2], "x", None), ValueError),
                 ("epochrange_epoch", ([1, 2], None, None), TypeError),
                 ("epochrange_epoch", ([1.0, 2.0], 3.0, 1.0), ValueError),
                 ("epochrange_epoch", ([1.0, 2.0], "x", None), TypeError),
                 ("epochrange_epoch16", ([1j], "x", None), ValueError),
                 ("findepochrange", (["a"], None, None), TypeError))
        for method, args, error in cases:
            with self.subTest(method=method, args=args):
                for reference in (REF, CDFepoch):
                    function = getattr(reference, method)
                    with self.assertRaises(error):
                        function(*args)


class LeapSeconds(unittest.TestCase):
    def test_every_leap_second_of_cdflib_table_is_second_60(self):
        days = [row for row in REF.LTS[REF.NERA1 + 1:]]
        values = [REF.compute_tt2000([y, m, d, 0, 0, 0, 0, 0, 0]) - SECOND // 2 for y, m, d, *_ in days]
        np.testing.assert_array_equal(CDFepoch.breakdown_tt2000(values)[:, 5], 60)

    def test_leapsec_file_is_cdflib_public_constant(self):
        import cdflib.epochs
        from pycdfpp.cdflib import epochs
        self.assertEqual(epochs.LEAPSEC_FILE, cdflib.epochs.LEAPSEC_FILE)
        self.assertEqual(CDFepoch.LTS, REF.LTS)


class Package(unittest.TestCase):
    def test_cdfepoch_alias(self):
        import pycdfpp.cdflib
        self.assertIs(pycdfpp.cdflib.cdfepoch, CDFepoch)


if __name__ == "__main__":
    unittest.main()
