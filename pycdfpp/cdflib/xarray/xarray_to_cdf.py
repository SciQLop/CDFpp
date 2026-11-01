"""xarray.Dataset to CDF file: cdflib 1.3.16's xarray_to_cdf, its ISTP checks and type choices, on our cdfwrite.CDF."""
import os
import re
from datetime import datetime
from typing import Any, Callable, Dict, List, NamedTuple, Optional, Tuple

import numpy as np
import xarray as xr

import pycdfpp

from .. import _types
from ..cdfwrite import CDF
from ..epochs import CDFepoch as cdfepoch
from ..logging import logger


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
ISTP_COMPLIANT_NAME = re.compile(r"^[A-Za-z][A-Za-z0-9_]*$")
DEPEND_ATTRIBUTE = re.compile(r"^DEPEND_[0-9]+$")


class ISTPError(Exception):
    """
    Raised by xarray_to_cdf with terminate_on_warning=True when the Dataset breaks an ISTP rule.
    """

    def __init__(self, message: str = ""):
        super().__init__(message)


_TIME_TYPES = ("CDF_EPOCH", "CDF_EPOCH16", "CDF_TIME_TT2000")
_VAR_TYPES = ("data", "support_data", "metadata", "ignore_data")
_REQUIRED_GLOBAL_ATTRIBUTES = (
    "Project", "Source_name", "Discipline", "Data_type", "Descriptor", "Data_version", "Logical_file_id", "PI_name",
    "PI_affiliation", "TEXT", "Instrument_type", "Mission_group", "Logical_source", "Logical_source_description")
_EPOCH_NAME = re.compile(r"epoch(_[0-9]+)?$")
_DEPEND_KEY = re.compile(r"depend_[0-9]+$")
_LABELLED_DEPEND = re.compile(r"^DEPEND_([1-9])$")
# DISPLAY_TYPE: first DEPEND_n number that needs a LABL_PTR_n.
_FIRST_LABELLED_DEPEND = {"time_series": 1, "stack_plot": 1, "spectrogram": 2}
_NUMPY_TO_CDF = {np.dtype(code): name for code, name in (
    ("i1", "CDF_INT1"), ("i2", "CDF_INT2"), ("i4", "CDF_INT4"), ("i8", "CDF_INT8"), ("u1", "CDF_UINT1"),
    ("u2", "CDF_UINT2"), ("u4", "CDF_UINT4"), ("f2", "CDF_FLOAT"), ("f4", "CDF_FLOAT"), ("f8", "CDF_DOUBLE"),
    ("c16", "CDF_EPOCH16"))}
_FROM_UNIX = {"CDF_EPOCH": cdfepoch.timestamp_to_cdfepoch, "CDF_EPOCH16": cdfepoch.timestamp_to_cdfepoch16}

_Report = Callable[..., None]


class _Records(NamedTuple):
    depend_0s: List[Any]
    varying: List[Any]


def _reporter(terminate_on_warning: bool) -> _Report:
    """report(message, error): raises error with terminate_on_warning, else logs a warning. ISTP rules raise
    ISTPError, other problems Exception, as in cdflib."""
    def report(message: str, error: type = ISTPError) -> None:
        if terminate_on_warning:
            raise error(message)
        logger.warning(message)
    return report


def _names(dataset: xr.Dataset) -> list:
    return [*dataset.data_vars, *dataset.coords]


def _is_epoch_name(name) -> bool:
    return bool(_EPOCH_NAME.match(name.lower()))


def _has_depend_0(attrs: dict) -> bool:
    return any(key.lower() == "depend_0" for key in attrs)


def _holds_datetimes(value) -> bool:
    values = np.asarray(value)
    if values.dtype.kind == "M":
        return True
    return values.dtype == object and values.size > 0 and all(isinstance(v, datetime) for v in values.flat)


def _text_size(values) -> int:
    return max((len(str(v).encode("utf-8")) for v in np.ravel(values)), default=0)


def _cdf_type(variable: xr.DataArray) -> Tuple[_types.CdfType, int]:
    """The CDF type of a variable and its number of elements, as cdflib chooses them."""
    override = variable.attrs.get("CDF_DATA_TYPE")
    if override in STRINGS_TO_DATATYPES:
        return _types.by_name[override], 1
    if _is_epoch_name(variable.name):
        return _types.by_name["CDF_TIME_TT2000"], 1
    name, size = _numpy_cdf_type(variable)
    return _types.by_name[name], size


def _numpy_cdf_type(variable: xr.DataArray) -> Tuple[str, int]:
    dtype = variable.dtype
    if dtype in _NUMPY_TO_CDF:
        return _NUMPY_TO_CDF[dtype], 1
    if dtype == np.uint64:
        logger.warning(f"{variable.name}: CDF has no 64 bit unsigned integers, written as CDF_UINT4")
        return "CDF_UINT4", 1
    if dtype.kind == "S":
        return "CDF_CHAR", dtype.itemsize
    if dtype.kind == "U":
        return "CDF_CHAR", max(dtype.itemsize // 4, _text_size(variable.values))
    if dtype.kind == "M" or (dtype == object and _holds_datetimes(variable.values)):
        return "CDF_TIME_TT2000", 1
    if dtype == object:
        return "CDF_CHAR", _text_size(variable.values)
    logger.warning(f"{variable.name}: values of type {dtype} have no CDF type")
    return "CDF_CHAR", 1


def _fill_value_of(variable: xr.DataArray):
    return DATATYPE_FILLVALS[_cdf_type(variable)[0].number]


def _nan_to_fill(value, fill):
    try:
        return np.nan_to_num(value, nan=fill)
    except Exception:  # anything nan_to_num can't take is kept as it is
        return value


def _fill_nans(dataset: xr.Dataset) -> None:
    for name in _names(dataset):
        fill = _fill_value_of(dataset[name])
        attrs = dataset[name].attrs
        if np.asarray(fill).dtype.kind != "M":
            try:
                dataset[name] = dataset[name].fillna(fill)
            except Exception:  # values fillna can't fill keep their NaNs
                pass
        dataset[name].attrs = {key: _nan_to_fill(value, fill) for key, value in attrs.items()}


def _check_names(dataset: xr.Dataset, report: _Report) -> None:
    named = (("variable", dataset.variables), ("global attribute", dataset.attrs),
             ("variable attribute", [key for v in dataset.variables.values() for key in v.attrs]))
    for kind, names in named:
        for name in names:
            if not ISTP_COMPLIANT_NAME.match(str(name)):
                report(f"ISTP: {name!r} is not a valid {kind} name")


def _mark_label_variables(dataset: xr.Dataset, report: _Report) -> None:
    """Variables a LABL_PTR names are metadata, unless they say otherwise."""
    labels = set()
    for name in dataset.data_vars:
        for key, target in dataset[name].attrs.items():
            if not key.startswith("LABL_PTR"):
                continue
            if target in dataset.variables:
                labels.add(target)
            else:
                report(f"ISTP: {key} of {name} names {target}, which is not in the Dataset")
    for label in labels:
        dataset[label].attrs.setdefault("VAR_TYPE", "metadata")


def _checked_var_type(name, attrs: dict, report: _Report) -> str:
    if "VAR_TYPE" not in attrs:
        report(f"ISTP: {name} has no VAR_TYPE (data, support_data, metadata or ignore_data)")
        return ""
    if attrs["VAR_TYPE"].lower() not in _VAR_TYPES:
        report(f"ISTP: VAR_TYPE of {name} is {attrs['VAR_TYPE']}, not one of {', '.join(_VAR_TYPES)}")
        return ""
    return attrs["VAR_TYPE"].lower()


# Shape rules a DEPEND_n variable must follow, in cdflib's order: (rule broken, problem).
# p: shape of the variable, c: shape of its DEPEND_n, n: the DEPEND number, a: the variable's attributes.
_DEPEND_SHAPE_RULES = (
    (lambda p, c, n, a: len(p) != 0 and len(c) == 0, "their dimensions differ"),
    (lambda p, c, n, a: len(c) != 0 and len(p) == 0, "their dimensions differ"),
    (lambda p, c, n, a: len(c) > 2, "it has more than 2 dimensions"),
    (lambda p, c, n, a: len(c) == 2 and p[0] != c[0], "their records differ"),
    (lambda p, c, n, a: len(p) < n, "the variable has fewer dimensions"),
    (lambda p, c, n, a: len(p) < n + 1 and a.get("VAR_TYPE", None) == "data",
     "a data variable needs a dimension per DEPEND"),
    (lambda p, c, n, a: len(p) < n + 1 and "VAR_TYPE" in a and a["VAR_TYPE"] != "data" and _has_depend_0(a),
     "a variable with a DEPEND_0 needs a dimension per DEPEND"),
    (lambda p, c, n, a: _has_depend_0(a) and p[n] != c[-1], "their sizes differ"),
    (lambda p, c, n, a: not _has_depend_0(a) and p[n - 1] != c[-1], "their sizes differ"),
)


def _depend_fits(dataset: xr.Dataset, number: int, name, target, report: _Report) -> bool:
    """Whether target can be DEPEND_<number> of name; reports why not."""
    shape, target_shape, attrs = dataset[name].shape, dataset[target].shape, dataset[name].attrs
    try:
        problem = next((problem for broken, problem in _DEPEND_SHAPE_RULES
                        if broken(shape, target_shape, number, attrs)), None)
    except IndexError as error:
        report(f"{target} as DEPEND_{number} of {name} could not be checked: {error}", Exception)
        return False
    if problem is not None:
        report(f"ISTP: {target} can't be DEPEND_{number} of {name}: {problem}")
    return problem is None


def _depend_dimensions(dataset: xr.Dataset, report: _Report) -> set:
    """Names of the non-record dimensions: dimensions after the first, and valid DEPEND_n (n > 0) targets."""
    found = set()
    for name in _names(dataset):
        attrs = dataset[name].attrs
        var_type = _checked_var_type(name, attrs, report)
        for key, target in list(attrs.items()):
            if not _DEPEND_KEY.match(key.lower()) or key == "DEPEND_0":
                continue
            if target not in dataset.variables:
                report(f"ISTP: {key} of {name} names {target}, which is not in the Dataset")
            elif _depend_fits(dataset, int(key[-1]), name, target, report):
                found.add(target)
        for number, dim in enumerate(dataset[name].dims[1:], 1):
            found.add(dim)
            if dim not in dataset.variables and var_type == "data" and f"DEPEND_{number}" not in attrs:
                report(f"ISTP: dimension {dim} of {name} is no variable: give {name} a DEPEND_{number}")
    return found


def _depend_0_candidate(name, dims, attrs: dict):
    """What the records of a variable would be: its DEPEND_0, an Epoch, or the first dimension of (2D support) data."""
    if "DEPEND_0" in attrs:
        return attrs["DEPEND_0"]
    if _is_epoch_name(dims[0]):
        return dims[0]
    if _is_epoch_name(name):
        return name
    var_type = attrs["VAR_TYPE"].lower() if "VAR_TYPE" in attrs else ""
    if var_type == "data" or (var_type == "support_data" and len(dims) > 1):
        return dims[0]
    return ""


def _record_variables(dataset: xr.Dataset, dimensions: set, report: _Report) -> Tuple[List[Any], List[Any]]:
    """The record (DEPEND_0) variables, and the variables that vary along them."""
    depend_0s, varying = [], []
    for name in _names(dataset):
        dims, attrs = dataset[name].dims, dataset[name].attrs
        target = _depend_0_candidate(name, dims, attrs) if dims else ""
        if target in dimensions or target == "":
            continue
        if target in dataset.variables:
            if not _depend_fits(dataset, 0, name, target, report):
                report(f"ISTP: {name} varies along {target}, but their records differ")
                continue
        elif not _is_epoch_name(name):
            report(f"ISTP: {name} varies along {target}, which is not in the Dataset")
            continue
        depend_0s.append(target)
        varying.append(name)
    _check_epochs(dataset, set(depend_0s), report)
    return list(set(depend_0s)), varying


def _check_epochs(dataset: xr.Dataset, depend_0s: set, report: _Report) -> None:
    if not depend_0s:
        report("ISTP: no variable gives the records (no DEPEND_0, no Epoch)")
    increasing = [_is_increasing(dataset, name, report) for name in depend_0s if name.lower().startswith("epoch")]
    if not any(increasing):
        report("ISTP: no variable is named Epoch, the name ISTP requires for the records")


def _is_increasing(dataset: xr.Dataset, name, report: _Report) -> bool:
    values = dataset[name].data
    if np.all(values[1:] > values[:-1]):
        return True
    report(f"ISTP: {name} gives the records, but its values don't increase")
    return False


def _dimension_names(dataset: xr.Dataset, varying: list, dimensions: set) -> set:
    return dimensions | {dataset[name].dims[0] for name in _names(dataset)
                         if name not in varying and dataset[name].dims}


def _add_depend(attrs: dict, name, key: str, target, auto_fix_depends: bool) -> None:
    """These never stop the conversion, even with terminate_on_warning, as in cdflib."""
    logger.warning(f"ISTP: {name} needs {key}={target}")
    if auto_fix_depends:
        logger.warning(f"{key}={target} added to {name}")
        attrs[key] = target


def _add_missing_depends(dataset: xr.Dataset, dimensions: set, records: _Records, auto_fix_depends: bool) -> None:
    for name in _names(dataset):
        attrs, dims = dataset[name].attrs, dataset[name].dims
        others = dims
        if name in records.varying:
            others = dims[1:]
            if "DEPEND_0" not in attrs and dims[0] in records.depend_0s and name != dims[0]:
                _add_depend(attrs, name, "DEPEND_0", dims[0], auto_fix_depends)
        for number, dim in enumerate(others, 1):
            key = f"DEPEND_{number}"
            if dim in dataset.variables and dim in dimensions and key not in attrs and name != dim:
                _add_depend(attrs, name, key, dim, auto_fix_depends)


def _check_global_attributes(dataset: xr.Dataset, report: _Report) -> None:
    for name in _REQUIRED_GLOBAL_ATTRIBUTES:
        if name not in dataset.attrs:
            report(f"ISTP: global attribute {name} is missing")


def _check_labels(dataset: xr.Dataset, name, attrs: dict, report: _Report) -> None:
    display = attrs["DISPLAY_TYPE"].lower()
    first = _FIRST_LABELLED_DEPEND.get(display)
    for key in list(attrs):
        match = _LABELLED_DEPEND.match(key)
        if first is None or not match or int(match.group(1)) < first:
            continue
        label = f"LABL_PTR_{match.group(1)}"
        if label not in attrs:
            report(f"ISTP: {name} has a {key} and DISPLAY_TYPE={display}: it needs a {label}")
            continue
        if attrs[label] not in dataset.variables:
            report(f"ISTP: {label} of {name} names {attrs[label]}, which is not in the Dataset")
        if "LABLAXIS" in attrs:
            report(f"{name} has both LABLAXIS and {label}: keep one", Exception)


def _check_display(dataset: xr.Dataset, name, attrs: dict, report: _Report) -> None:
    if "DISPLAY_TYPE" not in attrs:
        report(f"ISTP: data variable {name} has no DISPLAY_TYPE")
    elif attrs["DISPLAY_TYPE"].lower() == "image":
        if "LABLAXIS" not in attrs:
            report(f"ISTP: data variable {name} with DISPLAY_TYPE=image needs a LABLAXIS")
    else:
        _check_labels(dataset, name, attrs, report)
    if "LABLAXIS" not in attrs and "LABL_PTR_1" not in attrs:
        report(f"ISTP: data variable {name} needs a LABLAXIS or a LABL_PTR_1")


def _check_pointer(dataset: xr.Dataset, name, attrs: dict, key: str, pointer: str, report: _Report) -> None:
    if key in attrs:
        return
    if pointer not in attrs:
        report(f"ISTP: {name} needs a {key} or a {pointer}")
    elif attrs[pointer] not in dataset.variables:
        report(f"ISTP: {pointer} of {name} names {attrs[pointer]}, which is not in the Dataset")


def _check_required(dataset: xr.Dataset, name, var_type: str, depend_0s: list, report: _Report) -> None:
    """VALIDMIN, VALIDMAX and FILLVAL of data and of record varying support_data. A missing FILLVAL is added."""
    variable = dataset[name]
    varies = bool(variable.dims) and variable.dims[0] in depend_0s
    if not (var_type == "data" or (var_type == "support_data" and varies)):
        return
    for key in ("VALIDMIN", "VALIDMAX", "FILLVAL"):
        if key not in variable.attrs:
            report(f"ISTP: {name} needs a {key}")
    if "FILLVAL" not in variable.attrs:
        variable.attrs["FILLVAL"] = fill = _fill_value_of(variable)
        report(f"ISTP: FILLVAL of {name} set to {fill}")


def _check_variable_attributes(dataset: xr.Dataset, depend_0s: list, report: _Report) -> None:
    for name in _names(dataset):
        attrs = dataset[name].attrs
        for key in [key for key, value in attrs.items() if value is None]:
            report(f"{key} of {name} is None: CDF has no None, it is written as an empty string", Exception)
        var_type = _checked_var_type(name, attrs, report)
        if "CATDESC" not in attrs:
            report(f"ISTP: {name} needs a CATDESC")
        if var_type == "data":
            _check_display(dataset, name, attrs, report)
        if "FIELDNAM" not in attrs:
            report(f"ISTP: {name} needs a FIELDNAM")
        _check_pointer(dataset, name, attrs, "FORMAT", "FORM_PTR", report)
        if var_type in ("data", "support_data"):
            _check_pointer(dataset, name, attrs, "UNITS", "UNIT_PTR", report)
        _check_required(dataset, name, var_type, depend_0s, report)


def _istp_records(dataset: xr.Dataset, record_dimensions: List[str], auto_fix_depends: bool,
                  report: _Report) -> _Records:
    """Checks the Dataset against ISTP, completes its attributes and finds which variables vary along records."""
    _check_names(dataset, report)
    _mark_label_variables(dataset, report)
    dimensions = _depend_dimensions(dataset, report)
    depend_0s, varying = _record_variables(dataset, dimensions, report)
    records = _Records([*record_dimensions, *depend_0s], [*record_dimensions, *varying])
    _add_missing_depends(dataset, _dimension_names(dataset, records.varying, dimensions), records, auto_fix_depends)
    _check_global_attributes(dataset, report)
    _check_variable_attributes(dataset, records.depend_0s, report)
    return records


def _is_time(cdf_type: _types.CdfType) -> bool:
    return cdf_type.name in _TIME_TYPES


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


def _from_unix(values, attrs: dict):
    return _FROM_UNIX.get(attrs.get("CDF_DATA_TYPE"), cdfepoch.timestamp_to_tt2000)(values)


def _variable_values(variable: xr.DataArray, cdf_type: _types.CdfType, unix_time_to_cdf_time: bool):
    values = variable.values
    if _holds_datetimes(values):
        return _time_values(values, cdf_type)
    if unix_time_to_cdf_time and (_is_epoch_name(variable.name) or _is_time(cdf_type)):
        return _from_unix(values, variable.attrs)
    return values


def _typed_attribute(key, value, attrs: dict, cdf_type: _types.CdfType, unix_time_to_cdf_time: bool, istp: bool):
    """An attribute value, given the variable's type where cdflib does. cdflib skips the VALIDMIN, VALIDMAX, FILLVAL
    and BIN_LOCATION types with unix_time_to_cdf_time: ISTP wants them anyway."""
    if _holds_datetimes(value):
        return [_time_values(value, cdf_type), cdf_type.name]
    if unix_time_to_cdf_time and _is_time(cdf_type) and key in attrs.get("TIME_ATTRS", ()):
        return [_from_unix(value, attrs), cdf_type.name]
    if istp and key.upper() == "BIN_LOCATION":
        return [np.float32(value), "CDF_REAL4"]
    if istp and key in ("VALIDMIN", "VALIDMAX", "FILLVAL") and (
            not _is_time(cdf_type) or np.issubdtype(np.asarray(value).dtype, np.integer)):
        return [value, cdf_type.name]
    return value


def _check_depend_values(name, attrs: dict, report: _Report) -> None:
    for key, value in attrs.items():
        if DEPEND_ATTRIBUTE.match(key.upper()) and (not isinstance(value, str) or not value.strip()):
            report(f"{key} of {name} must name a variable, not {value!r}", Exception)


class _Variable(NamedTuple):
    spec: Dict[str, Any]
    attrs: Dict[str, Any]
    values: Any


def _variable(dataset: xr.Dataset, name, records: _Records, options: dict) -> _Variable:
    variable = dataset[name]
    cdf_type, size = _cdf_type(variable)
    record_vary = not variable.dims or name in records.varying or name in records.depend_0s or variable.size == 0
    dim_sizes = list(variable.shape[1:] if variable.dims and record_vary else variable.shape)
    skip = _is_epoch_like(name, cdf_type, variable.dims, dim_sizes, record_vary) or name in options["skip"]
    spec = {"Variable": name, "Data_Type": cdf_type.number, "Num_Elements": size, "Rec_Vary": record_vary,
            "Dim_Sizes": dim_sizes, "Compress": 0 if skip else options["compression"]}
    attrs = {key: _typed_attribute(key, value, variable.attrs, cdf_type, options["unix"], options["istp"])
             for key, value in variable.attrs.items()}
    return _Variable(spec, attrs, _variable_values(variable, cdf_type, options["unix"]))


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
    if nan_to_fillval:
        _fill_nans(dataset)
    if istp:
        records = _istp_records(dataset, record_dimensions, auto_fix_depends, report)
    else:
        records = _Records(list(record_dimensions), list(record_dimensions))
    options = {"skip": compression_skip_vars or [], "compression": compression, "unix": unix_time_to_cdf_time,
               "istp": istp}
    if istp:
        for name in _names(dataset):
            _check_depend_values(name, dataset[name].attrs, report)
    _write(dataset, file_name, [_variable(dataset, name, records, options) for name in _names(dataset)])
