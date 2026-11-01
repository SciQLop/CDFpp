"""xarray.Dataset to CDF file: cdflib 1.3.16's xarray_to_cdf, its ISTP checks and type choices, on our cdfwrite.CDF."""
import functools
import os
from datetime import datetime
from typing import Any, Dict, List, NamedTuple, Optional

import numpy as np
import xarray as xr

import pycdfpp

from .. import _types, _values
from ..cdfwrite import CDF
from ..epochs import CDFepoch as cdfepoch
from ..logging import logger
from ._istp import DEPEND_ATTRIBUTE, ISTP_COMPLIANT_NAME, ISTPError  # noqa: F401 (public, as in cdflib)
from ._istp import Records as _Records
from ._istp import Report as _Report
from ._istp import is_epoch_name as _is_epoch_name
from ._istp import istp_records as _istp_records
from ._istp import names as _names
from ._istp import reporter as _reporter


def _fill_value(cdf_type: _types.CdfType):
    """ISTP's FILLVAL of a CDF type: the most negative value, the largest unsigned one, -1e31 for reals."""
    dtype = cdf_type.dtype
    if cdf_type.name == "CDF_TIME_TT2000":
        return np.datetime64(np.iinfo(np.int64).min, "ns")
    if dtype.kind == "S":
        return np.str_(" ")
    if dtype.kind in "iu":
        limits = np.iinfo(dtype)
        return dtype.type(limits.min if dtype.kind == "i" else limits.max)
    return dtype.type(complex(-1e31, -1e31) if dtype.kind == "c" else -1e31)


DATATYPES_TO_STRINGS = {t.number: t.name for t in _types.CDF_TYPES}
STRINGS_TO_DATATYPES = {t.name: t.number for t in _types.CDF_TYPES}
DATATYPE_FILLVALS = {t.number: _fill_value(t) for t in _types.CDF_TYPES}

_NUMPY_TO_CDF = {np.dtype(code): name for code, name in (
    ("i1", "CDF_INT1"), ("i2", "CDF_INT2"), ("i4", "CDF_INT4"), ("i8", "CDF_INT8"), ("u1", "CDF_UINT1"),
    ("u2", "CDF_UINT2"), ("u4", "CDF_UINT4"), ("f2", "CDF_FLOAT"), ("f4", "CDF_FLOAT"), ("f8", "CDF_DOUBLE"),
    ("c16", "CDF_EPOCH16"))}
_FROM_UNIX = {"CDF_EPOCH": cdfepoch.timestamp_to_cdfepoch, "CDF_EPOCH16": cdfepoch.timestamp_to_cdfepoch16}
# Raw fill values of the time types, for unix times: DATATYPE_FILLVALS holds NaT for TT2000.
_RAW_TIME_FILLS = {"CDF_EPOCH": -1e31, "CDF_EPOCH16": complex(-1e31, -1e31),
                   "CDF_TIME_TT2000": np.iinfo(np.int64).min}


class _Choice(NamedTuple):
    """A variable's CDF type, its number of elements, and why cdflib would warn about it (or None)."""
    cdf_type: _types.CdfType
    size: int
    problem: Optional[str] = None


class _Options(NamedTuple):
    compression: int
    skip: List[str]
    unix: bool
    istp: bool


def _holds_datetimes(value) -> bool:
    values = np.asarray(value)
    if values.dtype.kind == "M":
        return True
    return values.dtype == object and values.size > 0 and all(isinstance(v, datetime) for v in values.flat)


def _text_size(values) -> int:
    return max((len(str(v).encode("utf-8")) for v in np.ravel(values)), default=0)


def _is_time(cdf_type: _types.CdfType) -> bool:
    return cdf_type.name in _types.TIME_NAMES


def _choice(variable: xr.DataArray) -> _Choice:
    """The CDF type cdflib chooses: CDF_DATA_TYPE, else TT2000 for epoch names, else from the numpy dtype."""
    override = variable.attrs.get("CDF_DATA_TYPE")
    if override in STRINGS_TO_DATATYPES:
        return _Choice(_types.by_name[override], 1)
    if _is_epoch_name(variable.name):
        return _Choice(_types.by_name["CDF_TIME_TT2000"], 1)
    name, size, problem = _numpy_choice(variable)
    return _Choice(_types.by_name[name], size, problem)


def _uint64_choice(variable: xr.DataArray):
    """CDF has no 64 bit unsigned integers: CDF_INT8 holds them below 2**63. cdflib wraps them into CDF_UINT4."""
    values = variable.values
    if values.size and values.max() >= 2**63:
        raise ValueError(f"{variable.name}: values from 2**63 on fit no CDF type")
    return "CDF_INT8", 1, f"{variable.name}: CDF has no 64 bit unsigned integers, written as CDF_INT8"


def _numpy_choice(variable: xr.DataArray):
    dtype = variable.dtype.newbyteorder("=")
    if dtype in _NUMPY_TO_CDF:
        return _NUMPY_TO_CDF[dtype], 1, None
    if dtype == np.uint64:
        return _uint64_choice(variable)
    if dtype.kind == "S":
        return "CDF_CHAR", dtype.itemsize, None
    if dtype.kind == "U":
        return "CDF_CHAR", max(dtype.itemsize // 4, _text_size(variable.values)), None
    if dtype.kind == "M" or (dtype == object and _holds_datetimes(variable.values)):
        return "CDF_TIME_TT2000", 1, None
    if dtype == object:
        return "CDF_CHAR", _text_size(variable.values), None
    return "CDF_CHAR", 1, f"{variable.name}: values of type {dtype} have no CDF type"


def _fill_of(choices: Dict[Any, _Choice], name):
    return DATATYPE_FILLVALS[choices[name].cdf_type.number]


def _choices(dataset: xr.Dataset) -> Dict[Any, _Choice]:
    return {name: _choice(dataset[name]) for name in _names(dataset)}


def _log_problems(*choices: Dict[Any, _Choice]) -> None:
    """One warning per variable, whether its type was a problem before or after filling NaN."""
    problems = {name: c.problem for found in choices for name, c in found.items() if c.problem}
    for problem in problems.values():
        logger.warning(problem)


def _is_unix_time(variable: xr.DataArray, cdf_type: _types.CdfType, unix_time_to_cdf_time: bool) -> bool:
    return unix_time_to_cdf_time and not _holds_datetimes(variable.values) and (
        _is_epoch_name(variable.name) or _is_time(cdf_type))


def _nan_to_fill(value, fill):
    try:
        return np.nan_to_num(value, nan=fill)
    except Exception:  # anything nan_to_num can't take is kept as it is
        return value


def _fill_nans(dataset: xr.Dataset, choices: Dict[Any, _Choice], unix_time_to_cdf_time: bool) -> None:
    """NaN to FILLVAL, as cdflib. Unix times keep their NaN: the time conversion maps them to the fill value.
    Integers hold no NaN: left alone, filling would make floats of uint64."""
    for name in _names(dataset):
        cdf_type = choices[name].cdf_type
        fill, attrs = DATATYPE_FILLVALS[cdf_type.number], dataset[name].attrs
        unix_time = _is_unix_time(dataset[name], cdf_type, unix_time_to_cdf_time)
        time_attrs = attrs.get("TIME_ATTRS", ()) if unix_time else ()
        if np.asarray(fill).dtype.kind != "M" and not unix_time and dataset[name].dtype.kind not in "iu":
            try:
                dataset[name] = dataset[name].fillna(fill)
            except Exception:  # values fillna can't fill keep their NaNs
                pass
        dataset[name].attrs = {key: value if key in time_attrs else _nan_to_fill(value, fill)
                               for key, value in attrs.items()}


def _is_epoch_like(name, cdf_type: _types.CdfType, dims, dim_sizes: list, record_vary: bool) -> bool:
    """SPDF's test for an epoch variable (never compressed): a time type, named like an epoch or scalar per record."""
    is_own_coordinate = len(dims) == 1 and dims[0] == name
    return (_is_time(cdf_type) and name.lower() != "range_epoch"
            and (_is_epoch_name(name) or (not dim_sizes and record_vary) or is_own_coordinate))


def _time_values(values, cdf_type: _types.CdfType) -> np.ndarray:
    """datetimes for a time type go as datetime64 (the writer converts them, NaT to the fill value); any other type
    gets TT2000 nanoseconds, as in cdflib."""
    times = np.atleast_1d(np.asarray(values, dtype="datetime64[ns]"))
    if _is_time(cdf_type):
        return times
    return np.asarray(pycdfpp.to_tt2000(times.ravel())).view(np.int64).reshape(times.shape)


def _from_unix(values, cdf_type: _types.CdfType):
    """Unix times to the time type; NaN (missing) to its fill value. cdflib raises or writes junk for NaN."""
    times = np.asarray(values, dtype=np.float64)
    missing = np.isnan(times)
    converted = _FROM_UNIX.get(cdf_type.name, cdfepoch.timestamp_to_tt2000)(np.where(missing, 0.0, times))
    fill = _RAW_TIME_FILLS.get(cdf_type.name, DATATYPE_FILLVALS[cdf_type.number])
    return np.where(missing.reshape(np.shape(converted)), fill, converted)


def _variable_values(variable: xr.DataArray, cdf_type: _types.CdfType, unix_time_to_cdf_time: bool):
    values = variable.values
    if _holds_datetimes(values):
        return _time_values(values, cdf_type)
    if _is_unix_time(variable, cdf_type, unix_time_to_cdf_time):
        return _from_unix(values, cdf_type)
    return values


def _typed_attribute(key, value, attrs: dict, cdf_type: _types.CdfType, options: _Options):
    """An attribute value, given the variable's type where cdflib does. cdflib skips the VALIDMIN, VALIDMAX, FILLVAL
    and BIN_LOCATION types with unix_time_to_cdf_time: ISTP wants them anyway. None was reported by the ISTP check:
    an empty string spares the writer's warning."""
    if value is None and options.istp:
        return ""
    if _holds_datetimes(value):
        return [_time_values(value, cdf_type), cdf_type.name]
    if options.unix and _is_time(cdf_type) and key in attrs.get("TIME_ATTRS", ()):
        return [_from_unix(value, cdf_type), cdf_type.name]
    if options.istp and key.upper() == "BIN_LOCATION":
        return [np.float32(value), "CDF_REAL4"]
    if options.istp and key in ("VALIDMIN", "VALIDMAX", "FILLVAL") and _is_raw_value(value, cdf_type):
        return _istp_typed(key, value, cdf_type)
    return value


def _istp_typed(key, value, cdf_type: _types.CdfType):
    """value as the variable's type, when it fits. A FILLVAL that doesn't (a float -1e31 of an integer variable) is
    the type's own fill value; VALIDMIN and VALIDMAX keep their type. cdflib wraps them."""
    if _values.fits(value, cdf_type):
        return [value, cdf_type.name]
    return [DATATYPE_FILLVALS[cdf_type.number], cdf_type.name] if key == "FILLVAL" else value


def _is_raw_value(value, cdf_type: _types.CdfType) -> bool:
    """Whether value can be a raw value of the type: any value for other types; integers (as cdflib) or the type's
    own dtype for time types. cdflib types only integers, so a raw CDF_EPOCH FILLVAL (float64) became CDF_DOUBLE."""
    dtype = np.asarray(value).dtype
    return not _is_time(cdf_type) or dtype.kind in "iu" or dtype == cdf_type.dtype


def _check_depend_values(name, attrs: dict, report: _Report) -> None:
    for key, value in attrs.items():
        if DEPEND_ATTRIBUTE.match(key.upper()) and (not isinstance(value, str) or not value.strip()):
            report(f"{key} of {name} must name a variable, not {value!r}", Exception)


class _Variable(NamedTuple):
    spec: Dict[str, Any]
    attrs: Dict[str, Any]
    values: Any


def _layout(variable: xr.DataArray, name, records: _Records):
    """Rec_Vary and Dim_Sizes. A variable without values has zero records: CDF has no dimension of size 0."""
    record_vary = not variable.dims or name in records.varying or name in records.depend_0s or variable.size == 0
    dim_sizes = list(variable.shape[1:] if variable.dims and record_vary else variable.shape)
    if 0 in dim_sizes:
        raise ValueError(f"{name}: CDF has no dimension of size 0 (shape {variable.shape})")
    return record_vary, dim_sizes


def _variable(dataset: xr.Dataset, name, choice: _Choice, records: _Records, options: _Options) -> _Variable:
    variable = dataset[name]
    record_vary, dim_sizes = _layout(variable, name, records)
    skip = _is_epoch_like(name, choice.cdf_type, variable.dims, dim_sizes, record_vary) or name in options.skip
    spec = {"Variable": name, "Data_Type": choice.cdf_type.number, "Num_Elements": choice.size,
            "Rec_Vary": record_vary, "Dim_Sizes": dim_sizes, "Compress": 0 if skip else options.compression}
    attrs = {key: _typed_attribute(key, value, variable.attrs, choice.cdf_type, options)
             for key, value in variable.attrs.items()}
    return _Variable(spec, attrs, _variable_values(variable, choice.cdf_type, options.unix))


def _global_entries(value) -> dict:
    if hasattr(value, "__iter__") and not isinstance(value, str):
        return dict(enumerate(value))
    return {0: value}


def _write(dataset: xr.Dataset, file_name, variables: List[_Variable]) -> None:
    """Nothing is written when a variable fails: cdflib leaves a partial file."""
    with CDF(file_name) as writer:
        writer.write_globalattrs({name: _global_entries(value) for name, value in dataset.attrs.items()})
        for variable in variables:
            writer.write_var(variable.spec, var_attrs=variable.attrs, var_data=variable.values)


def xarray_to_cdf(
    xarray_dataset: xr.Dataset,
    file_name: str,
    unix_time_to_cdf_time: bool = False,
    istp: bool = True,
    terminate_on_warning: bool = False,
    auto_fix_depends: bool = True,
    record_dimensions: List[str] = ["record0"],
    compression: int = 0,
    compression_skip_vars: Optional[List[str]] = None,
    nan_to_fillval: bool = True,
) -> None:
    """
    Writes an xarray Dataset to a new CDF file, as cdflib.xarray.xarray_to_cdf does.

    Parameters
    ----------
    xarray_dataset : xarray.Dataset
        The Dataset to write. It is not modified.
    file_name : str
        Path of the new CDF file. An existing file is left alone: nothing is written.
    unix_time_to_cdf_time : bool, optional
        Values of time variables (named epoch or epoch_N, or with a CDF time type) are unix timestamps.
    istp : bool, optional
        Check the Dataset against the ISTP guidelines, add the DEPEND_n attributes it can guess and the FILLVAL of
        data, type VALIDMIN, VALIDMAX and FILLVAL as the variable.
    terminate_on_warning : bool, optional
        Raise on the first problem (ISTPError for ISTP rules, Exception otherwise) instead of logging a warning.
    auto_fix_depends : bool, optional
        Add the DEPEND_n attributes the ISTP check finds missing.
    record_dimensions : list of str, optional
        Variables (and dimensions) that vary along records, besides the ones the ISTP check finds.
    compression : int, optional
        GZIP level of the variables, 0 for none. Epoch variables are never compressed.
    compression_skip_vars : list of str, optional
        Variables to leave uncompressed.
    nan_to_fillval : bool, optional
        Replace NaN in values and attributes by the FILLVAL of the variable's type.

    The CDF type of a variable is its CDF_DATA_TYPE attribute (CDF_INT1, ..., CDF_TIME_TT2000), else
    CDF_TIME_TT2000 for variables named epoch or epoch_N, else the type of its numpy dtype; datetime64 values
    are CDF_TIME_TT2000 and strings CDF_CHAR.
    """
    report = _reporter(terminate_on_warning)
    if os.path.isfile(file_name):
        report(f"{file_name} exists: nothing written", Exception)
        return
    dataset = xarray_dataset.copy()
    choices = before = _choices(dataset)
    if nan_to_fillval:
        _fill_nans(dataset, before, unix_time_to_cdf_time)
        choices = _choices(dataset)
    _log_problems(before, choices)
    if istp:
        records = _istp_records(dataset, record_dimensions, auto_fix_depends, report,
                                functools.partial(_fill_of, choices))
        for name in _names(dataset):
            _check_depend_values(name, dataset[name].attrs, report)
    else:
        records = _Records(list(record_dimensions), list(record_dimensions))
    options = _Options(compression, compression_skip_vars or [], unix_time_to_cdf_time, istp)
    _write(dataset, file_name, [_variable(dataset, name, choices[name], records, options) for name in _names(dataset)])
