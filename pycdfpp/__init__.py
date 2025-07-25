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
from functools import singledispatch
from datetime import datetime
import re

import numpy as np

from ._pycdfpp import DataType, CompressionType, Majority, Variable, VariableAttribute, Attribute, CDF, tt2000_t, epoch, \
    epoch16, save
from . import _pycdfpp

# ByteString is deprecated in Python 3.9+ and removed in Python 3.14
ByteString = Union[bytes, bytearray, memoryview]

__version__ = _pycdfpp.__version__
__here__ = os.path.dirname(os.path.abspath(__file__))
sys.path.append(__here__)
if sys.platform == 'win32' and sys.version_info[0] == 3 and sys.version_info[1] >= 8:
    os.add_dll_directory(__here__)

__all__ = ['tt2000_t', 'epoch', 'epoch16', 'load', 'save', 'CDF', 'Variable',
           'Attribute', 'to_datetime64', 'to_datetime', 'DataType', 'CompressionType', 'Majority']

_NUMPY_TO_CDF_TYPE_ = (
    DataType.CDF_NONE,
    DataType.CDF_INT1,
    DataType.CDF_UINT1,
    DataType.CDF_INT2,
    DataType.CDF_UINT2,
    DataType.CDF_INT4,
    DataType.CDF_UINT4,
    DataType.CDF_INT8,
    DataType.CDF_NONE,
    DataType.CDF_NONE,
    DataType.CDF_NONE,
    DataType.CDF_FLOAT,
    DataType.CDF_DOUBLE,
    DataType.CDF_NONE,
    DataType.CDF_NONE,
    DataType.CDF_NONE,
    DataType.CDF_NONE,
    DataType.CDF_NONE,
    DataType.CDF_CHAR,
    DataType.CDF_NONE,
    DataType.CDF_NONE,
    DataType.CDF_TIME_TT2000
)

_CDF_TYPES_COMPATIBILITY_TABLE_ = {
    DataType.CDF_NONE: (DataType.CDF_CHAR, DataType.CDF_UCHAR, DataType.CDF_INT1, DataType.CDF_BYTE, DataType.CDF_UINT1,
                        DataType.CDF_UINT2, DataType.CDF_UINT4, DataType.CDF_INT1, DataType.CDF_INT2, DataType.CDF_INT4,
                        DataType.CDF_INT8, DataType.CDF_FLOAT, DataType.CDF_REAL4, DataType.CDF_DOUBLE,
                        DataType.CDF_REAL8, DataType.CDF_TIME_TT2000, DataType.CDF_EPOCH, DataType.CDF_EPOCH16),
    DataType.CDF_CHAR: (DataType.CDF_CHAR, DataType.CDF_UCHAR),
    DataType.CDF_UCHAR: (DataType.CDF_CHAR, DataType.CDF_UCHAR),
    DataType.CDF_BYTE: (DataType.CDF_INT1, DataType.CDF_BYTE),
    DataType.CDF_INT1: (DataType.CDF_INT1, DataType.CDF_BYTE),
    DataType.CDF_UINT1: (DataType.CDF_UINT1,),
    DataType.CDF_INT2: (DataType.CDF_INT2,),
    DataType.CDF_UINT2: (DataType.CDF_UINT2,),
    DataType.CDF_INT4: (DataType.CDF_INT4,),
    DataType.CDF_UINT4: (DataType.CDF_UINT4,),
    DataType.CDF_INT8: (DataType.CDF_INT8,),
    DataType.CDF_FLOAT: (DataType.CDF_FLOAT, DataType.CDF_REAL4),
    DataType.CDF_REAL4: (DataType.CDF_FLOAT, DataType.CDF_REAL4),
    DataType.CDF_DOUBLE: (DataType.CDF_DOUBLE, DataType.CDF_REAL8),
    DataType.CDF_REAL8: (DataType.CDF_DOUBLE, DataType.CDF_REAL8),
    DataType.CDF_TIME_TT2000: (DataType.CDF_TIME_TT2000,),
    DataType.CDF_EPOCH: (DataType.CDF_EPOCH,),
    DataType.CDF_EPOCH16: (DataType.CDF_EPOCH16,),
}

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


def _holds_datetime(values: list):
    if len(values):
        if type(values[0]) is list:
            return _holds_datetime(values[0])
        if type(values[0]) is datetime:
            return True
    return False


def _max_integer_dtype(type1: np.dtype, type2: np.dtype):
    if not np.issubdtype(type1, np.integer):
        return type2
    if not np.issubdtype(type2, np.integer):
        return type1

    if np.dtype(type1).itemsize > np.dtype(type2).itemsize:
        return type1
    else:
        return type2

    return None


def _min_integer_dtype(values: list, target_type: np.dtype or None = None):
    min_v = np.min(values)
    max_v = np.max(values)
    if min_v < 0:
        if min_v >= -128 and max_v <= 127:
            return _max_integer_dtype(np.int8, target_type)
        elif min_v >= -32768 and max_v <= 32767:
            return _max_integer_dtype(np.int16, target_type)
        elif min_v >= -2147483648 and max_v <= 2147483647:
            return _max_integer_dtype(np.int32, target_type)
        else:
            return _max_integer_dtype(np.int64, target_type)
    else:
        if max_v <= 255:
            return _max_integer_dtype(np.uint8, target_type)
        elif max_v <= 65535:
            return _max_integer_dtype(np.uint16, target_type)
        elif max_v <= 4294967295:
            return _max_integer_dtype(np.uint32, target_type)
        else:
            return _max_integer_dtype(np.uint64, target_type)
    return None


def _values_view_and_type(values: np.ndarray or list, data_type: DataType or None = None,
                          target_type: DataType or None = None):
    shrink_int = True
    if type(values) is list:
        if _holds_datetime(values):
            values = np.array(values, dtype="datetime64[ns]")
        else:
            if len(values) and type(values[0]) in (np.int8, np.int16, np.int32, np.int64, np.uint8, np.uint16,
                                                   np.uint32, np.uint64):
                shrink_int = False
            values = np.array(
                values, dtype=_CDF_TYPES_TO_NUMPY_DTYPE_.get(data_type, None))

        if values.dtype.num == 19:
            values = np.char.encode(values, encoding='utf-8')
        elif data_type is None and np.issubdtype(values.dtype, np.integer):
            target_type = _CDF_TYPES_TO_NUMPY_DTYPE_.get(target_type, np.float32)
            if shrink_int:
                if not np.issubdtype(target_type, np.integer):
                    target_type = None
                values = values.astype(_min_integer_dtype(values, target_type))
            else:
                return values, data_type or _NUMPY_TO_CDF_TYPE_[values.dtype.num]
        return _values_view_and_type(values, data_type)
    else:
        if not values.flags['C_CONTIGUOUS']:
            values = np.ascontiguousarray(values)
        elif values.base is not None:
            values = values.copy()
        if values.dtype.num == 21:
            if data_type in (None, DataType.CDF_TIME_TT2000, DataType.CDF_EPOCH, DataType.CDF_EPOCH16):
                return (values.astype(np.dtype('datetime64[ns]'), copy=False).view(np.uint64),
                        data_type or DataType.CDF_TIME_TT2000)
        if values.dtype.num == 19:
            return _values_view_and_type(np.char.encode(values, encoding='utf-8'), data_type)
        else:
            return (values, data_type or _NUMPY_TO_CDF_TYPE_[
                values.dtype.num])


def _patch_set_values():
    @overload
    def _set_values_wrapper(self: Variable, values: np.ndarray or list, data_type: DataType or None = None):
        ...
    @overload
    def _set_values_wrapper(self: Variable, values: Variable):
        ...

    def _set_values_wrapper(self, *args, **kwargs):
        """Sets the values of the variable.
        
        This method can be called in two ways:
        1. With values and optional data type: set_values(values, data_type=None)
        2. With another Variable object: set_values(variable)
        
        Parameters
        ----------
        values : np.ndarray or list
            The values to set for the variable.
            When a list is passed, the values are converted to a numpy.ndarray with the appropriate data type,
            with integers, it will choose the smallest data type that can hold all the values.
        data_type : DataType or None, optional
            The data type of the variable. If None, the data type is inferred from the values. (Default is None)
        variable : Variable
            An existing Variable object to set the values from (for the second calling method).
            
        Raises
        ------
        ValueError
            If the variable already exists or if the values are not compatible with the variable's type.
        """

        if len(args) == 1 and isinstance(args[0], Variable):
            if self.type != DataType.CDF_NONE and self.type != args[0].type:
                raise ValueError(
                    f"Can't set variable of type {self.type} with values of type {args[0].type}")
            self._set_values(args[0])
        else:
            arg_names = ['values', 'data_type']
            for i, arg in enumerate(args):
                if i < len(arg_names):
                    kwargs[arg_names[i]] = arg
                else:
                    raise TypeError(f"Unexpected argument {arg} at position {i+1}. Expected one of {arg_names}.")
            values = kwargs.get('values')
            data_type = kwargs.get('data_type')
            values, data_type = _values_view_and_type(values, data_type, target_type=self.type)
            if data_type not in _CDF_TYPES_COMPATIBILITY_TABLE_[self.type]:
                raise ValueError(
                    f"Can't set variable of type {self.type} with values of type {data_type}")
            if self.is_nrv:
                if self.shape[1:] != values.shape[1:]:
                    if self.shape[1:] != values.shape:
                        raise ValueError(
                            f"Can't set NRV variable of shape {self.shape} with values of shape {values.shape}")
                    else:
                        values = values.reshape((1,) + values.shape)
            elif self.type != DataType.CDF_NONE and self.shape[1:] != values.shape[1:]:
                raise ValueError(
                    f"Can't set variable of shape {self.shape} with values of shape {values.shape}")
            self._set_values(values, data_type)

    Variable.set_values = _set_values_wrapper


def _patch_add_variable():
    @overload
    def _add_variable_wrapper(self: CDF,
                              name: str,
                              values: np.ndarray or None = None, data_type: DataType or None = None,
                              is_nrv: bool = False,
                              compression: CompressionType = CompressionType.no_compression,
                              attributes: Mapping[str, List[Any]] or None = None) -> Variable:
        ...

    @overload
    def _add_variable_wrapper(self: CDF, variable: Variable) -> Variable:
        ...

    def _add_variable_wrapper(self, *args, **kwargs) -> Variable:
        """Adds a new variable to the CDF.

        This method can be called in two ways:
        1. With variable parameters: add_variable(name, values=None, data_type=None, is_nrv=False, compression=CompressionType.no_compression, attributes=None)
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
        if len(args) == 1 and isinstance(args[0], Variable):
            # If the first argument is a Variable, we assume it's an existing variable to add
            return self._add_variable(variable=args[0])
        arg_names = ['name', 'values', 'data_type', 'is_nrv', 'compression', 'attributes']
        for i, arg in enumerate(args):
            if i < len(arg_names):
                kwargs[arg_names[i]] = arg
            else:
                raise TypeError(f"Unexpected argument {arg} at position {i+1}. Expected one of {arg_names}.")
        name = kwargs.get('name')
        values = kwargs.get('values')
        data_type = kwargs.get('data_type')
        is_nrv = kwargs.get('is_nrv', False)
        compression = kwargs.get('compression', CompressionType.no_compression)
        attributes = kwargs.get('attributes', None)

        if values is not None:
            v, t = _values_view_and_type(values, data_type)
            var = self._add_variable(
                name=name, values=v, data_type=t, is_nrv=is_nrv, compression=compression)
        elif data_type is not None:
            v, t = _values_view_and_type([], data_type)
            var = self._add_variable(
                name=name, values=v, data_type=t, is_nrv=is_nrv, compression=compression)
        else:
            var = self._add_variable(
                name=name, is_nrv=is_nrv, compression=compression)
        if attributes is not None and var is not None:
            for name, values in attributes.items():
                var.add_attribute(name, values)
        return var

    CDF.add_variable = _add_variable_wrapper


def _attribute_values_view_and_type(values: np.ndarray or list or str, data_type=None):
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

    def _add_attribute_wrapper(self, *args, **kwargs)-> VariableAttribute:
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
        if len(args) == 1 and isinstance(args[0], VariableAttribute):
            # If the first argument is a VariableAttribute, we assume it's an existing attribute to add
            return self._add_attribute(attribute=args[0])

        arg_names = ['name','values', 'data_type']
        for i, arg in enumerate(args):
            if i < len(arg_names):
                kwargs[arg_names[i]] = arg
            else:
                raise TypeError(f"Unexpected argument {arg} at position {i+1}. Expected one of {arg_names}.")

        v, t = _attribute_values_view_and_type(kwargs['values'], kwargs.get('data_type'))
        return self._add_attribute(name=kwargs['name'], values=v, data_type=t)

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
    def _add_attribute_wrapper(self, *args, **kwargs) -> Attribute:
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

        if len(args) == 1 and isinstance(args[0], Attribute):
            # If the first argument is an Attribute, we assume it's an existing attribute to add
            return self._add_attribute(attribute=args[0])
        arg_names = ['name', 'entries_values', 'entries_types']
        for i, arg in enumerate(args):
            if i < len(arg_names):
                kwargs[arg_names[i]] = arg
            else:
                raise TypeError(f"Unexpected argument {arg} at position {i+1}. Expected one of {arg_names}.")

        name = kwargs.get('name')
        entries_values = kwargs.get('entries_values')
        entries_types = kwargs.get('entries_types') or [None] * len(entries_values)
        v, t = [list(l) for l in zip(*[_attribute_values_view_and_type(values, data_type)
                                       for values, data_type in zip(entries_values, entries_types)])]
        return self._add_attribute(name=name, entries_values=v, entries_types=t)

    CDF.add_attribute = _add_attribute_wrapper


def _patch_attribute_set_values():
    @overload
    def _attribute_set_values(self: Attribute, entries_values: List[np.ndarray or List[float or int or datetime] or str],
                              entries_types: List[DataType or None] or None = None):
        ...
    @overload
    def _attribute_set_values(self: Attribute, attribute: Attribute):
        ...
    def _attribute_set_values(self, *args, **kwargs):
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
        if len(args) == 1 and isinstance(args[0], Attribute):
            self._set_values(args[0])
        else:
            arg_names = ['entries_values', 'entries_types']
            for i, arg in enumerate(args):
                if i < len(arg_names):
                    kwargs[arg_names[i]] = arg
                else:
                    raise TypeError(f"Unexpected argument {arg} at position {i+1}. Expected one of {arg_names}.")
            entries_values = kwargs.get('entries_values')
            entries_types = kwargs.get('entries_types') or [None] * len(entries_values)
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
    def _attribute_set_value(self, *args, **kwargs):
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
        if len(args) == 1 and isinstance(args[0], VariableAttribute):
            self._set_value(args[0])
        else:
            arg_names = ['value', 'data_type']
            for i, arg in enumerate(args):
                if i < len(arg_names):
                    kwargs[arg_names[i]] = arg
                else:
                    raise TypeError(f"Unexpected argument {arg} at position {i+1}. Expected one of {arg_names}.")
            value = kwargs.get('value')
            data_type = kwargs.get('data_type', None)
            v, t = _attribute_values_view_and_type(value, data_type)
            self._set_value(v, t)

    VariableAttribute.set_value = _attribute_set_value


_patch_add_cdf_attribute()
_patch_add_variable_attribute()
_patch_set_values()
_patch_add_variable()
_patch_attribute_set_values()
_patch_var_attribute_set_value()


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
        A list of variable names to keep or a regex pattern or a callable function that returns True for variables to keep.
    attributes : Union[List[str], str, re.Pattern, Callable[[Attribute], bool]], optional
        A list of attribute names to keep or a regex pattern or a callable function that returns True for attributes to keep.
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
            return lambda x: False
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

def to_datetime64(values):
    """
    to_datetime64

    Parameters
    ----------
    values: Variable or epoch or List[epoch] or numpy.ndarray[epoch] or epoch16 or List[epoch16] or numpy.ndarray[epoch16] or tt2000_t or List[tt2000_t] or numpy.ndarray[tt2000_t]
        input value(s)
to convert to numpy.datetime64

    Returns
    -------
    numpy.ndarray[numpy.datetime64]
    """
    return _pycdfpp.to_datetime64(values)


def to_datetime(values):
    """
    to_datetime

    Parameters
    ----------
    values: Variable or epoch or List[epoch] or epoch16 or List[epoch16] or tt2000_t or List[tt2000_t]
        input value(s)
to convert to datetime.datetime

    Returns
    -------
    List[datetime.datetime]
    """
    return _pycdfpp.to_datetime(values)


def to_tt2000(values):
    """
    to_tt2000

    Parameters
    ----------
    values: datetime.datetime or List[datetime.datetime]
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
    values: datetime.datetime or List[datetime.datetime]
        input value(s)
to convert to CDF epoch

    Returns
    -------
    epoch or List[epoch]
    """
    return _pycdfpp.to_epoch(values)


def to_epoch16(values):
    """
    to_epoch16

    Parameters
    ----------
    values: datetime.datetime or List[datetime.datetime]
        input value(s)
to convert to CDF epoch16

    Returns
    -------
    epoch16 or List[epoch16]
    """
    return _pycdfpp.to_epoch16(values)


def load(file_or_buffer: str or ByteString, iso_8859_1_to_utf8: bool = False, lazy_load: bool = True):
    """
    Load and parse a CDF file.

    Parameters
    ----------
    file_or_buffer : str or ByteString
        Either a filename to be loaded or an in-memory file implementing the Python buffer protocol.
    iso_8859_1_to_utf8 : bool, optional
        Automatically convert Latin-1 characters to their equivalent UTF counterparts when True.
        For CDF files prior to version 3.8, UTF-8 wasn't supported and some CDF files might contain "illegal" Latin-1 characters.
        (Default is False)
    lazy_load : bool, optional
        Controls whether variable values are loaded immediately or only when accessed by the user.
        If True, variables' values are loaded on demand. If False, all variable values are loaded during parsing.
        (Default is True)

    Returns
    -------
    CDF or None
        Returns a CDF object upon successful read.
        If there's an issue with the read, None is returned.
    """
    if type(file_or_buffer) is str:
        return _pycdfpp.load(file_or_buffer, iso_8859_1_to_utf8, lazy_load)
    if lazy_load:
        return _pycdfpp.lazy_load(file_or_buffer, iso_8859_1_to_utf8)
    else:
        return _pycdfpp.load(file_or_buffer, iso_8859_1_to_utf8)


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
    Returns a default padding value for the given CDF data type.
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
        return b'\x00'
    if cdf_type == DataType.CDF_TIME_TT2000:
        return tt2000_t(-9223372036854775807)
    if cdf_type == DataType.CDF_EPOCH:
        return epoch(0.0)
    if cdf_type == DataType.CDF_EPOCH16:
        return epoch16(0.0)
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
