"""cdflib 1.3.16's ISTP checks of xarray_to_cdf: same checks, same order, same escalation (ISTPError or Exception)."""
import re
from typing import Any, Callable, List, NamedTuple, Tuple

import numpy as np
import xarray as xr

from ..logging import logger

ISTP_COMPLIANT_NAME = re.compile(r"^[A-Za-z]\w*$", re.ASCII)
DEPEND_ATTRIBUTE = re.compile(r"^DEPEND_\d+$", re.ASCII)
_VAR_TYPES = ("data", "support_data", "metadata", "ignore_data")
_REQUIRED_GLOBAL_ATTRIBUTES = (
    "Project", "Source_name", "Discipline", "Data_type", "Descriptor", "Data_version", "Logical_file_id", "PI_name",
    "PI_affiliation", "TEXT", "Instrument_type", "Mission_group", "Logical_source", "Logical_source_description")
_EPOCH_NAME = re.compile(r"epoch(_\d+)?$", re.ASCII)
_DEPEND_KEY = re.compile(r"depend_\d+$", re.ASCII)
_LABELLED_DEPEND = re.compile(r"^DEPEND_([1-9])$")
# DISPLAY_TYPE: first DEPEND_n number that needs a LABL_PTR_n.
_FIRST_LABELLED_DEPEND = {"time_series": 1, "stack_plot": 1, "spectrogram": 2}

Report = Callable[..., None]
FillValueOf = Callable[[Any], Any]


class ISTPError(Exception):
    """
    Raised by xarray_to_cdf with terminate_on_warning=True when the Dataset breaks an ISTP rule.
    """

    def __init__(self, message: str = ""):
        super().__init__(message)


class Records(NamedTuple):
    depend_0s: List[Any]
    varying: List[Any]


def reporter(terminate_on_warning: bool) -> Report:
    """report(message, error): raises error with terminate_on_warning, else logs a warning. ISTP rules raise
    ISTPError, other problems Exception, as in cdflib. The log record keeps the error type (cdflib_error)."""
    def report(message: str, error: type = ISTPError) -> None:
        if terminate_on_warning:
            raise error(message)
        logger.warning(message, extra={"cdflib_error": error.__name__})
    return report


def names(dataset: xr.Dataset) -> list:
    return [*dataset.data_vars, *dataset.coords]


def is_epoch_name(name) -> bool:
    return bool(_EPOCH_NAME.match(name.lower()))


def _has_depend_0(attrs: dict) -> bool:
    return any(key.lower() == "depend_0" for key in attrs)


def _check_names(dataset: xr.Dataset, report: Report) -> None:
    named = (("variable", dataset.variables), ("global attribute", dataset.attrs),
             ("variable attribute", [key for v in dataset.variables.values() for key in v.attrs]))
    for kind, names in named:
        for name in names:
            if not ISTP_COMPLIANT_NAME.match(str(name)):
                report(f"ISTP: {name!r} is not a valid {kind} name")


def _mark_label_variables(dataset: xr.Dataset, report: Report) -> None:
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


def _checked_var_type(name, attrs: dict, report: Report) -> str:
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


def _depend_fits(dataset: xr.Dataset, number: int, name, target, report: Report) -> bool:
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


def _depend_dimensions(dataset: xr.Dataset, report: Report) -> set:
    """Names of the non-record dimensions: dimensions after the first, and valid DEPEND_n (n > 0) targets."""
    found = set()
    for name in names(dataset):
        attrs = dataset[name].attrs
        var_type = _checked_var_type(name, attrs, report)
        found |= _depend_targets(dataset, name, report)
        found |= _check_dimensions_have_depend(dataset, name, var_type, report)
    return found


def _depend_targets(dataset: xr.Dataset, name, report: Report) -> set:
    """The valid targets of the DEPEND_n (n > 0) attributes of a variable."""
    targets = set()
    for key, target in dataset[name].attrs.items():
        if not _DEPEND_KEY.match(key.lower()) or key == "DEPEND_0":
            continue
        if target not in dataset.variables:
            report(f"ISTP: {key} of {name} names {target}, which is not in the Dataset")
        elif _depend_fits(dataset, int(key[-1]), name, target, report):
            targets.add(target)
    return targets


def _check_dimensions_have_depend(dataset: xr.Dataset, name, var_type: str, report: Report) -> set:
    """The non-record dimensions of a variable; data variables must give each a DEPEND_n."""
    attrs, dims = dataset[name].attrs, dataset[name].dims[1:]
    for number, dim in enumerate(dims, 1):
        if dim not in dataset.variables and var_type == "data" and f"DEPEND_{number}" not in attrs:
            report(f"ISTP: dimension {dim} of {name} is no variable: give {name} a DEPEND_{number}")
    return set(dims)


def _depend_0_candidate(name, dims, attrs: dict):
    """What the records of a variable would be: its DEPEND_0, an Epoch, or the first dimension of (2D support) data."""
    if "DEPEND_0" in attrs:
        return attrs["DEPEND_0"]
    if is_epoch_name(dims[0]):
        return dims[0]
    if is_epoch_name(name):
        return name
    var_type = attrs["VAR_TYPE"].lower() if "VAR_TYPE" in attrs else ""
    if var_type == "data" or (var_type == "support_data" and len(dims) > 1):
        return dims[0]
    return ""


def _record_variables(dataset: xr.Dataset, dimensions: set, report: Report) -> Tuple[List[Any], List[Any]]:
    """The record (DEPEND_0) variables, and the variables that vary along them."""
    depend_0s, varying = [], []
    for name in names(dataset):
        dims, attrs = dataset[name].dims, dataset[name].attrs
        target = _depend_0_candidate(name, dims, attrs) if dims else ""
        if target in dimensions or target == "":
            continue
        if target in dataset.variables:
            if not _depend_fits(dataset, 0, name, target, report):
                report(f"ISTP: {name} varies along {target}, but their records differ")
                continue
        elif not is_epoch_name(name):
            report(f"ISTP: {name} varies along {target}, which is not in the Dataset")
            continue
        depend_0s.append(target)
        varying.append(name)
    _check_epochs(dataset, set(depend_0s), report)
    return list(set(depend_0s)), varying


def _check_epochs(dataset: xr.Dataset, depend_0s: set, report: Report) -> None:
    if not depend_0s:
        report("ISTP: no variable gives the records (no DEPEND_0, no Epoch)")
    increasing = [_is_increasing(dataset, name, report) for name in depend_0s if name.lower().startswith("epoch")]
    if not any(increasing):
        report("ISTP: no variable is named Epoch, the name ISTP requires for the records")


def _is_increasing(dataset: xr.Dataset, name, report: Report) -> bool:
    values = dataset[name].data
    if np.all(values[1:] > values[:-1]):
        return True
    report(f"ISTP: {name} gives the records, but its values don't increase")
    return False


def _dimension_names(dataset: xr.Dataset, varying: list, dimensions: set) -> set:
    return {*dimensions, *(dataset[name].dims[0] for name in names(dataset)
                           if name not in varying and dataset[name].dims)}


def _add_depend(attrs: dict, name, key: str, target, auto_fix_depends: bool) -> None:
    """These never stop the conversion, even with terminate_on_warning, as in cdflib."""
    logger.warning(f"ISTP: {name} needs {key}={target}")
    if auto_fix_depends:
        logger.warning(f"{key}={target} added to {name}")
        attrs[key] = target


def _add_missing_depends(dataset: xr.Dataset, dimensions: set, records: Records, auto_fix_depends: bool) -> None:
    for name in names(dataset):
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


def _check_global_attributes(dataset: xr.Dataset, report: Report) -> None:
    for name in _REQUIRED_GLOBAL_ATTRIBUTES:
        if name not in dataset.attrs:
            report(f"ISTP: global attribute {name} is missing")


def _check_labels(dataset: xr.Dataset, name, attrs: dict, report: Report) -> None:
    display = attrs["DISPLAY_TYPE"].lower()
    first = _FIRST_LABELLED_DEPEND.get(display)
    for key in attrs:
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


def _check_display(dataset: xr.Dataset, name, attrs: dict, report: Report) -> None:
    if "DISPLAY_TYPE" not in attrs:
        report(f"ISTP: data variable {name} has no DISPLAY_TYPE")
    elif attrs["DISPLAY_TYPE"].lower() == "image":
        if "LABLAXIS" not in attrs:
            report(f"ISTP: data variable {name} with DISPLAY_TYPE=image needs a LABLAXIS")
    else:
        _check_labels(dataset, name, attrs, report)
    if "LABLAXIS" not in attrs and "LABL_PTR_1" not in attrs:
        report(f"ISTP: data variable {name} needs a LABLAXIS or a LABL_PTR_1")


def _check_pointer(dataset: xr.Dataset, name, attrs: dict, key: str, pointer: str, report: Report) -> None:
    if key in attrs:
        return
    if pointer not in attrs:
        report(f"ISTP: {name} needs a {key} or a {pointer}")
    elif attrs[pointer] not in dataset.variables:
        report(f"ISTP: {pointer} of {name} names {attrs[pointer]}, which is not in the Dataset")


def _check_required(dataset: xr.Dataset, name, var_type: str, depend_0s: list, report: Report,
                    fill_value_of: FillValueOf) -> None:
    """VALIDMIN, VALIDMAX and FILLVAL of data and of record varying support_data. A missing FILLVAL is added."""
    variable = dataset[name]
    varies = bool(variable.dims) and variable.dims[0] in depend_0s
    if not (var_type == "data" or (var_type == "support_data" and varies)):
        return
    for key in ("VALIDMIN", "VALIDMAX", "FILLVAL"):
        if key not in variable.attrs:
            report(f"ISTP: {name} needs a {key}")
    if "FILLVAL" not in variable.attrs:
        variable.attrs["FILLVAL"] = fill = fill_value_of(name)
        report(f"ISTP: FILLVAL of {name} set to {fill}")


def _check_variable_attributes(dataset: xr.Dataset, depend_0s: list, report: Report,
                               fill_value_of: FillValueOf) -> None:
    for name in names(dataset):
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
        _check_required(dataset, name, var_type, depend_0s, report, fill_value_of)


def istp_records(dataset: xr.Dataset, record_dimensions: List[str], auto_fix_depends: bool, report: Report,
                 fill_value_of: FillValueOf) -> Records:
    """Checks the Dataset against ISTP, completes its attributes and finds which variables vary along records.
    fill_value_of(name): the FILLVAL added to a variable that lacks one."""
    _check_names(dataset, report)
    _mark_label_variables(dataset, report)
    dimensions = _depend_dimensions(dataset, report)
    depend_0s, varying = _record_variables(dataset, dimensions, report)
    records = Records([*record_dimensions, *depend_0s], [*record_dimensions, *varying])
    _add_missing_depends(dataset, _dimension_names(dataset, records.varying, dimensions), records, auto_fix_depends)
    _check_global_attributes(dataset, report)
    _check_variable_attributes(dataset, records.depend_0s, report, fill_value_of)
    return records
