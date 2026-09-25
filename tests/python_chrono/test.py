#!/usr/bin/env python
# -*- coding: utf-8 -*-
import os
import time
from datetime import datetime, timedelta, timezone
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


@unittest.skipUnless(hasattr(time, "tzset"), "needs time.tzset to change the local timezone")
class PycdfDatetimeTimezones(unittest.TestCase):
    """Naive datetimes are UTC, whatever the machine's timezone; aware ones are converted."""

    def setUp(self):
        self._tz = os.environ.get("TZ")
        os.environ["TZ"] = "Asia/Tokyo"
        time.tzset()

    def tearDown(self):
        os.environ["TZ"] = self._tz or "UTC"
        time.tzset()

    def _assert_is(self, cdf_times, expected):
        np.testing.assert_array_equal(
            np.asarray(pycdfpp.to_datetime64(cdf_times)).reshape(-1),
            np.array([expected], dtype="datetime64[ns]"))

    def test_naive_datetime_is_utc(self):
        dt = datetime(2020, 1, 1, 12)
        for convert in (pycdfpp.to_tt2000, pycdfpp.to_epoch, pycdfpp.to_epoch16):
            with self.subTest(convert=convert.__name__):
                self._assert_is(convert(dt), "2020-01-01T12:00")
                self._assert_is(convert([dt]), "2020-01-01T12:00")
        self._assert_is([dt], "2020-01-01T12:00")

    def test_aware_datetime_is_converted_to_utc(self):
        dt = datetime(2020, 1, 1, 14, tzinfo=timezone(timedelta(hours=2)))
        for convert in (pycdfpp.to_tt2000, pycdfpp.to_epoch, pycdfpp.to_epoch16):
            with self.subTest(convert=convert.__name__):
                self._assert_is(convert(dt), "2020-01-01T12:00")
                self._assert_is(convert([dt]), "2020-01-01T12:00")
        self._assert_is([dt], "2020-01-01T12:00")
        self.assertEqual(pycdfpp.to_datetime([dt]), [datetime(2020, 1, 1, 12)])


@unittest.skipUnless(hasattr(time, "tzset"), "needs time.tzset to change the local timezone")
class PycdfCdfTimesAreUtc(unittest.TestCase):
    """Every conversion from a CDF time gives UTC, whatever the machine's timezone."""
    EXPECTED = datetime(2020, 6, 1, 12, 30, 15)
    TIMEZONES = ("Asia/Tokyo", "America/New_York")  # +9, and -4 with daylight saving in June

    def setUp(self):
        self._tz = os.environ.get("TZ")

    def tearDown(self):
        os.environ["TZ"] = self._tz or "UTC"
        time.tzset()

    def _cdf_times(self):
        dt64 = np.array([self.EXPECTED], dtype="datetime64[ns]")
        cdf = pycdfpp.CDF()
        for name, data_type in (("tt2000", pycdfpp.DataType.CDF_TIME_TT2000),
                                ("epoch", pycdfpp.DataType.CDF_EPOCH),
                                ("epoch16", pycdfpp.DataType.CDF_EPOCH16)):
            cdf.add_variable(name, values=dt64, data_type=data_type)
        return cdf

    def test_every_path_gives_utc(self):
        for tz in self.TIMEZONES:
            os.environ["TZ"] = tz
            time.tzset()
            cdf = self._cdf_times()
            for name, time_type in (("tt2000", pycdfpp.tt2000_t), ("epoch", pycdfpp.epoch),
                                    ("epoch16", pycdfpp.epoch16)):
                var = cdf[name]
                scalar = time_type(*var.values[0].item())
                with self.subTest(tz=tz, time_type=name):
                    self.assertEqual(pycdfpp.to_datetime(var), [self.EXPECTED])
                    self.assertEqual(pycdfpp.to_datetime(var.values), [self.EXPECTED])
                    self.assertEqual(pycdfpp.to_datetime(scalar), self.EXPECTED)
                    self.assertEqual(pycdfpp.to_datetime([scalar]), [self.EXPECTED])
                    self.assertEqual(pycdfpp.to_datetime64(var)[0], np.datetime64(self.EXPECTED, "ns"))
                    self.assertEqual(pycdfpp.to_time_string(var, "%Y-%m-%d %H:%M:%S")[0][:19],
                                     b"2020-06-01 12:30:15")
                    self.assertTrue(str(scalar).startswith("2020-06-01T12:30:15"), str(scalar))


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


# NASA CDF library 3.9.2 (CDF_TT2000_from_UTC_parts / CDF_TT2000_to_UTC_parts), 1955-1975.
# (UTC ns since 1970, TT2000) pairs for UTC -> TT2000:
NASA_UTC_TO_TT2000 = [(-315619200000000001, -1262347167816000001), (-315619200000000000, -1262347166871870000), (-315619199999999999, -1262347166871869999), (-283996800000000001, -1230724766398830001), (-283996800000000000, -1230724766392534000), (-283996799999999999, -1230724766392533999), (-265680000000000001, -1212407966119078002), (-265680000000000000, -1212407966167782000), (-265679999999999999, -1212407966167781999), (-252460800000000001, -1199188765970790001), (-252460800000000000, -1199188765969580400), (-252460799999999999, -1199188765969580399), (-194659200000000001, -1141387165219282801), (-194659200000000000, -1141387165118159600), (-194659199999999999, -1141387165118159599), (-189388800000000001, -1136116765050767601), (-189388800000000000, -1136116765049558000), (-189388799999999999, -1136116765049557999), (-181526400000000001, -1128254364932918001), (-181526400000000000, -1128254364831622000), (-181526399999999999, -1128254364831621999), (-168307200000000001, -1115035164634630001), (-168307200000000000, -1115035164533334000), (-168307199999999999, -1115035164533333999), (-157766400000000001, -1104494364376518001), (-157766400000000000, -1104494364275222000), (-157766399999999999, -1104494364275221999), (-152668800000000001, -1099396764200054001), (-152668800000000000, -1099396764098758000), (-152668799999999999, -1099396764098757999), (-142128000000000001, -1088855963941942001), (-142128000000000000, -1088855963840646000), (-142127999999999999, -1088855963840645999), (-136771200000000001, -1083499163761590001), (-136771200000000000, -1083499163660294001), (-136771199999999999, -1083499163660294000), (-126230400000000001, -1072958363503478002), (-126230400000000000, -1072958363501534000), (-126230399999999999, -1072958363501533999), (-60480000000000001, -1007207961531614001), (-60480000000000000, -1007207961629022000), (-60479999999999999, -1007207961629021999), (63071999999999999, -883655957925054001), (63072000000000000, -883655957816000000), (63072000000000001, -883655957815999999), (-459118800000000000, -1405846767816000000), (-315619200000000001, -1262347167816000001), (0, -946727959814622001), (63071999999999999, -883655957925054001), (-345029610554492515, -1291757578370492515), (-191044511888092585, -1137772476960200985), (-312509326862625058, -1259237293689135058), (-355216709923086167, -1301944677739086167), (155379212031418704, -791348742784581296), (-436742659500273255, -1383470627316273255), (-365362340539413906, -1312090308355413906), (-205145851835733877, -1151873817190923877), (10287823642446070, -936440135863727930), (44517165409618912, -902210793070123088), (-152653550226170452, -1099381514324928452), (-289310943274259523, -1236038909752145523), (13855632158376826, -932872327241525174), (-153021754547001226, -1099749718752239226), (-225145736688008912, -1171873702302658112), (-355545173559472263, -1302273141375472263), (-35359241045462767, -982087201922804767), (-59510559393355881, -1006238520993865881), (-76831648172448749, -1023559610193950749), (-168413808102684157, -1115141772738610157), (-423290652771027585, -1370018620587027585), (56295222053090256, -890432736074139744), (-329470834269929851, -1276198802085929851), (-382535160184266477, -1329263128000266477), (-135374059435620111, -1082102023075178111), (-56448798476479807, -1003176759986269807), (-251691952775079489, -1198419918735674289), (-393193966237527568, -1339921934053527568), (-35129898346333488, -981857859215899488), (49360888039229241, -897367070295360759), (-285858195189226431, -1232586161615272431), (-63786365665726968, -1010514327295836968), (-391064109378132565, -1337792077194132565), (142435745167490561, -804292209648509439), (-191137818102264364, -1137865783175495964), (59564140704546543, -887163817324187458), (-162156947953746441, -1108884912395064441), (-220186546680413639, -1166914512231040439), (-99513061924544209, -1046241024625150209), (-408893535981474247, -1355621503797474247)]
# (TT2000, UTC ns since 1970) pairs for TT2000 -> UTC:
NASA_TT2000_TO_UTC = [(-1405846767816000001, -459118800000000001), (-1405846767816000000, -459118800000000000), (-1405846767815999999, -459118799999999999), (-1383470627316273256, -436742659500273256), (-1383470627316273255, -436742659500273255), (-1383470627316273254, -436742659500273254), (-1370018620587027586, -423290652771027586), (-1370018620587027585, -423290652771027585), (-1370018620587027584, -423290652771027584), (-1355621503797474248, -408893535981474248), (-1355621503797474247, -408893535981474247), (-1355621503797474246, -408893535981474246), (-1339921934053527569, -393193966237527569), (-1339921934053527568, -393193966237527568), (-1339921934053527567, -393193966237527567), (-1337792077194132566, -391064109378132566), (-1337792077194132565, -391064109378132565), (-1337792077194132564, -391064109378132564), (-1329263128000266478, -382535160184266478), (-1329263128000266477, -382535160184266477), (-1329263128000266476, -382535160184266476), (-1312090308355413907, -365362340539413907), (-1312090308355413906, -365362340539413906), (-1312090308355413905, -365362340539413905), (-1302273141375472264, -355545173559472264), (-1302273141375472263, -355545173559472263), (-1302273141375472262, -355545173559472262), (-1301944677739086168, -355216709923086168), (-1301944677739086167, -355216709923086167), (-1301944677739086166, -355216709923086166), (-1291757578370492516, -345029610554492516), (-1291757578370492515, -345029610554492515), (-1291757578370492514, -345029610554492514), (-1276198802085929852, -329470834269929852), (-1276198802085929851, -329470834269929851), (-1276198802085929850, -329470834269929850), (-1262347167816000002, -315619200000000002), (-1262347167816000001, -315619200000000001), (-1262347167816000000, -315619200000000000), (-1262347166871870001, -315619199055870001), (-1262347166871870000, -315619200000000000), (-1262347166871869999, -315619199999999999), (-1262347166871869998, -315619199999999998), (-1259237293689135059, -312509326862625059), (-1259237293689135058, -312509326862625058), (-1259237293689135057, -312509326862625057), (-1236038909752145524, -289310943274259524), (-1236038909752145523, -289310943274259523), (-1236038909752145522, -289310943274259522), (-1232586161615272432, -285858195189226432), (-1232586161615272431, -285858195189226431), (-1232586161615272430, -285858195189226430), (-1230724766398830002, -283996800000000002), (-1230724766398830001, -283996800000000001), (-1230724766398830000, -283996800000000000), (-1230724766392534001, -283996799993704001), (-1230724766392534000, -283996800000000000), (-1230724766392533999, -283996799999999999), (-1230724766392533998, -283996799999999998), (-1212407966167782001, -265680000048704000), (-1212407966167782000, -265680000000000000), (-1212407966167781999, -265679999999999999), (-1212407966167781998, -265679999999999998), (-1212407966119078003, -265679999951296003), (-1212407966119078002, -265679999951296002), (-1212407966119078001, -265679999951296001), (-1199188765970790002, -252460800000000002), (-1199188765970790001, -252460800000000001), (-1199188765970790000, -252460800000000000), (-1199188765969580401, -252460799998790401), (-1199188765969580400, -252460800000000000), (-1199188765969580399, -252460799999999999), (-1199188765969580398, -252460799999999998), (-1198419918735674290, -251691952775079490), (-1198419918735674289, -251691952775079489), (-1198419918735674288, -251691952775079488), (-1171873702302658113, -225145736688008913), (-1171873702302658112, -225145736688008912), (-1171873702302658111, -225145736688008911), (-1166914512231040440, -220186546680413640), (-1166914512231040439, -220186546680413639), (-1166914512231040438, -220186546680413638), (-1151873817190923878, -205145851835733878), (-1151873817190923877, -205145851835733877), (-1151873817190923876, -205145851835733876), (-1141387165219282802, -194659200000000002), (-1141387165219282801, -194659200000000001), (-1141387165219282800, -194659200000000000), (-1141387165118159601, -194659199898876801), (-1141387165118159600, -194659200000000000), (-1141387165118159599, -194659199999999999), (-1141387165118159598, -194659199999999998), (-1137865783175495965, -191137818102264365), (-1137865783175495964, -191137818102264364), (-1137865783175495963, -191137818102264363), (-1137772476960200986, -191044511888092586), (-1137772476960200985, -191044511888092585), (-1137772476960200984, -191044511888092584), (-1136116765050767602, -189388800000000002), (-1136116765050767601, -189388800000000001), (-1136116765050767600, -189388800000000000), (-1136116765049558001, -189388799998790401), (-1136116765049558000, -189388800000000000), (-1136116765049557999, -189388799999999999), (-1136116765049557998, -189388799999999998), (-1128254364932918002, -181526400000000002), (-1128254364932918001, -181526400000000001), (-1128254364932918000, -181526400000000000), (-1128254364831622001, -181526399898704001), (-1128254364831622000, -181526400000000000), (-1128254364831621999, -181526399999999999), (-1128254364831621998, -181526399999999998), (-1115141772738610158, -168413808102684158), (-1115141772738610157, -168413808102684157), (-1115141772738610156, -168413808102684156), (-1115035164634630002, -168307200000000002), (-1115035164634630001, -168307200000000001), (-1115035164634630000, -168307200000000000), (-1115035164533334001, -168307199898704001), (-1115035164533334000, -168307200000000000), (-1115035164533333999, -168307199999999999), (-1115035164533333998, -168307199999999998), (-1108884912395064442, -162156947953746442), (-1108884912395064441, -162156947953746441), (-1108884912395064440, -162156947953746440), (-1104494364376518002, -157766400000000002), (-1104494364376518001, -157766400000000001), (-1104494364376518000, -157766400000000000), (-1104494364275222001, -157766399898704001), (-1104494364275222000, -157766400000000000), (-1104494364275221999, -157766399999999999), (-1104494364275221998, -157766399999999998), (-1099749718752239227, -153021754547001227), (-1099749718752239226, -153021754547001226), (-1099749718752239225, -153021754547001225), (-1099396764200054002, -152668800000000002), (-1099396764200054001, -152668800000000001), (-1099396764200054000, -152668800000000000), (-1099396764098758001, -152668799898704001), (-1099396764098758000, -152668800000000000), (-1099396764098757999, -152668799999999999), (-1099396764098757998, -152668799999999998), (-1099381514324928453, -152653550226170453), (-1099381514324928452, -152653550226170452), (-1099381514324928451, -152653550226170451), (-1088855963941942002, -142128000000000002), (-1088855963941942001, -142128000000000001), (-1088855963941942000, -142128000000000000), (-1088855963840646001, -142127999898704001), (-1088855963840646000, -142128000000000000), (-1088855963840645999, -142127999999999999), (-1088855963840645998, -142127999999999998), (-1083499163761590002, -136771200000000002), (-1083499163761590001, -136771200000000001), (-1083499163761590000, -136771200000000000), (-1083499163660294002, -136771199898704002), (-1083499163660294001, -136771200000000000), (-1083499163660294000, -136771199999999999), (-1083499163660293999, -136771199999999998), (-1082102023075178112, -135374059435620112), (-1082102023075178111, -135374059435620111), (-1082102023075178110, -135374059435620110), (-1072958363503478003, -126230400000000002), (-1072958363503478002, -126230400000000001), (-1072958363503478001, -126230400000000000), (-1072958363501534001, -126230399998056000), (-1072958363501534000, -126230400000000000), (-1072958363501533999, -126230399999999999), (-1072958363501533998, -126230399999999998), (-1046241024625150210, -99513061924544210), (-1046241024625150209, -99513061924544209), (-1046241024625150208, -99513061924544208), (-1023559610193950750, -76831648172448750), (-1023559610193950749, -76831648172448749), (-1023559610193950748, -76831648172448748), (-1010514327295836969, -63786365665726969), (-1010514327295836968, -63786365665726968), (-1010514327295836967, -63786365665726967), (-1007207961629022001, -60480000097408001), (-1007207961629022000, -60480000000000000), (-1007207961629021999, -60479999999999999), (-1007207961629021998, -60479999999999998), (-1007207961531614002, -60479999902592002), (-1007207961531614001, -60479999902592001), (-1007207961531614000, -60479999902592000), (-1006238520993865882, -59510559393355882), (-1006238520993865881, -59510559393355881), (-1006238520993865880, -59510559393355880), (-1003176759986269808, -56448798476479808), (-1003176759986269807, -56448798476479807), (-1003176759986269806, -56448798476479806), (-982087201922804768, -35359241045462768), (-982087201922804767, -35359241045462767), (-982087201922804766, -35359241045462766), (-981857859215899489, -35129898346333489), (-981857859215899488, -35129898346333488), (-981857859215899487, -35129898346333487), (-946727959814622002, 2591998), (-946727959814622001, 0), (-946727959814622000, 1), (-936440135863727931, 10287823642446069), (-936440135863727930, 10287823642446070), (-936440135863727929, 10287823642446071), (-932872327241525175, 13855632158376825), (-932872327241525174, 13855632158376826), (-932872327241525173, 13855632158376827), (-902210793070123089, 44517165409618911), (-902210793070123088, 44517165409618912), (-902210793070123087, 44517165409618913), (-897367070295360760, 49360888039229240), (-897367070295360759, 49360888039229241), (-897367070295360758, 49360888039229242), (-890432736074139745, 56295222053090255), (-890432736074139744, 56295222053090256), (-890432736074139743, 56295222053090257), (-887163817324187459, 59564140704546542), (-887163817324187458, 59564140704546543), (-887163817324187457, 59564140704546544), (-883655957925054002, 63071999999999998), (-883655957925054001, 63071999999999999), (-883655957925054000, 63072000000000000), (-883655957816000001, 63072000109053999), (-883655957816000000, 63072000000000000), (-883655957815999999, 63072000000000001), (-883655957815999998, 63072000000000002), (-804292209648509440, 142435745167490560), (-804292209648509439, 142435745167490561), (-804292209648509438, 142435745167490562), (-791348742784581297, 155379212031418703), (-791348742784581296, 155379212031418704), (-791348742784581295, 155379212031418705)]


class PycdfTT2000Before1972(unittest.TestCase):
    """Before 1972, TAI-UTC drifted by a fraction of a second per day. TT2000 conversions must
    match NASA's library, on the SIMD (arrays) and scalar (single values) paths alike."""

    @staticmethod
    def _tt2000_array(values):
        dtype = pycdfpp.to_tt2000(np.array(["2000-01-01"], dtype="datetime64[ns]")).dtype
        return np.array([(v,) for v in values], dtype=dtype)

    def test_utc_to_tt2000(self):
        utc, tt = zip(*NASA_UTC_TO_TT2000)
        result = pycdfpp.to_tt2000(np.array(utc, dtype=np.int64).astype("datetime64[ns]"))
        self.assertEqual(result["nseconds"].tolist(), list(tt))

    def test_tt2000_to_utc_arrays(self):
        tt, utc = zip(*NASA_TT2000_TO_UTC)
        result = pycdfpp.to_datetime64(self._tt2000_array(tt)).astype(np.int64)
        self.assertEqual(result.tolist(), list(utc))

    def test_tt2000_to_utc_single_values(self):
        for tt, utc in NASA_TT2000_TO_UTC:
            with self.subTest(tt2000=tt):
                self.assertEqual(int(pycdfpp.to_datetime64(pycdfpp.tt2000_t(tt)).astype(np.int64)), utc)

    def test_batches_mixing_old_and_recent_times(self):
        recent = pycdfpp.to_tt2000(np.array(["2020-01-01"], dtype="datetime64[ns]"))["nseconds"][0]
        recent_utc = np.datetime64("2020-01-01", "ns").astype(np.int64)
        tt, utc = [], []
        for pair in NASA_TT2000_TO_UTC[:40]:
            tt += [pair[0], recent]
            utc += [pair[1], recent_utc]
        result = pycdfpp.to_datetime64(self._tt2000_array(tt)).astype(np.int64)
        self.assertEqual(result.tolist(), utc)


if __name__ == '__main__':
    unittest.main()
