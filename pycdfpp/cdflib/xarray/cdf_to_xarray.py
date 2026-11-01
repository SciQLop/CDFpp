"""CDF file to xarray.Dataset: the Dataset cdflib 1.3.16's cdf_to_xarray builds, read through pycdfpp."""
import functools
import itertools
import re
from typing import Any, Dict, List, NamedTuple, Optional, Tuple

import numpy as np
import xarray as xr

from ..cdfread import CDF
from ..dataclasses import AttData, VDRInfo
from ..epochs import CDFepoch as cdfepoch
from ..logging import logger

ISTP_TO_XARRAY_ATTRS = {"FIELDNAM": "standard_name", "LABLAXIS": "long_name", "UNITS": "units"}

_TIME_TYPES = ("CDF_EPOCH", "CDF_EPOCH16", "CDF_TIME_TT2000")
_FILLABLE_TYPES = ("CDF_FLOAT", "CDF_REAL4", "CDF_DOUBLE", "CDF_REAL8") + _TIME_TYPES
_TIME_CONVERSIONS = {"datetime": (cdfepoch.to_datetime, "Datetime (UTC)"), "unixtime": (cdfepoch.unixtime, "seconds")}
_DEPEND_KEY = re.compile("depend_[0-9]+$")
_UNCERTAINTY_KEYS = ("DELTA_PLUS_VAR", "DELTA_MINUS_VAR")


class _Variable(NamedTuple):
    name: str
    values: np.ndarray
    attrs: Dict[str, Any]
    props: VDRInfo

    @property
    def type_name(self) -> str:
        return self.props.Data_Type_Description


class _Axis(NamedTuple):
    name: str
    size: int
    is_depend: bool


def _time_mode(to_datetime: bool, to_unixtime: bool) -> Optional[str]:
    if to_unixtime:
        return "unixtime"
    return "datetime" if to_datetime else None


def _epoch16_as_epoch(values: np.ndarray):
    """Every record, down to the millisecond. cdflib slices the first 7 rows of the breakdown instead of its first 7
    columns: arrays keep their EPOCH16 values and only their first 7 records."""
    return cdfepoch.compute(np.asarray(cdfepoch.breakdown(values))[..., :7])


def _needs_conversion(type_name: str, mode: Optional[str]) -> bool:
    return type_name in _TIME_TYPES and (mode is not None or type_name == "CDF_EPOCH16")


def _converted_time(values: np.ndarray, mode: Optional[str]):
    return _TIME_CONVERSIONS[mode][0](values) if mode else _epoch16_as_epoch(values)


def _converted_values(raw, type_name: str, mode: Optional[str]) -> np.ndarray:
    values = np.atleast_1d(raw)
    if len(values) == 0 or type_name not in _TIME_TYPES:
        return values
    values = np.atleast_1d(np.squeeze(values))
    return np.atleast_1d(_converted_time(values, mode)) if _needs_conversion(type_name, mode) else values


def _converted_attribute(attribute: AttData, mode: Optional[str]):
    if not _needs_conversion(attribute.Data_Type, mode):
        return attribute.Data
    return _converted_time(np.array(attribute.Data), mode)


def _cdf_data_type(type_name: str, mode: Optional[str]) -> Optional[str]:
    """The CDF_DATA_TYPE attribute telling xarray_to_cdf the values' type, None to leave the file's attributes.
    EPOCH16 values read raw are CDF_EPOCH milliseconds: written back as EPOCH16 they would be wrong times."""
    if type_name == "CDF_EPOCH16" and mode is None:
        return "CDF_EPOCH"
    if (type_name in ("CDF_EPOCH", "CDF_REAL4", "CDF_REAL8", "CDF_EPOCH16")
            or (type_name == "CDF_TIME_TT2000" and mode == "unixtime")):
        return type_name
    return None


def _variable_attrs(attributes: Dict[str, AttData], type_name: str, has_values: bool, mode: Optional[str]):
    attrs = {name: _converted_attribute(attribute, mode) for name, attribute in attributes.items()}
    if mode and has_values and type_name in _TIME_TYPES and "UNITS" in attrs:
        attrs["UNITS"] = _TIME_CONVERSIONS[mode][1]
    data_type = _cdf_data_type(type_name, mode)
    if data_type:
        attrs["CDF_DATA_TYPE"] = data_type
    converted = [name for name, attribute in attributes.items() if mode and attribute.Data_Type in _TIME_TYPES]
    if converted:
        attrs["TIME_ATTRS"] = converted
    return attrs


def _read_variable(cdf: CDF, name: str, mode: Optional[str]) -> _Variable:
    props = cdf.varinq(name)
    attributes = {attribute: cdf.attget(attribute, name) for attribute in cdf.varattsget(name)}
    raw = np.array([]) if props.Last_Rec < 0 else cdf.varget(name)
    type_name = props.Data_Type_Description
    has_values = len(np.atleast_1d(raw)) > 0
    return _Variable(name, _converted_values(raw, type_name, mode), _variable_attrs(attributes, type_name, has_values, mode),
                     props)


def _global_attributes(cdf: CDF) -> Dict[str, Any]:
    try:
        return cdf.globalattsget()
    except Exception:
        return {}


def _read_file(filename, mode: Optional[str]) -> Tuple[Dict[str, Any], Dict[str, _Variable]]:
    with CDF(filename, string_encoding="latin-1") as cdf:
        info = cdf.cdf_info()
        variables = {name: _read_variable(cdf, name, mode) for name in info.rVariables + info.zVariables}
        return _global_attributes(cdf), variables


def _names_variable(value, variables: Dict[str, _Variable]) -> bool:
    return isinstance(value, str) and value in variables


def _depend_mismatch(primary: _Variable, coordinate: _Variable, number: int) -> Optional[str]:
    """Why coordinate can't be primary's DEPEND_<number>, None when it can."""
    shape, coordinate_shape = primary.values.shape, coordinate.values.shape
    if (len(shape) == 0) != (len(coordinate_shape) == 0):
        return "the dimensions do not match"
    if len(coordinate_shape) > 2:
        return "it has too many dimensions"
    if len(coordinate_shape) == 2 and shape[0] != coordinate_shape[0]:
        return "the Epoch dimensions do not match"
    has_records = primary.props.Rec_Vary and primary.props.Last_Rec > 0
    axis = number if has_records else number - 1
    if len(shape) <= axis:
        return f"{primary.name} does not have that many dimensions"
    # Without records, DEPEND_n can name axis n-1 or axis n: only both mismatching rejects it.
    next_axis_mismatch = axis == number or (len(shape) > number and shape[number] != coordinate_shape[-1])
    if shape[axis] != coordinate_shape[-1] and next_axis_mismatch:
        return "the dimensions do not match"
    return None


def _fits_as_depend(primary: _Variable, coordinate: _Variable, key: str) -> bool:
    # cdflib reads the DEPEND number from the key's last character only.
    reason = _depend_mismatch(primary, coordinate, int(key[-1]))
    if reason:
        logger.warning(f"ISTP Compliance Warning: {coordinate.name} is listed as the {key} for variable {primary.name},"
                       f" but {reason}.")
    return reason is None


def _depend_variables(variables: Dict[str, _Variable]) -> set:
    return {target
            for variable in variables.values()
            for key, target in variable.attrs.items()
            if _DEPEND_KEY.match(key.lower()) and _names_variable(target, variables)
            and _fits_as_depend(variable, variables[target], key)}


def _matching_depend_0(variable: _Variable, variables: Dict[str, _Variable], needs_records: bool) -> Optional[str]:
    if "DEPEND_0" not in variable.attrs:
        return None
    target = variable.attrs["DEPEND_0"]
    if not _names_variable(target, variables):
        logger.warning(f"Warning: Variable {variable.name} listed DEPEND_0 as {target}, but no variable by that name was found.")
        return None
    length = len(variable.values)
    if len(variables[target].values) == length and (length or not needs_records):
        return target
    logger.warning(f"Warning: Variable {variable.name} listed DEPEND_0 as {target}, but they have different dimension lengths.")
    return None


def _is_its_own_record_axis(props: VDRInfo) -> bool:
    return not (props.Dim_Sizes and props.Last_Rec > 0) or not any(props.Dim_Vary[:len(props.Dim_Sizes)])


def _record_axis(variable: _Variable, depends: set, variables: Dict[str, _Variable],
                 unlimited: Dict[str, int]) -> Tuple[Optional[str], bool, bool]:
    """(name, is a DEPEND variable, is a new unnamed record dimension) of the record axis, name None without one."""
    if not (variable.props.Rec_Vary and variable.props.Last_Rec >= 0):
        depend_0 = _matching_depend_0(variable, variables, needs_records=True)
        return depend_0, depend_0 is not None, False
    if variable.name in depends and _is_its_own_record_axis(variable.props):
        return variable.name, True, False
    depend_0 = _matching_depend_0(variable, variables, needs_records=False)
    if depend_0:
        return depend_0, True, False
    length = len(variable.values)
    existing = next((name for name, size in unlimited.items() if size == length), None)
    if existing:
        return existing, False, False
    return f"record{len(unlimited)}", False, True


def _depend_dimension(target: _Variable, values: np.ndarray, axis: int) -> Optional[str]:
    target_values = target.values
    if target_values.size == 0 or values.ndim <= axis:
        return None
    if target_values.ndim == 1 and target_values.shape[0] == values.shape[axis]:
        return target.name
    if target_values.ndim > 1 and target_values.shape[1] == values.shape[axis]:
        return target.name + "_dim"
    return None


def _named_by_depend(variable: _Variable, number: int, variables: Dict[str, _Variable], has_record: bool) -> Optional[str]:
    key = f"DEPEND_{number}"
    if key not in variable.attrs:
        return None
    target = variable.attrs[key]
    if not _names_variable(target, variables):
        logger.warning(f"Warning: Variable {variable.name} listed {key} as {target}, but no variable by that name was found.")
        return None
    name = _depend_dimension(variables[target], variable.values, number if has_record else number - 1)
    if name is None:
        logger.warning(f"Warning: Variable {variable.name} listed {key} as {target}, but that variable's dimensions do"
                       f" not match {variable.name}'s dimensions.")
    return name


def _named_by_shifted_depend(variable: _Variable, number: int, variables: Dict[str, _Variable]) -> Optional[str]:
    """Files without a record axis whose DEPEND_<n> describes axis n-1: cdflib still uses them."""
    target = variable.attrs.get(f"DEPEND_{number - 1}")
    if not _names_variable(target, variables):
        return None
    name = _depend_dimension(variables[target], variable.values, number - 1)
    if name:
        logger.warning(f"Warning: Variable {variable.name} has no determined time-varying component, but {target} was"
                       f" determined to match closely with one of the dimensions. It will be set automatically for"
                       f" convenience.")
    return name


def _named_after_itself(variable: _Variable, number: int, size: int, previous: List[_Axis],
                        has_record: bool) -> Optional[str]:
    props = variable.props
    if variable.values.ndim == 2 and props.Last_Rec == 0:
        return None if number == 1 and not has_record else variable.name + "_dim"
    if props.Rec_Vary and props.Last_Rec != 0:
        return functools.reduce(lambda name, axis: f"{variable.name}_dim{number}" if axis.name == name and axis.size != size
                                else name, previous, variable.name + "_dim")
    return variable.name


def _named_axis(variable: _Variable, number: int, size: int, depends: set, variables: Dict[str, _Variable],
                has_record: bool, previous: List[_Axis]) -> Optional[str]:
    return (_named_by_depend(variable, number, variables, has_record)
            or (None if has_record else _named_by_shifted_depend(variable, number, variables))
            or (_named_after_itself(variable, number, size, previous, has_record) if variable.name in depends else None))


def _unnamed_axis(size: int, regular: Dict[str, int]) -> _Axis:
    existing = next((name for name, known_size in regular.items() if known_size == size), None)
    if existing:
        return _Axis(existing, size, True)
    name = f"dim{len(regular)}"
    regular[name] = size
    return _Axis(name, size, False)


def _other_axes(variable: _Variable, depends: set, variables: Dict[str, _Variable], regular: Dict[str, int],
                has_record: bool) -> List[_Axis]:
    if not variable.props.Dim_Sizes or variable.props.Last_Rec < 0:
        return []
    sizes = variable.values.shape[1:] if has_record else variable.values.shape
    axes: List[_Axis] = []
    for number, size in enumerate(sizes, start=1):
        name = _named_axis(variable, number, size, depends, variables, has_record, axes)
        axes.append(_Axis(name, size, True) if name else _unnamed_axis(size, regular))
    return axes


def _fit_values_to_dims(dims: List[str], values: np.ndarray):
    """cdflib's dims can disagree with the values' shape: squeeze or add one axis, as it does."""
    if values.ndim > len(dims):
        values = np.squeeze(values)
    if values.ndim < len(dims):
        values = np.expand_dims(values, axis=0)
    return ("dim_empty" if values.size == 0 and not dims else dims), values


def _plotting_attrs(attrs: Dict[str, Any]) -> Dict[str, Any]:
    return {ISTP_TO_XARRAY_ATTRS[key]: value for key, value in attrs.items() if key in ISTP_TO_XARRAY_ATTRS}


def _fill_to_nan(values: np.ndarray, attrs: Dict[str, Any], type_name: str) -> np.ndarray:
    if "FILLVAL" not in attrs or type_name not in _FILLABLE_TYPES:
        return values
    missing = values == attrs["FILLVAL"]
    if not np.any(missing):
        return values
    filled = values.copy()
    filled[missing] = np.datetime64("NaT") if values.dtype.kind == "M" else np.nan
    return filled


def _xarray_variable(variable: _Variable, dims: List[str], fillval_to_nan: bool) -> xr.Variable:
    dims, values = _fit_values_to_dims(dims, variable.values)
    attrs = {**variable.attrs, **_plotting_attrs(variable.attrs)}
    if fillval_to_nan:
        values = _fill_to_nan(values, attrs, variable.type_name)
    return xr.Variable(dims, values, attrs=attrs)


def _xarray_variables(variables: Dict[str, _Variable], fillval_to_nan: bool) -> Tuple[Dict[str, xr.Variable], Dict[str, int]]:
    """Every variable as an xarray.Variable, and the dimensions DEPEND variables span."""
    depends = _depend_variables(variables)
    unlimited: Dict[str, int] = {}
    regular: Dict[str, int] = {}
    depend_dims: Dict[str, int] = {}
    created: Dict[str, xr.Variable] = {}
    for variable in variables.values():
        record, is_depend, is_new = _record_axis(variable, depends, variables, unlimited)
        if is_depend:
            depend_dims[record] = len(variable.values)
        if is_new:
            unlimited[record] = len(variable.values)
        axes = _other_axes(variable, depends, variables, regular, bool(record))
        depend_dims.update({axis.name: axis.size for axis in axes if axis.is_depend})
        dims = ([record] if record else []) + [axis.name for axis in axes]
        created[variable.name] = _xarray_variable(variable, dims, fillval_to_nan)
    return created, depend_dims


def _label_target(variable: _Variable, key: str, variables: Dict[str, _Variable]) -> Optional[str]:
    """The DEPEND variable the LABL_PTR_<n> key of variable labels, when the label fits it."""
    target = variable.attrs.get("DEPEND_" + key[-1])
    if not _names_variable(target, variables):
        return None
    target_dims, dims = variables[target].props.Dim_Sizes, variable.props.Dim_Sizes
    index = int(key[-1]) - 1
    fits = target_dims and dims and index < len(dims) and target_dims[0] == dims[index]
    return target if fits and variables[target].values.size != 0 else None


def _label_variables(variables: Dict[str, _Variable]) -> Dict[str, str]:
    """{label variable: the DEPEND variable it labels}."""
    labels: Dict[str, str] = {}
    for variable in variables.values():
        for key in (key for key in variable.attrs if key.startswith("LABL_PTR_")):
            target, label = _label_target(variable, key, variables), variable.attrs[key]
            if target and _names_variable(label, variables):
                labels[label] = target
            elif target:
                logger.warning(f"Warning, variable {variable.name} points to {label} as label {key}, but {label} does not exist.")
    return labels


def _uncertainty_variables(variables: Dict[str, _Variable]) -> Dict[str, str]:
    """{uncertainty variable: the variable it is the uncertainty of}."""
    return {variable.attrs[key]: variable.name
            for variable in variables.values() for key in _UNCERTAINTY_KEYS
            if isinstance(variable.attrs.get(key), str)}


def _attach_label(label: xr.Variable, coordinate: xr.Variable, label_name: str, coordinate_name: str) -> None:
    same_rank = len(label.dims) == len(coordinate.dims)
    fits = label.size == coordinate.size if same_rank else label.shape[0] == coordinate.shape[-1]
    if not fits:
        logger.warning(f"Warning, label variable {label_name} does not match the expected dimension sizes of {coordinate_name}")
    elif same_rank:
        label.dims = coordinate.dims
    else:
        label.dims = (coordinate.dims[-1],)


def _split_data_and_coords(created: Dict[str, xr.Variable], depend_dims: Dict[str, int], labels: Dict[str, str],
                           uncertainties: Dict[str, str]) -> Tuple[Dict[str, xr.Variable], Dict[str, xr.Variable]]:
    """DEPEND variables and their labels are coordinates; labels of no coordinate are dropped, as in cdflib."""
    data_vars: Dict[str, xr.Variable] = {}
    coords: Dict[str, xr.Variable] = {}
    for name, variable in created.items():
        if name in labels:
            continue
        if name in depend_dims or name + "_dim" in depend_dims:
            coords[name] = variable
            for label in (label for label, target in labels.items() if target == name):
                _attach_label(created[label], variable, label, name)
                coords[label] = created[label]
        else:
            if name in uncertainties and variable.size == created[uncertainties[name]].size:
                variable.dims = created[uncertainties[name]].dims
            data_vars[name] = variable
    return data_vars, coords


def _check_dimension_variables(data_vars: Dict[str, xr.Variable], coords: Dict[str, xr.Variable]) -> None:
    pools = (data_vars, coords)
    for name, variable in itertools.chain(data_vars.items(), coords.items()):
        for dim in variable.dims:
            wrong = next((pool[dim] for pool in pools if dim in pool and pool[dim].dims != (dim,)), None)
            if wrong is not None:
                raise Exception(f'Variable "{name}" has the dimension "{dim}", a variable whose dimensions are {wrong.dims}'
                                f' instead of ("{dim}",). Check the DEPEND_X attributes of {name} and {dim}.')


def cdf_to_xarray(filename: str, to_datetime: bool = True, to_unixtime: bool = False, fillval_to_nan: bool = False) -> xr.Dataset:
    """Converts a CDF file into an xarray Dataset, as cdflib.xarray.cdf_to_xarray does.

    Parameters
    ----------
    filename : str
        The path to the CDF file to read
    to_datetime : bool, optional
        Convert CDF_EPOCH, CDF_EPOCH16 and CDF_TIME_TT2000 values to datetime64
    to_unixtime : bool, optional
        Convert them to seconds since 1970 instead; wins over to_datetime
    fillval_to_nan : bool, optional
        Set the values equal to a variable's FILLVAL to NaN (NaT for times)

    Returns
    -------
    xarray.Dataset
        Every variable and attribute of the file. DEPEND_X variables, and their LABL_PTR_X labels, are coordinates.
    """
    global_attrs, variables = _read_file(filename, _time_mode(to_datetime, to_unixtime))
    created, depend_dims = _xarray_variables(variables, fillval_to_nan)
    data_vars, coords = _split_data_and_coords(created, depend_dims, _label_variables(variables),
                                               _uncertainty_variables(variables))
    _check_dimension_variables(data_vars, coords)
    return xr.Dataset(data_vars=data_vars, coords=coords, attrs=global_attrs)
