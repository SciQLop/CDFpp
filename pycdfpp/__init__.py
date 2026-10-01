"""
pycdfpp
-------
.. currentmodule:: pycdfpp

.. toctree::
    :maxdepth: 3

Indices and tables
------------------
* :ref:`genindex`
* :ref:`modindex`
* :ref:`search`
"""

from typing import Mapping, List, Any, Union, overload, Callable
import sys
import os
import copy
from functools import singledispatch, wraps
from datetime import datetime
import re
import errno
import warnings

import numpy as np

from ._pycdfpp import DataType, CompressionType, Majority, SparseRecords, Variable, VariableAttribute, \
    Attribute, CDF, tt2000_t, epoch, epoch16
from . import _pycdfpp

# ByteString is deprecated in Python 3.9+ and removed in Python 3.14
ByteString = Union[bytes, bytearray, memoryview]

__version__ = _pycdfpp.__version__
__here__ = os.path.dirname(os.path.abspath(__file__))
sys.path.append(__here__)
if sys.platform == 'win32' and sys.version_info[0] == 3 and sys.version_info[1] >= 8:
    os.add_dll_directory(__here__)

__all__ = ['load', 'save', 'CDF', 'Variable', 'Attribute', 'VariableAttribute', 'filter_cdf',
           'to_datetime64', 'to_datetime', 'to_time_string', 'to_tt2000', 'to_epoch', 'to_epoch16',
           'tt2000_t', 'epoch', 'epoch16', 'default_fill_value', 'default_pad_value', 'to_dict_skeleton',
           'DataType', 'CompressionType', 'Majority', 'SparseRecords', 'ExperimentalCompressionWarning']


def __dir__():
    # PEP 562: __all__ alone doesn't hide imports (np, os, ...) from dir() and tab completion
    return sorted(__all__ + ['__version__'])

# Build dtype.num → CDF type mapping dynamically to handle platform differences.
# On Windows, np.int64 is NPY_LONGLONG (num=9) while on Linux it's NPY_LONG (num=7).
_NUMPY_TO_CDF_TYPE_ = {np.dtype(dtype).num: data_type for dtype, data_type in (
    (np.int8, DataType.CDF_INT1), (np.uint8, DataType.CDF_UINT1), (np.int16, DataType.CDF_INT2),
    (np.uint16, DataType.CDF_UINT2), (np.int32, DataType.CDF_INT4), (np.uint32, DataType.CDF_UINT4),
    (np.int64, DataType.CDF_INT8), (np.float32, DataType.CDF_FLOAT), (np.float64, DataType.CDF_DOUBLE),
    (np.bytes_, DataType.CDF_CHAR))}

_CDF_TYPES_TO_NUMPY_DTYPE_ = {
    DataType.CDF_NONE: None,
    DataType.CDF_BYTE: np.int8,
    DataType.CDF_INT1: np.int8,
    DataType.CDF_UINT1: np.uint8,
    DataType.CDF_INT2: np.int16,
    DataType.CDF_UINT2: np.uint16,
    DataType.CDF_INT4: np.int32,
    DataType.CDF_UINT4: np.uint32,
    DataType.CDF_INT8: np.int64,
    DataType.CDF_FLOAT: np.float32,
    DataType.CDF_REAL4: np.float32,
    DataType.CDF_DOUBLE: np.float64,
    DataType.CDF_REAL8: np.float64,
    DataType.CDF_TIME_TT2000: np.int64,
    DataType.CDF_EPOCH: np.float64
}


_NUMERIC_DTYPES_ = {data_type: np.dtype(dtype) for data_type, dtype in _CDF_TYPES_TO_NUMPY_DTYPE_.items()
                    if dtype is not None and data_type not in (DataType.CDF_TIME_TT2000, DataType.CDF_EPOCH)}


# CDF has no boolean or unsigned 64-bit type: such attribute values are stored as UINT1 and
# INT8, when they fit (variables do the same in C++). Keyed by dtype.num, looked up on every
# attribute entry.
_DEFAULT_CDF_TYPE_ = {np.dtype(np.bool_).num: DataType.CDF_UINT1}
_DEFAULT_CDF_TYPE_.update({np.dtype(t).num: DataType.CDF_INT8 for t in (np.uint64, np.ulonglong)
                           if np.dtype(t).itemsize == 8})


def _keeps_every_value(values: np.ndarray, converted: np.ndarray) -> bool:
    if values.size == 0:
        return True
    if values.dtype.kind in "iu" and converted.dtype.kind in "iu":
        info = np.iinfo(converted.dtype)
        return info.min <= int(values.min()) and int(values.max()) <= info.max
    with np.errstate(all="ignore"):
        back = converted.astype(values.dtype)
    return np.array_equal(back, values, equal_nan=values.dtype.kind == "f")


def _exactly_as(values: np.ndarray, data_type) -> np.ndarray:
    """values in the numpy type of the numeric CDF type data_type. pycdfpp copies values as raw
    memory, so they are converted first, and only when no value changes: an int32 array given
    as CDF_FLOAT was stored as its bits."""
    if data_type is None:
        return values
    target = _NUMERIC_DTYPES_.get(data_type)
    if target is None or values.dtype.kind not in "biuf":
        return values
    if values.dtype.kind == target.kind and values.dtype.itemsize == target.itemsize:
        return values
    with np.errstate(all="ignore"):  # NaN or infinities cast to integers: checked just below
        converted = values.astype(target)
    if not _keeps_every_value(values, converted):
        raise ValueError(f"{values.dtype} values can't be stored as {data_type.name} without changing "
                         f"some of them: convert them first (values.astype(...)) if that is intended")
    return converted


def _check_stored_as_is(values: np.ndarray, data_type):
    """New values of an existing variable keep its type: converting them is asked explicitly."""
    target = _NUMERIC_DTYPES_.get(data_type)
    if target is None or values.dtype.kind not in "biuf":
        return
    if values.dtype.kind != target.kind or values.dtype.itemsize != target.itemsize:
        raise ValueError(f"{values.dtype} values don't match the variable's {data_type.name} type: "
                         f"pass data_type to convert them, or force=True to change the type")


def _first_item(values):
    while isinstance(values, (list, tuple)) and len(values):
        values = values[0]
    return values


def _holds_datetime(values: list):
    return type(_first_item(values)) is datetime


_SMALL_INTEGER_TYPES_ = ((np.int8, np.uint8), (np.int16, np.uint16), (np.int32, np.uint32))


def _min_integer_dtype(values: np.ndarray):
    """The smallest integer type holding every value: unsigned unless one is negative."""
    min_v, max_v = int(values.min()), int(values.max())
    for signed, unsigned in _SMALL_INTEGER_TYPES_:
        dtype = signed if min_v < 0 else unsigned
        info = np.iinfo(dtype)
        if info.min <= min_v and max_v <= info.max:
            return dtype
    return np.int64 if min_v < 0 else np.uint64


def _values_view_and_type(values: np.ndarray or list, data_type: DataType or None = None):
    """The buffer and CDF type of an attribute entry. The C++ side makes it contiguous and
    copies it."""
    if type(values) is list:
        if _holds_datetime(values):
            return _values_view_and_type(np.array(values, dtype="datetime64[ns]"), data_type)
        python_integers = not isinstance(_first_item(values), np.generic)
        values = np.array(values)
        if data_type is None and python_integers and values.size and values.dtype.kind in "iu":
            values = values.astype(_min_integer_dtype(values))
        return _values_view_and_type(values, data_type)
    target = _DEFAULT_CDF_TYPE_.get(values.dtype.num) if data_type is None else data_type
    if target is not None:
        values = _exactly_as(values, target)
    if values.dtype.kind == "M" and data_type in (None, DataType.CDF_TIME_TT2000, DataType.CDF_EPOCH,
                                                  DataType.CDF_EPOCH16):
        return (values.astype(np.dtype('datetime64[ns]'), copy=False).view(np.uint64),
                data_type or DataType.CDF_TIME_TT2000)
    return values, data_type or _NUMPY_TO_CDF_TYPE_.get(values.dtype.num, DataType.CDF_NONE)


def _strict_kwargs(arg_names, keyword_only=()):
    """Decorator that maps positional args to named kwargs and rejects unknown kwargs.

    Parameters
    ----------
    arg_names : list of str
        Allowed keyword argument names, in positional order (excluding 'self').
    keyword_only : list of str
        Allowed keyword argument names that can't be passed positionally.
    """
    allowed = set(arg_names) | set(keyword_only)
    def decorator(fn):
        @wraps(fn)
        def wrapper(self, *args, **kwargs):
            for i, arg in enumerate(args):
                if i < len(arg_names):
                    kwargs[arg_names[i]] = arg
                else:
                    raise TypeError(f"{fn.__name__}() takes at most {len(arg_names)} positional arguments ({i + 1} given)")
            unknown = set(kwargs.keys()) - allowed
            if unknown:
                raise TypeError(f"{fn.__name__}() got unexpected keyword argument(s): {', '.join(sorted(unknown))}")
            return fn(self, **kwargs)
        return wrapper
    return decorator


def _as_single_record(values):
    """Values of a non-record-varying variable are its only record: add the record axis if
    it is missing. Values with zero or one record are left as they are."""
    if isinstance(values, np.ndarray):
        return values[np.newaxis] if values.ndim == 0 or len(values) > 1 else values
    return [list(values)] if len(values) > 1 else values


def _add_trailing_unit_dims(values, record_shape):
    """Records declared with trailing dimensions of size 1, like the (1,) scalars of many
    master CDFs, accept values that leave them out: (N,) fills a variable of records (1,)."""
    if not isinstance(values, np.ndarray) or values.dtype.kind in "USO":
        return values
    missing = len(record_shape) - (values.ndim - 1)
    known = tuple(record_shape[:values.ndim - 1])
    if missing > 0 and values.shape[1:] == known and tuple(record_shape[values.ndim - 1:]) == (1,) * missing:
        return values.reshape(values.shape + (1,) * missing)
    return values


def _patch_set_values():
    def _set_values_wrapper(self, values, data_type=None, force=False, copy=True):
        """Sets or resets the values of the variable.

        Parameters
        ----------
        values : numpy.ndarray or list or tuple or Variable
            The values to set for the variable.
        data_type : DataType or None, optional
            The data type of the variable. If None, the data type is inferred from the values. (Default is None)
            When passing integer values as a list or tuple, it will choose the smallest data type that can hold all the values.
            When passing a Variable, the data type is taken from the Variable.
        force : bool, optional
            If True, allows to overwrite existing values even if the shape or data type do not match.
            (Default is False)
        copy : bool, optional
            If False, the variable borrows the numpy array instead of copying it, and saving writes
            straight from it: modify the array only if you want the change saved. Reading or
            modifying the variable's values copies them first. Raises ValueError if the array
            can't be borrowed: it must be C-contiguous, in native byte order, and hold numeric
            values stored as-is (not strings or times). (Default is True)

        Returns
        -------
        None

        Raises
        ------
        ValueError
            If the shape or data type do not match and force is False, or if copy is False and
            the values can't be borrowed.

        Examples
        --------
        >>> from pycdfpp import CDF, DataType
        >>> import numpy as np
        >>> cdf = CDF()
        >>> cdf.add_variable("var1")
        var1:
          shape: [  ]
          type: CDF_NONE
          record vary: True
          compression: None
          ...
        >>> # Setting values with numpy array
        >>> cdf["var1"].set_values(np.arange(10, 20, dtype=np.int32))
        >>> cdf["var1"].values
        array([10, 11, 12, 13, 14, 15, 16, 17, 18, 19], dtype=int32)
        """
        if isinstance(values, Variable):
            return self._set_values(values, force=force)
        keeps_its_type = not force and self.type != DataType.CDF_NONE
        if data_type is not None and data_type == DataType.CDF_NONE:
            data_type = None
        if not isinstance(values, np.ndarray):
            if isinstance(values, (list, tuple)) and isinstance(_first_item(values), (np.generic, bytes)):
                values = np.array(values)
        if isinstance(values, np.ndarray):
            if data_type is None and keeps_its_type:
                _check_stored_as_is(values, self.type)
            elif copy and data_type is not None:
                values = _exactly_as(values, data_type)
        if self.is_nrv:
            values = _as_single_record(values)
        if keeps_its_type:
            values = _add_trailing_unit_dims(values, self.shape[1:])
        if not copy:
            if not isinstance(values, np.ndarray):
                raise ValueError("copy=False needs a numpy array")
            return self._set_values(values, data_type=data_type, force=force, copy=False)
        return self._set_values(values, data_type=data_type, force=force)

    # Removed python injected wrappers, the logic is now implemented in C++
    Variable.set_values = _set_values_wrapper


def _patch_add_variable():
    @overload
    def _add_variable_wrapper(self: CDF,
                              name: str,
                              values: np.ndarray or None = None, data_type: DataType or None = None,
                              is_nrv: bool = False,
                              compression: CompressionType = CompressionType.no_compression,
                              attributes: Mapping[str, List[Any]] or None = None,
                              copy: bool = True, *, compression_level: int = 6,
                              sparse_records: SparseRecords = SparseRecords.no_sparse_records,
                              pad_value=None) -> Variable:
        ...

    @overload
    def _add_variable_wrapper(self: CDF, variable: Variable) -> Variable:
        ...

    @_strict_kwargs(['name', 'values', 'data_type', 'is_nrv', 'compression', 'attributes', 'copy'],
                    keyword_only=['compression_level', 'sparse_records', 'pad_value'])
    def _add_variable_wrapper(self, name=None, values=None, data_type=None,
                              is_nrv=False, compression=CompressionType.no_compression,
                              attributes=None, copy=True, *, compression_level=6,
                              sparse_records=SparseRecords.no_sparse_records, pad_value=None) -> Variable:
        """Adds a new variable to the CDF.

        This method can be called in two ways:
        1. With variable parameters: add_variable(name, values=None, data_type=None, is_nrv=False, compression=CompressionType.no_compression, attributes=None, copy=True, *, compression_level=6, sparse_records=SparseRecords.no_sparse_records, pad_value=None)
        2. With a Variable object: add_variable(variable)

        Parameters
        ----------
        name : str
            The name of the variable to add.
        values : numpy.ndarray or list or None, optional
            The values to set for the variable. If None, the variable is created with no values. (Default is None)
            When a list is passed, the values are converted to a numpy.ndarray with the appropriate data type, with integers, it will choose the smallest data type that can hold all the values.
        data_type : DataType or None, optional
            The data type of the variable. If None, the data type is inferred from the values. (Default is None)
        is_nrv : bool, optional
            Whether or not the variable is a non-record variable. (Default is False)
        compression : CompressionType, optional
            The compression type to use for the variable. (Default is CompressionType.no_compression)
        attributes : Mapping[str, List[Any]] or None, optional
            The attributes to set for the variable. If None, the variable is created with no attributes. (Default is None)
        copy : bool, optional
            If False, the variable borrows the numpy array instead of copying it, see
            Variable.set_values. Saves the copy of big arrays. (Default is True)
        compression_level : int, optional, keyword-only
            The GZIP compression level, from 1 to 9, ignored by other compression types. (Default is 6)
        sparse_records : SparseRecords, optional, keyword-only
            How readers fill the records the file doesn't store, see Variable.sparse_records.
            (Default is SparseRecords.no_sparse_records)
        pad_value : optional, keyword-only
            The value readers give the records the file doesn't store, see Variable.pad_value.
            (Default is None: the file declares no pad value)
        variable : Variable
            An existing Variable object to add to the CDF (for the second calling method).

        Returns
        -------
        Variable or None
            Returns the newly created variable if successful. Otherwise, returns None.

        Raises
        ------
        ValueError
            If the variable already exists.

        Examples
        --------
        >>> from pycdfpp import CDF, DataType, CompressionType
        >>> import numpy as np
        >>> cdf = CDF()
        >>> # First method: creating a new variable with parameters
        >>> cdf.add_variable("var1", np.arange(10, dtype=np.int32), DataType.CDF_INT4, compression=CompressionType.gzip_compression)
        var1:
          shape: [ 10 ]
          type: CDF_INT1
          record varry: True
          compression: GNU GZIP
          ...
        >>> # Second method: adding an existing variable
        >>> cdf2 = CDF()
        >>> cdf2.add_variable(cdf["var1"])  # Assuming var1 is already defined in cdf (from the first method)
        var1:
          shape: [ 5 ]
          type: CDF_INT1
          record varry: True
          compression: GNU GZIP
          ...
        """
        if isinstance(name, Variable):
            return self._add_variable(variable=name)
        var = self._add_variable(name, is_nrv=is_nrv, compression=compression,
                                 compression_level=compression_level)
        var.sparse_records = sparse_records
        if values is not None:
            var.set_values(values, data_type, copy=copy)
        elif data_type is not None:
            var.set_values([], data_type)
        if pad_value is not None:
            var.pad_value = pad_value
        if attributes is not None and var is not None:
            for attr_name, attr_values in attributes.items():
                var.add_attribute(attr_name, attr_values)
        return var

    CDF.add_variable = _add_variable_wrapper


def _single_string(values: np.ndarray):
    if values.size != 1:
        raise ValueError(f"An attribute entry holds one string, got an array of {values.size}")
    return values.reshape(-1)[0]


def _as_attribute_entry(values):
    """A single number, datetime or CDF time value becomes a one-element entry; numpy scalars
    keep their dtype. Numpy strings, as read from CDF_CHAR variables, become string entries:
    bytes stay numpy bytes so their characters are written as they are, without decoding."""
    if isinstance(values, np.ndarray):
        if values.dtype.kind in "SU":
            return _as_attribute_entry(_single_string(values))
        return np.atleast_1d(values) if values.ndim == 0 else values
    if isinstance(values, bytes):
        return np.bytes_(values)
    if isinstance(values, str):
        return str(values)
    if isinstance(values, tuple):
        return _as_attribute_entry(list(values))
    if isinstance(values, list) and values and isinstance(values[0], (str, bytes)):
        return _as_attribute_entry(np.array(values))
    if isinstance(values, np.generic):
        return np.atleast_1d(values)
    if isinstance(values, (int, float, datetime, tt2000_t, epoch, epoch16)):
        return [values]
    return values


def _attribute_values_view_and_type(values: np.ndarray or list or str, data_type=None):
    values = _as_attribute_entry(values)
    if type(values) is str:
        if data_type is None:
            data_type = DataType.CDF_CHAR
        elif data_type == DataType.CDF_CHAR or data_type == DataType.CDF_UCHAR:
            pass
        else:
            raise ValueError(
                f"Can't set attribute of type {data_type} with values of type str")
        return (values, data_type)
    return _values_view_and_type(values, data_type)


def _patch_add_variable_attribute():
    @overload
    def _add_attribute_wrapper(self, name: str, values: np.ndarray or List[float or int or datetime or np.integer] or str, data_type=None) -> VariableAttribute:
        ...
    @overload
    def _add_attribute(self: Variable, attribute: VariableAttribute) -> VariableAttribute:
        ...

    @_strict_kwargs(['name', 'values', 'data_type'])
    def _add_attribute_wrapper(self, name=None, values=None, data_type=None) -> VariableAttribute:
        """Adds a new attribute to the variable.

        This method can be called in two ways:
        1. With attribute parameters: add_attribute(name, values, data_type=None)
        2. With a VariableAttribute object: add_attribute(attribute)

        Parameters
        ----------
        name : str
            The name of the attribute to add.
        values : np.ndarray or List[float or int or datetime] or str
            The values to set for the attribute.
            When a list is passed, the values are converted to a numpy.ndarray with the appropriate data type, with integers, it will choose the smallest data type that can hold all the values.
        data_type : DataType or None, optional
            The data type of the attribute. If None, the data type is inferred from the values. (Default is None)
        attribute : VariableAttribute
            An existing VariableAttribute object to add to the variable (for the second calling method).

        Returns
        -------
        VariableAttribute
            Returns the newly created attribute if successful.

        Raises
        ------
        ValueError
            If the attribute already exists.

        Examples
        --------
        >>> from pycdfpp import CDF, DataType
        >>> import numpy as np
        >>> cdf = CDF()
        >>> cdf.add_variable("var1", np.arange(10, dtype=np.int32), DataType.CDF_INT4)
        var1:
          shape: [ 10 ]
          type: CDF_INT1
          record varry: True
          compression: None
          ...
        >>> # First method: creating a new attribute with parameters
        >>> cdf["var1"].add_attribute("attr1", np.arange(10, dtype=np.int32), DataType.CDF_INT4)
        attr1: [ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 ]
        >>> # Second method: adding an existing attribute
        >>> var2 = cdf.add_variable("var2", np.arange(5))
        >>> var2.add_attribute(cdf["var1"].attributes["attr1"])
        attr1: [ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 ]
        """
        if isinstance(name, VariableAttribute):
            return self._add_attribute(attribute=name)
        v, t = _attribute_values_view_and_type(values, data_type)
        return self._add_attribute(name=name, values=v, data_type=t)

    Variable.add_attribute = _add_attribute_wrapper


def _patch_add_cdf_attribute():
    @overload
    def _add_attribute_wrapper(self: CDF, name: str,
                                entries_values: List[np.ndarray or List[float or int or datetime] or str],
                                entries_types: List[DataType or None] or None = None) -> Attribute:
        ...
    @overload
    def _add_attribute(self: CDF, attribute: Attribute) -> Attribute:
        ...
    @_strict_kwargs(['name', 'entries_values', 'entries_types'])
    def _add_attribute_wrapper(self, name=None, entries_values=None, entries_types=None) -> Attribute:
        """Adds a new attribute to the CDF.

        This method can be called in two ways:
        1. With attribute parameters: add_attribute(name, entries_values, entries_types=None)
        2. With an Attribute object: add_attribute(attribute)

        Parameters
        ----------
        name : str
            The name of the attribute to add.
        entries_values : List[np.ndarray or List[float or int or datetime] or str]
            The values entries to set for the attribute.
            When a list is passed, the values are converted to a numpy.ndarray with the appropriate data type, with integers, it will choose the smallest data type that can hold all the values.
        entries_types : List[DataType] or None, optional
            The data type for each entry of the attribute. If None, the data type is inferred from the values. (Default is None)
        attribute : Attribute
            An existing Attribute object to add to the CDF (for the second calling method).

        Returns
        -------
        Attribute or None
            Returns the newly created attribute if successful. Otherwise, returns None.

        Raises
        ------
        ValueError
            If the attribute already exists.

        Examples
        --------
        >>> from pycdfpp import CDF, DataType
        >>> import numpy as np
        >>> from datetime import datetime
        >>> cdf = CDF()
        >>> # First method: creating a new attribute with parameters
        >>> cdf.add_attribute("attr1", [np.arange(10, dtype=np.int32)], [DataType.CDF_INT4])
        attr1: [ [ [ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 ] ] ]
        >>> # Second method: adding an existing attribute
        >>> cdf2 = CDF()
        >>> cdf2.add_attribute(cdf.attributes["attr1"])
        attr1: [ [ [ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 ] ] ]
        >>> # Another example with multiple entries of different types
        >>> cdf.add_attribute("multi", [np.arange(2, dtype=np.int32), [1.,2.,3.], "hello", [datetime(2010,1,1), datetime(2020,1,1)]])
        multi: [ [ [ 0, 1 ], [ 1, 2, 3 ], "hello", [ 2010-01-01T00:00:00.000000000, 2020-01-01T00:00:00.000000000 ] ] ]
        """
        if isinstance(name, Attribute):
            return self._add_attribute(attribute=name)
        entries_types = entries_types or [None] * len(entries_values)
        entries = [_attribute_values_view_and_type(values, data_type)
                   for values, data_type in zip(entries_values, entries_types)]
        return self._add_attribute(name=name, entries_values=[v for v, _ in entries],
                                   entries_types=[t for _, t in entries])

    CDF.add_attribute = _add_attribute_wrapper


def _patch_attribute_set_values():
    @overload
    def _attribute_set_values(self: Attribute, entries_values: List[np.ndarray or List[float or int or datetime] or str],
                              entries_types: List[DataType or None] or None = None):
        ...
    @overload
    def _attribute_set_values(self: Attribute, attribute: Attribute):
        ...
    @_strict_kwargs(['entries_values', 'entries_types'])
    def _attribute_set_values(self, entries_values=None, entries_types=None):
        """Sets the values of the attribute.

        This method can be called in two ways:
        1. With values and optional types: set_values(entries_values, entries_types=None)
        2. With another Attribute object: set_values(attribute)

        Parameters
        ----------
        entries_values : List[np.ndarray or List[float or int or datetime] or str]
            The values entries to set for the attribute.
            When a list is passed, the values are converted to a numpy.ndarray with the appropriate data type,
            with integers, it will choose the smallest data type that can hold all the values.
        entries_types : List[DataType] or None, optional
            The data type for each entry of the attribute. If None, the data type is inferred from the values. (Default is None)
        attribute : Attribute
            An existing Attribute object to set the values from (for the second calling method).
        """
        if isinstance(entries_values, Attribute):
            self._set_values(entries_values)
        else:
            entries_types = entries_types or [None] * len(entries_values)
            v, t = [list(l) for l in zip(*[_attribute_values_view_and_type(values, data_type)
                                           for values, data_type in zip(entries_values, entries_types)])]
            self._set_values(v, t)

    Attribute.set_values = _attribute_set_values


def _patch_var_attribute_set_value():
    @overload
    def _attribute_set_value(self: VariableAttribute, value: np.ndarray or List[float or int or datetime] or str, data_type=None):
        ...
    @overload
    def _attribute_set_value(self: VariableAttribute, value: VariableAttribute):
        ...
    @_strict_kwargs(['value', 'data_type'])
    def _attribute_set_value(self, value=None, data_type=None):
        """Sets the value of the variable attribute.

        This method can be called in two ways:
        1. With value and optional data type: set_value(value, data_type=None)
        2. With another VariableAttribute object: set_value(attribute)

        Parameters
        ----------
        value : np.ndarray or List[float or int or datetime] or str
            The value to set for the attribute.
            When a list is passed, the values are converted to a numpy.ndarray with the appropriate data type,
            with integers, it will choose the smallest data type that can hold all the values.
        data_type : DataType or None, optional
            The data type of the attribute. If None, the data type is inferred from the values. (Default is None)
        attribute : VariableAttribute
            An existing VariableAttribute object to set the value from (for the second calling method).

        Examples
        --------
        >>> from pycdfpp import CDF, DataType
        >>> import numpy as np
        >>> from datetime import datetime
        >>> cdf = CDF()
        >>> var = cdf.add_variable("var1", np.arange(10, dtype=np.int32), DataType.CDF_INT4)
        >>> # First method: setting value with parameters
        >>> var.attributes["attr1"].set_value([1, 2, 3])
        >>> # Second method: setting from existing attribute
        >>> var.attributes["attr2"].set_value(var.attributes["attr1"])
        >>> var.attributes["attr2"]
        [ 1, 2, 3 ]
        """
        if isinstance(value, VariableAttribute):
            self._set_value(value)
        else:
            v, t = _attribute_values_view_and_type(value, data_type)
            self._set_value(v, t)

    VariableAttribute.set_value = _attribute_set_value


_patch_add_cdf_attribute()
_patch_add_variable_attribute()
_patch_set_values()
_patch_add_variable()
_patch_attribute_set_values()
_patch_var_attribute_set_value()


def _patch_pad_value():
    def _get_pad_value(self: Variable):
        return self._pad_value

    def _set_pad_value(self: Variable, value):
        if value is None:
            self._clear_pad_value()
        else:
            self._set_pad_value(*_attribute_values_view_and_type(value, self.type))

    Variable.pad_value = property(_get_pad_value, _set_pad_value, doc="""The value readers give
        the records the file doesn't store (see sparse_records), or None when the file declares
        none. It reads and is set like the value of an attribute of the variable's type, FILLVAL
        for instance. Set it to None to remove it.""")


_patch_pad_value()


def filter_cdf(cdf: CDF,
               variables: Union[List[str], str, re.Pattern, Callable[[Variable], bool]] = None,
               attributes: Union[List[str], str, re.Pattern, Callable[[Attribute], bool]]= None,
               inplace=False) -> CDF:
    """Filters the CDF object based on the provided criteria.

    Parameters
    ----------
    cdf : CDF
        The CDF object to filter.
    variables : Union[List[str], str, re.Pattern, Callable[[Variable], bool]], optional
        A list of variable names to keep, a regex pattern, or a callable that returns True for variables to keep.
        If None (default), all variables are kept.
    attributes : Union[List[str], str, re.Pattern, Callable[[Attribute], bool]], optional
        A list of global attribute names to keep, a regex pattern, or a callable that returns True for attributes
        to keep. If None (default), all global attributes are kept.
    inplace : bool, optional
        If True, modifies the original CDF object. If False, returns a new filtered CDF object. (Default is False)

    Returns
    -------
    CDF
        Returns a new CDF object with the filtered variables and attributes.
    """

    result_cdf = cdf if inplace else copy.deepcopy(cdf)

    def _make_filter(criterion):
        if criterion is None:
            return lambda x: True
        elif isinstance(criterion, (list, tuple)):
            return lambda x: x.name in criterion
        elif isinstance(criterion, str):
            return lambda x: re.match(criterion, x.name) is not None
        elif isinstance(criterion, re.Pattern):
            return lambda x: criterion.match(x.name) is not None
        elif callable(criterion):
            return criterion
        else:
            raise TypeError(f"Unsupported type for filter criterion: {type(criterion)}")
    
    var_filter = _make_filter(variables)
    attr_filter = _make_filter(attributes)

    vars_to_remove = [ name for name, var in result_cdf.items() if not var_filter(var)]
    attrs_to_remove = [ name for name, attr in list(result_cdf.attributes.items()) if not attr_filter(attr)]

    list(map(result_cdf._remove_variable, vars_to_remove))
    list(map(result_cdf._remove_attribute, attrs_to_remove))
    
    return result_cdf

CDF.filter = filter_cdf

def _records_as_array(values):
    """A list of numpy records, e.g. list(variable.values), back to the array they came from."""
    if isinstance(values, (list, tuple)) and len(values) and isinstance(values[0], np.void):
        return np.array(values)
    return values


def to_datetime64(values):
    """Convert any compatible given collection of time values to a numpy.datetime64 array.

    Parameters
    ----------
    values: Variable or epoch or List[epoch] or numpy.ndarray[epoch] or epoch16 or List[epoch16] or numpy.array[epoch16] or tt2000_t or List[tt2000_t] or numpy.array[tt2000_t]
        input value(s)
to convert to numpy.datetime64

    Returns
    -------
    numpy.ndarray[numpy.datetime64]

    Raises
    ------
    TypeError or IndexError
        If the input values are not compatible time types.

    Notes
    -----
    On modern x86_64 systems, it will use the CPU's vectorized instructions to perform the conversion even faster.
    """
    return _pycdfpp.to_datetime64(_records_as_array(values))


def to_datetime(values):
    """
    to_datetime

    Parameters
    ----------
    values: Variable or epoch or List[epoch] or epoch16 or List[epoch16] or tt2000_t or List[tt2000_t] or numpy.array[numpy.datetime64[ns]]
        input value(s)
to convert to datetime.datetime

    Returns
    -------
    List[datetime.datetime]

    Raises
    ------
    TypeError or IndexError
        If the input values are not compatible time types.
    """
    return _pycdfpp.to_datetime(_records_as_array(values))


def to_tt2000(values):
    """
    to_tt2000

    Parameters
    ----------
    values: datetime.datetime or List[datetime.datetime] or numpy.array[numpy.datetime64[ns]]
        input value(s)
to convert to CDF tt2000

    Returns
    -------
    tt2000_t or List[tt2000_t]
    """
    return _pycdfpp.to_tt2000(values)


def to_epoch(values):
    """
    to_epoch

    Parameters
    ----------
    values: datetime.datetime or List[datetime.datetime] or numpy.array[numpy.datetime64[ns]]
        input value(s)
to convert to CDF epoch

    Returns
    -------
    epoch or List[epoch]
    """
    return _pycdfpp.to_epoch(values)


def to_time_string(values, format: str):
    """Format CDF time values as an array of fixed-width ASCII strings.

    Parameters
    ----------
    values : Variable or numpy.ndarray[tt2000_t] or numpy.ndarray[epoch] or numpy.ndarray[epoch16]
        CDF time values to format.
    format : str
        strftime-compatible format string (e.g. ``'%Y-%m-%dT%H:%M:%SZ'``).
        ``%S`` includes the fraction of a second, with 9 digits (nanoseconds) for every
        time type.

    Returns
    -------
    numpy.ndarray
        Array of byte strings (dtype ``S{N}``) with the same shape as input.
    """
    return _pycdfpp.to_time_string(_records_as_array(values), format)


def to_epoch16(values):
    """
    to_epoch16

    Parameters
    ----------
    values: datetime.datetime or List[datetime.datetime] or numpy.array[numpy.datetime64[ns]]
        input value(s)
to convert to CDF epoch16

    Returns
    -------
    epoch16 or List[epoch16]
    """
    return _pycdfpp.to_epoch16(values)


class ExperimentalCompressionWarning(UserWarning):
    """A CDF was saved with a codec outside the CDF standard (zstd, blosc2): only CDFpp can read it."""


_EXPERIMENTAL_CODECS = {getattr(CompressionType, name) for name in ("zstd_compression", "blosc2_compression")
                        if hasattr(CompressionType, name)}


def _warn_if_experimental_compression(cdf: CDF):
    used = {cdf.compression} | {cdf[name].compression for name in cdf}
    experimental = sorted(str(codec).split(".")[-1] for codec in used & _EXPERIMENTAL_CODECS)
    if experimental:
        warnings.warn(f"saving with {', '.join(experimental)}: this is not standard CDF, and only CDFpp "
                      f"can read the file. Use gzip_compression for files meant to be shared.",
                      ExperimentalCompressionWarning, stacklevel=3)


def save(cdf: CDF, fname: Union[str, os.PathLike, None] = None):
    """
    Save a CDF to a file, or to memory.

    Saving over the file the CDF was loaded from is safe, even with lazy loading: every
    value is read before the file is overwritten.

    Parameters
    ----------
    cdf : CDF
        The CDF to save.
    fname : str or os.PathLike, optional
        Destination file name. When omitted, the CDF is serialized in memory.

    Returns
    -------
    bool or buffer
        True when saving to a file; otherwise an object implementing the buffer protocol
        (e.g. ``bytes(pycdfpp.save(cdf))``).

    Raises
    ------
    OSError
        When the file can't be written.

    Warns
    -----
    ExperimentalCompressionWarning
        When the CDF or one of its variables uses zstd_compression or blosc2_compression.
    """
    _warn_if_experimental_compression(cdf)
    if fname is None:
        return _pycdfpp.save(cdf)
    path = os.fspath(fname)
    if not _pycdfpp.save(cdf, path):
        raise OSError(f"could not write the CDF file '{path}'")
    return True


def load(file_or_buffer: Union[str, os.PathLike, ByteString], iso_8859_1_to_utf8: bool = True,
         lazy_load: bool = True):
    """
    Load and parse a CDF file.

    Parameters
    ----------
    file_or_buffer : str or os.PathLike or ByteString
        Either a file path or an in-memory file implementing the Python buffer protocol.
    iso_8859_1_to_utf8 : bool, optional
        Automatically convert Latin-1 characters to their equivalent UTF counterparts when True.
        For CDF files prior to version 3.8, UTF-8 wasn't supported and some CDF files might contain "illegal" Latin-1 characters.
        This option has no impact on valid UTF-8 characters.
        (Default is True)
    lazy_load : bool, optional
        Controls whether variable values are loaded immediately or only when accessed by the user.
        If True, variables' values are loaded on demand. If False, all variable values are loaded during parsing.
        (Default is True)

    Returns
    -------
    CDF

    Raises
    ------
    FileNotFoundError
        When the file doesn't exist.
    ValueError
        When the file or buffer is not a valid CDF file.
    """
    if isinstance(file_or_buffer, (str, os.PathLike)):
        path = os.fspath(file_or_buffer)
        if not os.path.exists(path):
            raise FileNotFoundError(errno.ENOENT, "No such CDF file", path)
        cdf = _pycdfpp.load(path, iso_8859_1_to_utf8, lazy_load)
        if cdf is None:
            raise ValueError(f"'{path}' is not a valid CDF file")
        return cdf
    if lazy_load:
        cdf = _pycdfpp.lazy_load(file_or_buffer, iso_8859_1_to_utf8)
    else:
        cdf = _pycdfpp.load(file_or_buffer, iso_8859_1_to_utf8)
    if cdf is None:
        raise ValueError("the buffer does not hold a valid CDF file")
    return cdf


def _stringify_time_values(values, values_type):
    if values_type in (DataType.CDF_TIME_TT2000, DataType.CDF_EPOCH, DataType.CDF_EPOCH16):
        return list(map(str, values))
    else:
        return values


@singledispatch
def to_dict_skeleton(obj: Any) -> Any:
    pass


@to_dict_skeleton.register(Attribute)
def _(attribute: Attribute) -> dict:
    """
    to_dict_skeleton builds a dictionary skeleton of the Attribute object for use with json.dumps or similar functions.

    Parameters
    ----------
    attribute: Attribute
        input Attribute object

    Returns
    -------
    dict
        dictionary skeleton of the Attribute
    """
    return {
        "values": [_stringify_time_values(attribute[i], attribute.type(i)) for i in range(len(attribute))],
        "types": [str(attribute.type(i)) for i in range(len(attribute))],
    }


@to_dict_skeleton.register(VariableAttribute)
def _(attribute: VariableAttribute) -> dict:
    """
    to_dict_skeleton builds a dictionary skeleton of the VariableAttribute object for use with json.dumps or similar functions.

    A variable attribute holds a single entry, so its skeleton uses the same
    shape as a global Attribute with a single-element values/types list.

    Parameters
    ----------
    attribute: VariableAttribute
        input VariableAttribute object

    Returns
    -------
    dict
        dictionary skeleton of the VariableAttribute
    """
    return {
        "values": [_stringify_time_values(attribute.value, attribute.type())],
        "types": [str(attribute.type())],
    }


@to_dict_skeleton.register(Variable)
def _(variable: Variable) -> dict:
    """
    to_dict_skeleton builds a dictionary skeleton of the Variable object for use with json.dumps or similar functions.

    Parameters
    ----------
    variable: Variable
        input Variable object

    Returns
    -------
    dict
        dictionary skeleton of the Variable
    """
    return {
        "attributes": {
            k: to_dict_skeleton(a) for k, a in variable.attributes.items()
        },
        "type": str(variable.type),
        "shape": variable.shape,
        "compression": str(variable.compression),
        "is_nrv": variable.is_nrv
    }


@to_dict_skeleton.register(CDF)
def _(cdf: CDF) -> dict:
    """
    to_dict_skeleton builds a dictionary skeleton of the CDF object for use with json.dumps or similar functions.

    Parameters
    ----------
    cdf: CDF
        input CDF object

    Returns
    -------
    dict
        dictionary skeleton of the CDF
    """
    return {
        "compression": str(cdf.compression),
        "attributes": {
            k: to_dict_skeleton(a) for k, a in cdf.attributes.items()
        },
        "variables": {
            k: to_dict_skeleton(v) for k, v in cdf.items()
        }
    }


def default_pad_value(cdf_type: DataType):
    """
    Returns the default pad value for the given CDF data type (CDF User's Guide, table 2.8):
    the value of records a file doesn't store, when it declares no pad value of its own.
    """
    if cdf_type in (DataType.CDF_INT1, DataType.CDF_BYTE):
        return np.int8(-127)
    if cdf_type == DataType.CDF_UINT1:
        return np.uint8(254)
    if cdf_type == DataType.CDF_INT2:
        return np.int16(-32767)
    if cdf_type == DataType.CDF_UINT2:
        return np.uint16(65534)
    if cdf_type == DataType.CDF_INT4:
        return np.int32(-2147483647)
    if cdf_type == DataType.CDF_UINT4:
        return np.uint32(4294967294)
    if cdf_type == DataType.CDF_INT8:
        return np.int64(-9223372036854775807)
    if cdf_type in (DataType.CDF_REAL4, DataType.CDF_FLOAT):
        return np.float32(-1e30)
    if cdf_type in (DataType.CDF_REAL8, DataType.CDF_DOUBLE):
        return np.float64(-1e30)
    if cdf_type in (DataType.CDF_CHAR, DataType.CDF_UCHAR):
        return b' '
    if cdf_type == DataType.CDF_TIME_TT2000:
        return tt2000_t(-9223372036854775807)
    if cdf_type == DataType.CDF_EPOCH:
        return epoch(0.0)
    if cdf_type == DataType.CDF_EPOCH16:
        return epoch16(0.0, 0.0)
    return None


def default_fill_value(cdf_type: DataType):
    """
    Return a default fill value for the given CDF data type.

    Parameters
    ----------
    cdf_type : DataType
        The CDF data type for which to return the default fill value.
    Returns
    -------
    Any
        The default fill value for the specified CDF data type.
    """
    if cdf_type in (DataType.CDF_INT1, DataType.CDF_BYTE):
        return np.int8(-128)
    if cdf_type == DataType.CDF_UINT1:
        return np.uint8(255)
    if cdf_type == DataType.CDF_INT2:
        return np.int16(-32768)
    if cdf_type == DataType.CDF_UINT2:
        return np.uint16(65535)
    if cdf_type == DataType.CDF_INT4:
        return np.int32(-2147483648)
    if cdf_type == DataType.CDF_UINT4:
        return np.uint32(4294967295)
    if cdf_type == DataType.CDF_INT8:
        return np.int64(-9223372036854775808)
    if cdf_type in (DataType.CDF_REAL4, DataType.CDF_FLOAT):
        return np.float32(-1e31)
    if cdf_type in (DataType.CDF_REAL8, DataType.CDF_DOUBLE):
        return np.float64(-1e31)
    if cdf_type == DataType.CDF_TIME_TT2000:
        return tt2000_t(-9223372036854775808)
    if cdf_type == DataType.CDF_EPOCH:
        return epoch(-1e31)
    if cdf_type == DataType.CDF_EPOCH16:
        return epoch16(-1e31, - 1e31)
    return None
