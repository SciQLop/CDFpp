"""Values given to cdfwrite.CDF, as pycdfpp takes them: cast to their CDF type as cdflib casts them, numbers that
don't fit their integer type refused."""
import functools
from contextlib import contextmanager
from typing import List, NamedTuple, Optional

import numpy as np

import pycdfpp
from pycdfpp import DataType

from . import epochs as cdfepoch
from . import _types
from .logging import logger

# An untyped attribute value's CDF type, by numpy (kind, item size), 0 for any size. As cdflib: any signed
# integer is CDF_INT8, any float CDF_DOUBLE; uint64, long double and complex64 have none (skipped).
_GUESSED_TYPES = {("b", 1): "CDF_INT8", **{("i", size): "CDF_INT8" for size in (1, 2, 4, 8)},
                  ("u", 1): "CDF_UINT1", ("u", 2): "CDF_UINT2", ("u", 4): "CDF_UINT4",
                  **{("f", size): "CDF_DOUBLE" for size in (2, 4, 8)}, ("c", 16): "CDF_EPOCH16",
                  ("U", 0): "CDF_CHAR", ("S", 0): "CDF_CHAR", ("M", 8): "CDF_TIME_TT2000"}
_RAW_TIMES = {DataType.CDF_TIME_TT2000: (np.int64, pycdfpp.tt2000_t), DataType.CDF_EPOCH: (np.float64, pycdfpp.epoch),
              DataType.CDF_EPOCH16: (np.complex128, lambda value: pycdfpp.epoch16(value.real, value.imag))}


def _encoded_dtype(data_type: DataType) -> np.dtype:
    """The dtype pycdfpp takes raw values of a CDF time type in (a numpy record pycdfpp registers)."""
    cdf = pycdfpp.CDF()
    return cdf.add_variable("probe", np.zeros(1, "datetime64[ns]"), data_type).values_encoded.dtype


@functools.cache
def _encoded_dtypes() -> dict:
    return {data_type: _encoded_dtype(data_type) for data_type in _RAW_TIMES}


class Variable(NamedTuple):
    name: str
    cdf_type: _types.CdfType
    num_elements: int
    rec_vary: bool
    is_z: bool
    dims: List[int]
    dim_vary: list


def _is_sequence(value) -> bool:
    return hasattr(value, "__len__") and not isinstance(value, (str, bytes))

def _text(value) -> str:
    """cdflib joins several strings of one attribute entry with NASA's "\\N " separator."""
    if value is None:
        return ""
    if _is_sequence(value):
        return "\\N ".join(_text(item) for item in value)
    return value.decode("utf-8") if isinstance(value, bytes) else str(value)


class _OutOfRange(ValueError):
    pass


def _integers_outside(values: np.ndarray, limits: np.iinfo) -> Optional[str]:
    low, high = int(values.min()), int(values.max())
    return f"integers from {low} to {high}" if low < limits.min or high > limits.max else None


def _floats_outside(values: np.ndarray, limits: np.iinfo) -> Optional[str]:
    """Truncated as C casts them; limits.max + 1 is exact as a float, limits.max may not be (int64)."""
    truncated = np.trunc(values)
    outside = ~np.isfinite(values) | (truncated < limits.min) | (truncated >= limits.max + 1)
    return f"{values[outside].flat[0]}" if outside.any() else None


_OUTSIDE = {"i": _integers_outside, "u": _integers_outside, "f": _floats_outside}


def _outside(values: np.ndarray, dtype: np.dtype) -> Optional[str]:
    """The numbers that don't fit an integer type, None when all do."""
    outside = _OUTSIDE.get(values.dtype.kind)
    if outside is None or dtype.kind not in "iu" or not values.size:
        return None
    return outside(values, np.iinfo(dtype))


def fits(value, cdf_type: _types.CdfType) -> bool:
    return _outside(np.asarray(value), cdf_type.dtype) is None


def _fitting(values: np.ndarray, dtype: np.dtype, type_name: str) -> np.ndarray:
    """Numbers cast to an integer type they fit. cdflib wraps the others silently (300 or 300.5 as CDF_INT1 is
    44, NaN is 0)."""
    dtype = np.dtype(dtype)
    problem = _outside(values, dtype)
    if problem:
        limits = np.iinfo(dtype)
        raise _OutOfRange(f"{type_name} holds {limits.min} to {limits.max}, not {problem}")
    return values.astype(dtype)


@contextmanager
def about(what: str):
    """Names what an out of range integer belongs to."""
    try:
        yield
    except _OutOfRange as error:
        raise ValueError(f"{what}: {error}") from None


def _numbers(values, cdf_type: _types.CdfType) -> np.ndarray:
    """values cast to the type as cdflib does (C casts), integers that fit only; numbers written as text are
    parsed first."""
    values = np.asarray(values)
    if values.dtype.kind in "US":
        values = values.astype(np.float64)
    return _fitting(values, cdf_type.dtype, cdf_type.name)


def _parsed_times(value):
    if isinstance(value, str) or (_is_sequence(value) and len(value) and isinstance(value[0], str)):
        return cdfepoch.CDFepoch.parse(value if isinstance(value, str) else list(value))
    return value


def _time_entry(value, data_type: DataType):
    """datetime64 values are converted by pycdfpp; numbers are raw CDF time values, kept as they are."""
    values = np.atleast_1d(_parsed_times(value))
    if values.dtype.kind == "M":
        return values
    raw_dtype, wrap = _RAW_TIMES[data_type]
    return [wrap(raw) for raw in _fitting(values, raw_dtype, data_type.name)]


def _entry_value(value, cdf_type: _types.CdfType):
    if _types.is_string(cdf_type):
        return _text(value)
    if cdf_type.data_type in _RAW_TIMES:
        return _time_entry(value, cdf_type.data_type)
    return np.atleast_1d(_numbers(value, cdf_type))


def _named_type(value):
    """cdflib's [data, 'CDF_TYPE'] form: the type is a name, a number would be data."""
    if _is_sequence(value) and len(value) == 2 and isinstance(value[1], str) and value[1].upper() in _types.by_name:
        return value[0], _types.by_name[value[1].upper()]
    return value, None


def _kind_and_size(dtype: np.dtype):
    return dtype.kind, 0 if dtype.kind in "US" else dtype.itemsize


def _dtype_of(value) -> np.dtype:
    """Python integers out of int64's range are uint64 (or more): numpy would make floats of a list of them."""
    items = value if _is_sequence(value) else [value]
    if all(type(item) is int for item in items):
        return np.dtype(np.int64 if all(-2**63 <= item < 2**63 for item in items) else np.uint64)
    return np.asarray(value).dtype


def _guessed_type(value) -> Optional[_types.CdfType]:
    if value is None or (_is_sequence(value) and len(value) == 0):
        return _types.by_name["CDF_CHAR"]
    sample = value[0] if _is_sequence(value) else value
    name = None if isinstance(sample, np.bool_) else _GUESSED_TYPES.get(_kind_and_size(_dtype_of(value)))
    if name is None:
        logger.warning(f"Invalid data type for data {sample!r}.... Skip")
        return None
    return _types.by_name[name]


def attribute_entry(value):
    """An attribute entry as pycdfpp takes it, (value, DataType), or None when its type is unknown."""
    data, cdf_type = _named_type(value)
    cdf_type = cdf_type or _guessed_type(data)
    if cdf_type is None:
        return None
    return _entry_value(data, cdf_type), cdf_type.data_type


def pad_value(pad, variable: Variable):
    if pad is None:
        return None
    pad = np.ravel(pad)[0] if _is_sequence(pad) else pad
    if _types.is_string(variable.cdf_type):
        return _text(pad)[:variable.num_elements].ljust(variable.num_elements, "\0")
    return _entry_value(pad, variable.cdf_type)


def _strings(data, num_elements: int) -> np.ndarray:
    if isinstance(data, bytes):
        return np.frombuffer(data, f"S{num_elements}")
    values = np.asarray(data)
    if values.dtype.kind != "S":
        values = np.char.encode(values.astype(str), "utf-8")
    if values.size and np.char.str_len(values).max() > num_elements:
        raise ValueError(f"A string is longer than Num_Elements ({num_elements})")
    return values.astype(f"S{num_elements}")


def _numeric_values(data, cdf_type: _types.CdfType) -> np.ndarray:
    if isinstance(data, bytes):
        return np.frombuffer(data, cdf_type.dtype)
    values = np.asarray(data)
    if cdf_type.data_type not in _RAW_TIMES:
        return _numbers(values, cdf_type)
    if values.dtype.kind == "M":
        return values
    return np.ascontiguousarray(_numbers(values, cdf_type)).view(_encoded_dtypes()[cdf_type.data_type])


def records(data, variable: Variable) -> np.ndarray:
    """var_data as records, shaped (records, *dims): cdflib reads any shape as a flat sequence of values."""
    if isinstance(data, dict):
        data = data["Data"]
    data = [] if data is None else data
    if _types.is_string(variable.cdf_type):
        values = _strings(data, variable.num_elements)
    else:
        values = _numeric_values(data, variable.cdf_type)
    values = values.reshape(-1, *variable.dims)
    return values if variable.rec_vary else values[:1]


def _with_virtual_records(records, values: np.ndarray):
    """cdflib's sparse data: values of the listed records only, or of every record up to the last."""
    records = np.asarray(records, dtype=np.int64)
    if len(records) == 0:
        return values[:0], None
    count = int(records.max()) + 1
    virtual = np.ones(count, dtype=bool)
    virtual[records] = False
    if len(values) != len(records):
        if len(values) < count:
            raise ValueError(f"Invalid sparse data: less data than the specified records ({len(values)} values "
                             f"for {len(records)} records up to record {count - 1})")
        return values[:count], virtual
    every_record = np.zeros((count, *values.shape[1:]), dtype=values.dtype)
    every_record[records] = values
    return every_record, virtual


def sparse_values(var_data, variable: Variable):
    if not isinstance(var_data, (list, tuple)) or len(var_data) != 2:
        logger.warning("Sparse record #s and data are not of list/tuple form:\n"
                       " [ [rec_#1, rec_#2, rec_#3,    ],\n   [data_#1, data_#2, data_#3, ....] ]")
        return records(None, variable), None
    return _with_virtual_records(var_data[0], records(var_data[1], variable))
