"""cdflib.epochs (1.3.16) on pycdfpp's vectorised time conversions."""
import functools
import re
from typing import List, Optional, Tuple, Union

import numpy as np
import numpy.typing as npt

import pycdfpp
from cdflib.epochs import LEAPSEC_FILE

epochs_type = Union[str, List[float], List[int], List[complex], Tuple[float, ...], Tuple[int, ...], Tuple[complex, ...], np.ndarray]


epoch_scalar_types = Union[int, np.int64, float, np.float64, complex, np.complex128]
epoch_list_types = Union[List[int], List[np.int64], List[float], List[np.float64], List[complex], List[np.complex128]]
epoch_types = Union[epoch_scalar_types, epoch_list_types, npt.NDArray]

encoded_type = Union[str, List[str]]

cdf_epoch_type = Union[float, np.float64, List[float], npt.NDArray[np.float64]]
cdf_epoch16_type = Union[complex, np.complex128, List[complex], npt.NDArray[np.complex128]]
cdf_tt2000_type = Union[int, np.int64, List[int], npt.NDArray[np.int64]]

_SECOND = 1_000_000_000
_NS = "datetime64[ns]"
_NOT_SURE_HOW = "Not sure how to handle this type"
_FILL = (9999, 12, 31, 23, 59, 59, 999, 999, 999, 999)
_PAD = (0, 1, 1, 0, 0, 0, 0, 0, 0, 0)
_LAST_DATETIME64_NS_DAY = np.datetime64("2262-04-11", "D")
_MS_FROM_YEAR_0_TO_1970 = 62167219200000
_S_FROM_YEAR_0_TO_1970 = 62167219200
_UNIX_TT2000_FILL = 253402300799.999  # 9999-12-31T23:59:59.999, the fill date
# pycdfpp's numpy record types for raw CDF time values
_RECORDS = {
    np.dtype(np.int64): np.dtype([("nseconds", "<i8")]),
    np.dtype(np.float64): np.dtype([("mseconds", "<f8")]),
    np.dtype(np.complex128): np.dtype([("seconds", "<f8"), ("picoseconds", "<f8")]),
}
_KIND = {np.dtype(np.int64): "tt2000", np.dtype(np.float64): "epoch", np.dtype(np.complex128): "epoch16"}
# Columns of each field in pycdfpp's "%Y%m%d%H%M%S" text: YYYYMMDDHHMMSS.mmmuuunnn
_TEXT_COLUMNS = ((0, 4), (4, 6), (6, 8), (8, 10), (10, 12), (12, 14), (15, 18), (18, 21), (21, 24))
# How many of each field make one of the field before it: day..picosecond
_UNITS_PER_PREVIOUS = (None, None, None, 24, 60, 60, 1000, 1000, 1000, 1000)
_ENCODINGS = {
    (7, True): "{0:04d}-{1:02d}-{2:02d}T{3:02d}:{4:02d}:{5:02d}.{6:03d}",
    (9, True): "{0:04d}-{1:02d}-{2:02d}T{3:02d}:{4:02d}:{5:02d}.{6:03d}{7:03d}{8:03d}",
    (10, True): "{0:04d}-{1:02d}-{2:02d}T{3:02d}:{4:02d}:{5:02d}.{6:03d}{7:03d}{8:03d}{9:03d}",
    (7, False): "{2:02d}-{month}-{0:04d} {3:02d}:{4:02d}:{5:02d}.{6:03d}",
    (9, False): "{2:02d}-{month}-{0:04d} {3:02d}:{4:02d}:{5:02d}.{6:03d}.{7:03d}.{8:03d}",
    (10, False): "{2:02d}-{month}-{0:04d} {3:02d}:{4:02d}:{5:02d}.{6:03d}.{7:03d}.{8:03d}.{9:03d}",
}
_FIELD_NAMES = ("y", "m", "d", "H", "M", "S", "ms", "us", "ns", "ps")
_ISO_DATE = r"(?P<y>\d+)-(?P<m>\d+)-(?P<d>\d+)T(?P<H>\d+):(?P<M>\d+):(?P<S>\d+)\."
_TEXT_DATE = r"(?P<d>\d+)-(?P<mon>[a-z]{3})-(?P<y>\d+) (?P<H>\d+):(?P<M>\d+):(?P<S>\d+)\."


def _read_leap_seconds(path):
    with open(path) as file:
        rows = [line.split() for line in file if not line.startswith(";")]
    return [[int(v) for v in row[:3]] + [float(v) for v in row[3:6]] for row in rows]


def _squeeze_or_scalar(values):
    values = np.squeeze(values)
    return values[()] if values.ndim == 0 else values


def _complex(real, imag):
    values = np.empty(np.shape(real), dtype=np.complex128)
    values.real = real
    values.imag = imag
    return values


def _as_records(values, dtype):
    return np.ascontiguousarray(values, dtype=dtype).reshape(-1).view(_RECORDS[dtype])


def _kind_of(values, error, message):
    kind = _KIND.get(np.array(values).dtype)
    if kind is None:
        raise error(message)
    return kind


def _utc_fields(values, dtype):
    """(N, 9) year..nanosecond of CDF time values, as pycdfpp writes them (fill: 9999-12-31...)."""
    text = pycdfpp.to_time_string(_as_records(values, dtype), "%Y%m%d%H%M%S")
    digits = np.frombuffer(text.tobytes(), dtype=np.uint8).reshape(len(text), -1).astype(np.int64) - ord("0")
    return np.stack([digits[:, a:b] @ 10 ** np.arange(b - a - 1, -1, -1) for a, b in _TEXT_COLUMNS], axis=-1)


@functools.cache
def _leap_second_starts():
    """From pycdfpp's own table, so detection and conversion never disagree: a leap day is 86401 s long."""
    midnights = np.arange(np.datetime64("1972-01-01"), _LAST_DATETIME64_NS_DAY).astype(_NS)
    tt2000 = pycdfpp.to_tt2000(midnights)["nseconds"]
    return tt2000[1:][np.diff(tt2000) != 86400 * _SECOND] - _SECOND


def _in_leap_second(tt2000):
    starts = _leap_second_starts()
    previous = np.clip(np.searchsorted(starts, tt2000, side="right") - 1, 0, None)
    return (tt2000 >= starts[0]) & (tt2000 - starts[previous] < _SECOND)


def _tt2000_of_midnights(midnights):
    """datetime64[ns] ends in 2262 and TT2000 in 2292; no leap second comes after 2262, so days add plainly."""
    clamped = np.minimum(midnights, _LAST_DATETIME64_NS_DAY)
    days_after = (midnights - clamped).astype(np.int64)
    return pycdfpp.to_tt2000(clamped.astype(_NS))["nseconds"] + days_after * 86400 * _SECOND


def _tt2000_fields(tt2000):
    """pycdfpp shows a leap second as the next second; NASA and cdflib show it as second 60."""
    in_leap = _in_leap_second(tt2000)
    fields = _utc_fields(np.where(in_leap, tt2000 - _SECOND, tt2000), np.dtype(np.int64))
    fields[in_leap, 5] = 60
    return fields


def _epoch16_fields(epoch16):
    fill = (epoch16.real == -1.0e31) & (epoch16.imag == -1.0e31)
    date = _utc_fields(np.where(fill, -1.0e31, epoch16.real * 1000.0), np.dtype(np.float64))[:, :6]
    picoseconds = np.where(fill, 999_999_999_999, epoch16.imag).astype(np.int64)
    sub_second = picoseconds[:, None] // 10 ** np.array([9, 6, 3, 0], dtype=np.int64) % 1000
    return np.concatenate([date, sub_second], axis=1)


def _components(dates, size):
    """(N, size) integer components; a fractional last given component spills into the next ones."""
    rows = np.atleast_2d(dates)
    given = min(rows.shape[1], size)
    if given < 3:
        raise ValueError("Invalid date/time components")
    components = np.zeros((rows.shape[0], size), dtype=np.int64)
    components[:, :given] = rows[:, :given].astype(np.int64)
    rest = rows[:, given - 1].astype(np.float64)
    for index in range(given, size):
        rest = _UNITS_PER_PREVIOUS[index] * (rest - np.trunc(rest))
        components[:, index] = np.trunc(rest)
    return components


def _is_row(components, row):
    return np.all(components == row[: components.shape[1]], axis=1)


def _check_years(components):
    if np.any(components[:, 0] < 0):
        raise ValueError("Illegal epoch field")


def _midnights(components, keep):
    """datetime64[D] of each date; month 0 means day of year. Rows not kept get 2000-01-01."""
    years = np.where(keep, components[:, 0], 2000)
    months = np.where(keep & (components[:, 1] != 0), components[:, 1], 1)
    days = np.where(keep, components[:, 2], 1)
    first_of_month = np.array(years - 1970, dtype="datetime64[Y]") + np.array(months - 1, dtype="timedelta64[M]")
    return first_of_month.astype("datetime64[D]") + np.array(days - 1, dtype="timedelta64[D]")


def _days_since_0ad(components):
    keep = np.ones(len(components), dtype=bool)
    return (_midnights(components, keep) - np.datetime64("0000-01-01", "D")).astype(np.int64)


def _seconds_in_day(components):
    return components[:, 3] * 3600 + components[:, 4] * 60 + components[:, 5]


def _unix_microseconds(fields):
    days = _days_since_0ad(fields) - _S_FROM_YEAR_0_TO_1970 // 86400
    microseconds = fields[:, 7] if fields.shape[1] > 7 else 0
    return (days * 86400 + _seconds_in_day(fields)) * 10**6 + fields[:, 6] * 1000 + microseconds


def _to_unixtime(fields):
    """Python's int / int division, as cdflib's datetime.timestamp(); a leap second adds 1 after it."""
    leap = (fields[:, 5] == 60).astype(np.int64)
    microseconds = _unix_microseconds(fields - leap[:, None] * (np.arange(fields.shape[1]) == 5))
    return [us / 10**6 + extra for us, extra in zip(microseconds.tolist(), leap.tolist())]


def _check_unixtime_years(fields):
    if np.any(fields[:, 0] < 1):
        raise ValueError("year 0 is out of range")


def _check_only_fill_and_pad_are_nat(nat_values):
    """pycdfpp also gives NaT for dates datetime64[ns] can't hold; cdflib raises for those."""
    if len(nat_values):
        fields = np.atleast_2d(CDFepoch.breakdown(nat_values))[:, :7]
        if not np.all(_is_row(fields, _FILL) | _is_row(fields, _PAD)):
            raise OverflowError("Date out of the datetime64[ns] range (1678 to 2262)")


def _encode(fields, iso_8601):
    template = _ENCODINGS[(fields.shape[1], bool(iso_8601))]
    encoded = [template.format(*row, month=CDFepoch.month_Token[row[1] - 1]) for row in fields.tolist()]
    return encoded[0] if len(encoded) == 1 else encoded


def _bound(value, default, scalar_types, list_types, compute, error):
    if value is None:
        return default
    if isinstance(value, scalar_types):
        return value
    if isinstance(value, list_types):
        return compute(value)
    raise error("Bad start/end time")


def _check_first_value(epochs, scalar_types, error):
    first = epochs[0] if isinstance(epochs, (list, tuple, np.ndarray)) else epochs
    if not isinstance(first, scalar_types):
        raise error("Bad data")


def _indices_between(values, start, end):
    if start > end:
        raise ValueError("Invalid start/end time")
    values = np.array(values)
    return np.nonzero(np.logical_and(values >= start, values <= end))[0]


def _not_before(values, bound):
    return (values.real > bound.real) | ((values.real == bound.real) & (values.imag >= bound.imag))


def _not_after(values, bound):
    return (values.real < bound.real) | ((values.real == bound.real) & (values.imag <= bound.imag))


def _month_number(name):
    return [month.lower() for month in CDFepoch.month_Token].index(name.lower()) + 1


def _parse_one(text):
    for pattern, compute in _PARSERS:
        match = pattern.fullmatch(text)
        if match:
            fields = match.groupdict()
            if "mon" in fields:
                fields["m"] = _month_number(fields.pop("mon"))
            return compute([int(fields[name]) for name in _FIELD_NAMES if name in fields])
    raise ValueError("Invalid cdf epoch type...")


def _timestamps(unixtime_data) -> np.ndarray:
    """Unix times as an array; cdflib drops the imaginary part of complex ones."""
    return np.atleast_1d(np.real(unixtime_data))


class CDFepoch:
    """
    Convert between CDF-based epochs, np.datetime64, and Unix time.

    There are three (3) epoch data types in CDF:
        1. CDF_EPOCH is milliseconds since Year 0 represented as a
        single double (float in Python),
        2. CDF_EPOCH16 is picoseconds since Year 0 represented as
        2-doubles (complex in Python), and
        3. CDF_TIME_TT2000 (TT2000 as short) is nanoseconds since J2000 with
        leap seconds, represented by an 8-byte integer (int in Python).

    In Numpy, they are np.float64, np.complex128 and np.int64, respectively.
    """

    version = 3
    release = 7
    increment = 0

    month_Token = ["Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"]

    JulianDateJ2000_12h = 2451545
    J2000Since0AD12h = 730485
    J2000Since0AD12hSec = 63113904000.0
    J2000Since0AD12hMilsec = 63113904000000.0
    J2000LeapSeconds = 32.0
    dT = 32.184
    dTinNanoSecs = 32184000000
    MJDbase = 2400000.5
    SECinNanoSecs = 1000000000
    SECinNanoSecsD = 1000000000.0
    DAYinNanoSecs = int(86400000000000)
    HOURinNanoSecs = int(3600000000000)
    MINUTEinNanoSecs = int(60000000000)
    T12hinNanoSecs = int(43200000000000)

    JDY17070922 = 2344793
    JDY22920411 = 2558297
    DEFAULT_TT2000_PADVALUE = int(-9223372036854775807)
    FILLED_TT2000_VALUE = int(-9223372036854775808)
    NERA1 = 14

    LTS = _read_leap_seconds(LEAPSEC_FILE)
    NDAT = len(LTS)

    NST: Optional[List[int]] = None
    currentDay = -1
    currentJDay = -1
    currentLeapSeconds: float = -1

    @staticmethod
    def encode(epochs: epoch_types, iso_8601: bool = True) -> encoded_type:
        kind = _kind_of(epochs, TypeError, _NOT_SURE_HOW)
        return getattr(CDFepoch, "encode_" + kind)(np.array(epochs), iso_8601)

    @staticmethod
    def breakdown(epochs: epoch_types) -> np.ndarray:
        kind = _kind_of(epochs, TypeError, _NOT_SURE_HOW)
        return getattr(CDFepoch, "breakdown_" + kind)(np.array(epochs))

    @classmethod
    def to_datetime(cls, cdf_time: epoch_types) -> npt.NDArray[np.datetime64]:
        """CDF time values to datetime64[ns]; fill and pad values give NaT."""
        values = np.atleast_1d(np.array(cdf_time))
        _kind_of(values, TypeError, _NOT_SURE_HOW)
        times = pycdfpp.to_datetime64(_as_records(values, values.dtype))
        _check_only_fill_and_pad_are_nat(values.reshape(-1)[np.isnat(times)])
        return times

    @staticmethod
    def unixtime(cdf_time: npt.ArrayLike) -> float | npt.NDArray:
        fields = np.atleast_2d(CDFepoch.breakdown(np.atleast_1d(cdf_time)))
        _check_unixtime_years(fields)
        return _squeeze_or_scalar(_to_unixtime(fields))

    @staticmethod
    def timestamp_to_cdfepoch(unixtime_data: npt.ArrayLike) -> np.ndarray:
        times = _timestamps(unixtime_data)
        return np.where(np.isnan(times), np.nan, np.trunc(times * 1000.0) + float(_MS_FROM_YEAR_0_TO_1970))

    @staticmethod
    def timestamp_to_cdfepoch16(unixtime_data: npt.ArrayLike) -> np.ndarray:
        times = _timestamps(unixtime_data)
        missing = np.isnan(times)
        microseconds = np.trunc(np.where(missing, 0, times) * 1000000.0).astype(np.int64)
        epoch16 = _complex(microseconds // 10**6 + _S_FROM_YEAR_0_TO_1970, microseconds % 10**6 * 10**6)
        return np.where(missing, np.nan, epoch16)

    @staticmethod
    def timestamp_to_tt2000(unixtime_data: npt.ArrayLike) -> np.ndarray:
        """Times from the fill date (unixtime of the TT2000 fill) on give the fill value, as NASA's computeTT2000."""
        times = _timestamps(unixtime_data)
        missing, fill = np.isnan(times), times >= _UNIX_TT2000_FILL
        microseconds = np.trunc(np.where(missing | fill, 0, times) * 1000000.0).astype(np.int64)
        tt2000 = pycdfpp.to_tt2000(microseconds.astype("datetime64[us]").astype(_NS))["nseconds"]
        tt2000 = np.where(fill, CDFepoch.FILLED_TT2000_VALUE, tt2000)
        return np.where(missing, np.nan, tt2000) if missing.any() else tt2000

    @staticmethod
    def compute(datetimes: npt.ArrayLike) -> int | float | complex | npt.NDArray:
        if not isinstance(datetimes, (list, tuple, np.ndarray)):
            raise TypeError("datetime must be in list form")
        compute = _COMPUTE_BY_SIZE.get(np.atleast_2d(datetimes).shape[1])
        if compute is None:
            raise TypeError("Unknown input")
        return _squeeze_or_scalar(compute(datetimes))

    @staticmethod
    def findepochrange(
        epochs: epochs_type, starttime: epoch_types | None = None, endtime: epoch_types | None = None
    ) -> np.ndarray:
        kind = _kind_of(epochs, TypeError, "Bad input")
        return getattr(CDFepoch, "epochrange_" + kind)(np.array(epochs), starttime, endtime)

    @staticmethod
    def encode_tt2000(tt2000: cdf_tt2000_type, iso_8601: bool = True) -> encoded_type:
        return _encode(np.atleast_2d(CDFepoch.breakdown_tt2000(tt2000)), iso_8601)

    @staticmethod
    def breakdown_tt2000(tt2000: cdf_tt2000_type) -> np.ndarray:
        values = np.atleast_1d(tt2000).astype(np.int64)
        return np.squeeze(_tt2000_fields(values.reshape(-1)).reshape(values.shape + (9,)))

    @staticmethod
    def compute_tt2000(datetimes: npt.ArrayLike) -> int | npt.NDArray[np.int64]:
        if not isinstance(datetimes, (list, tuple, np.ndarray)):
            raise TypeError("datetime must be in list form")
        components = _components(datetimes, 9)
        fill, pad = _is_row(components, _FILL), _is_row(components, _PAD)
        sub_day = (_seconds_in_day(components) * 1000000000 + components[:, 6] * 1000000 + components[:, 7] * 1000
                   + components[:, 8])
        tt2000 = _tt2000_of_midnights(_midnights(components, ~(fill | pad))) + sub_day
        tt2000 = np.where(fill, CDFepoch.FILLED_TT2000_VALUE, np.where(pad, CDFepoch.DEFAULT_TT2000_PADVALUE, tt2000))
        return np.squeeze(tt2000)

    @staticmethod
    def epochrange_tt2000(
        epochs: cdf_tt2000_type, starttime: epoch_types | None = None, endtime: epoch_types | None = None
    ) -> npt.NDArray:
        _check_first_value(epochs, (int, np.int64), ValueError)
        compute = lambda value: int(CDFepoch.compute_tt2000(value))  # noqa: E731
        start = _bound(starttime, int(-9223372036854775807), (int, np.int64), list, compute, ValueError)
        end = _bound(endtime, int(9223372036854775807), (int, np.int64), (list, tuple), compute, ValueError)
        return _indices_between(epochs, start, end)

    @staticmethod
    def encode_epoch16(epochs: cdf_epoch16_type, iso_8601: bool = True) -> encoded_type:
        return _encode(np.atleast_2d(CDFepoch.breakdown_epoch16(np.atleast_1d(epochs))), iso_8601)

    @staticmethod
    def compute_epoch16(datetimes: npt.ArrayLike) -> complex | npt.NDArray[np.complex128]:
        components = _components(datetimes, 10)
        if np.atleast_2d(datetimes).shape[1] < 9:
            components[:, 9] = 0  # cdflib carries a fraction down to picoseconds only from given nanoseconds
        _check_years(components)
        picoseconds = ((components[:, 6] * 1000 + components[:, 7]) * 1000 + components[:, 8]) * 1000 + components[:, 9]
        seconds = 86400.0 * _days_since_0ad(components) + (_seconds_in_day(components) + picoseconds // 10**12)
        epoch16 = _complex(seconds, picoseconds % 10**12)
        return _squeeze_or_scalar(np.where(_is_row(components, _FILL), complex(-1.0e31, -1.0e31), epoch16))

    @staticmethod
    def breakdown_epoch16(epochs: cdf_epoch16_type) -> npt.NDArray:
        if not isinstance(epochs, (complex, np.complex128, list, tuple, np.ndarray)):
            raise TypeError("Bad data for epochs: {:}".format(type(epochs)))
        values = np.asarray(epochs, dtype=np.complex128)
        return _epoch16_fields(values.reshape(-1)).reshape(values.shape + (10,))

    @staticmethod
    def epochrange_epoch16(
        epochs: cdf_epoch16_type, starttime: epoch_types | None = None, endtime: epoch_types | None = None
    ) -> np.ndarray | None:
        values = np.atleast_1d(epochs)
        compute = lambda value: complex(CDFepoch.compute_epoch16(value))  # noqa: E731
        start = _bound(starttime, complex(-1.0e31, -1.0e31), (complex, np.complex128), list, compute, ValueError)
        end = _bound(endtime, complex(1.0e31, 1.0e31), (complex, np.complex128), list, compute, ValueError)
        if (start.real, start.imag) > (end.real, end.imag):
            raise ValueError("Invalid start/end time")
        from_start, until_end = _not_before(values, start), _not_after(values, end)
        if not until_end[0] or not from_start[-1]:
            return None
        last = np.argmax(~until_end) - 1 if not until_end.all() else len(values) - 1
        return np.arange(np.argmax(from_start), last + 1, step=1)

    @staticmethod
    def encode_epoch(epochs: cdf_epoch_type, iso_8601: bool = True) -> encoded_type:
        return _encode(np.atleast_2d(CDFepoch.breakdown_epoch(epochs)), iso_8601)

    @staticmethod
    def compute_epoch(dates: npt.ArrayLike) -> float | npt.NDArray:
        components = _components(dates, 7)
        _check_years(components)
        milliseconds_in_day = _seconds_in_day(components) * 1000 + components[:, 6]
        epochs = np.where(_is_row(components, _FILL), -1.0e31,
                          86400000.0 * _days_since_0ad(components) + milliseconds_in_day)
        return np.array(epochs[0]) if len(epochs) == 1 else np.squeeze(epochs)

    @staticmethod
    def breakdown_epoch(epochs: cdf_epoch_type) -> np.ndarray:
        if not isinstance(epochs, (float, np.float64, list, tuple, np.ndarray, int)):
            raise TypeError("Bad data for epochs: {:}".format(type(epochs)))
        values = np.asarray(epochs, dtype=np.float64)
        fields = _utc_fields(values.reshape(-1), np.dtype(np.float64))[:, :7]
        return np.squeeze(fields.reshape(values.shape + (7,)))

    @staticmethod
    def epochrange_epoch(
        epochs: epoch_types, starttime: epoch_types | None = None, endtime: epoch_types | None = None
    ) -> np.ndarray:
        _check_first_value(epochs, (float, np.float64), TypeError)
        compute = lambda value: float(CDFepoch.compute_epoch(value))  # noqa: E731
        start = _bound(starttime, 0.0, (float, int, np.float64), (list, tuple), compute, TypeError)
        end = _bound(endtime, 1.0e31, (float, int, np.float64), (list, tuple), compute, TypeError)
        return _indices_between(epochs, start, end)

    @staticmethod
    def parse(value: str | Tuple[str, ...] | List[str]) -> np.ndarray:
        """
        Parses date/time string(s), as encode writes them, into CDF epoch value(s).
        The format picks the type: milliseconds for CDF_EPOCH, nanoseconds for TT2000,
        picoseconds for CDF_EPOCH16.
        """
        if isinstance(value, (list, tuple)) and not isinstance(value[0], str):
            raise TypeError("should be a string or a list of string")
        if not isinstance(value, (list, tuple, str)):
            raise TypeError("Invalid value... should be a string or a list of string")
        if isinstance(value, str):
            return np.squeeze(_parse_one(value))
        return np.squeeze([_parse_one(text) for text in value])


_COMPUTE_BY_SIZE = {7: CDFepoch.compute_epoch, 9: CDFepoch.compute_tt2000, 10: CDFepoch.compute_epoch16}
_SUB_SECONDS = {
    "epoch": (r"(?P<ms>\d{3})", r"(?P<ms>\d{3})"),
    "tt2000": (r"(?P<ms>\d{3})(?P<us>\d{3})(?P<ns>\d{3})", r"(?P<ms>\d{3})\.(?P<us>\d{3})\.(?P<ns>\d{3})"),
    "epoch16": (r"(?P<ms>\d{3})(?P<us>\d{3})(?P<ns>\d{3})(?P<ps>\d{3})",
                r"(?P<ms>\d{3})\.(?P<us>\d{3})\.(?P<ns>\d{3})\.(?P<ps>\d{3})"),
}
_PARSED_AS = {
    "epoch": lambda components: float(CDFepoch.compute_epoch(components)),
    "tt2000": lambda components: int(CDFepoch.compute_tt2000(components)),
    "epoch16": lambda components: complex(CDFepoch.compute_epoch16(components)),
}
_PARSERS = tuple(
    (re.compile(date + sub_second, re.IGNORECASE), _PARSED_AS[kind])
    for kind, sub_seconds in _SUB_SECONDS.items()
    for date, sub_second in zip((_ISO_DATE, _TEXT_DATE), sub_seconds)
)
