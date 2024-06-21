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


import numpy as np
#from ._pycdfpp import *
from ._pycdfpp import DataType, CompressionType, Majority, Variable, VariableAttribute, Attribute, CDF, tt2000_t, epoch, epoch16, save
from datetime import datetime
from . import _pycdfpp
from typing import ByteString, Mapping, List, Any
import sys
import os
from functools import singledispatch

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
    DataType.CDF_NONE: (DataType.CDF_CHAR, DataType.CDF_UCHAR, DataType.CDF_INT1, DataType.CDF_BYTE, DataType.CDF_UINT1, DataType.CDF_UINT2, DataType.CDF_UINT4, DataType.CDF_INT1, DataType.CDF_INT2, DataType.CDF_INT4, DataType.CDF_INT8, DataType.CDF_FLOAT, DataType.CDF_REAL4, DataType.CDF_DOUBLE, DataType.CDF_REAL8, DataType.CDF_TIME_TT2000, DataType.CDF_EPOCH, DataType.CDF_EPOCH16),
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
    if not np.issubdtype(type1, np.integer) :
        return type2
    if not np.issubdtype(type2, np.integer):
        return type1

    if np.dtype(type1).itemsize > np.dtype(type2).itemsize:
        return type1
    else:
        return type2

    return None

def _min_integer_dtype(values: list, target_type:np.dtype or None=None):
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

def _values_view_and_type(values: np.ndarray or list, data_type:DataType or None=None, target_type:DataType or None=None):
    if type(values) is list:
        if _holds_datetime(values):
            values = np.array(values, dtype="datetime64[ns]")
        else:
            values = np.array(
                values, dtype=_CDF_TYPES_TO_NUMPY_DTYPE_.get(data_type, None))

        if values.dtype.num == 19:
            values = np.char.encode(values, encoding='utf-8')
        elif data_type is None and np.issubdtype(values.dtype, np.integer):
            target_type = _CDF_TYPES_TO_NUMPY_DTYPE_.get(target_type, np.float32)
            if not np.issubdtype(target_type, np.integer):
                target_type = None
            values = values.astype(_min_integer_dtype(values, target_type))
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
    def _set_values_wrapper(self:Variable, values: np.ndarray, data_type:DataType or None=None):
        values, data_type = _values_view_and_type(values, data_type, target_type=self.type)
        print(values, data_type)
        if data_type not in _CDF_TYPES_COMPATIBILITY_TABLE_[self.type]:
            raise ValueError(
                f"Can't set variable of type {self.type} with values of type {data_type}")
        if self.is_nrv:
            if self.shape[1:] != values.shape[1:]:
                if self.shape[1:] != values.shape:
                    raise ValueError(
                        f"Can't set NRV variable of shape {self.shape} with values of shape {values.shape}")
                else:
                    values = values.reshape((1,)+values.shape)
        elif self.type != DataType.CDF_NONE and self.shape[1:] != values.shape[1:]:
            raise ValueError(
                f"Can't set variable of shape {self.shape} with values of shape {values.shape}")
        self._set_values(values, data_type)

    Variable.set_values = _set_values_wrapper


def _patch_add_variable():
    def _add_variable_wrapper(self, name: str, values: np.ndarray or None = None, data_type:DataType or None=None, is_nrv: bool = False,
                              compression: CompressionType = CompressionType.no_compression,
                              attributes: Mapping[str, List[Any]] or None = None):
        """Adds a new variable to the CDF. If values is None, the variable is created with no values. Otherwise, the variable is created with the given values. If attributes is not None, the variable is created with the given attributes.
        If data_type is not None, the variable is created with the given data type. Otherwise, the data type is inferred from the values.

        Parameters
        ----------
        name : str
            The name of the variable to add.
        values : numpy.ndarray or list or None, optional
            The values to set for the variable. If None, the variable is created with no values. Otherwise, the variable is created with the given values. (Default is None)
            When a list is passed, the values are converted to a numpy.ndarray with the appropriate data type, with integers, it will choose the smallest data type that can hold all the values.
        data_type : DataType or None, optional
            The data type of the variable. If None, the data type is inferred from the values. (Default is None)
        is_nrv : bool, optional
            Whether or not the variable is a non-record variable. (Default is False)
        compression : CompressionType, optional
            The compression type to use for the variable. (Default is CompressionType.no_compression)
        attributes : Mapping[str, List[Any]] or None, optional
            The attributes to set for the variable. If None, the variable is created with no attributes. (Default is None)

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
        >>> cdf.add_variable("var1", np.arange(10, dtype=np.int32), DataType.CDF_INT4, compression=CompressionType.gzip_compression)
        var1:
          shape: [ 10 ]
          type: CDF_INT1
          record varry: True
          compression: GNU GZIP
          ...

        """
        if values is not None:
            v, t = _values_view_and_type(values, data_type)
            var = self._add_variable(
                name=name, values=v, data_type=t, is_nrv=is_nrv, compression=compression)
        elif data_type is not None:
            v,t = _values_view_and_type([], data_type)
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
    def _add_attribute_wrapper(self, name: str, values: np.ndarray or List[float or int or datetime] or str, data_type=None):
        """Adds a new attribute to the variable. If values is None, the attribute is created with no values. Otherwise, the attribute is created with the given values.
        If data_type is not None, the attribute is created with the given data type. Otherwise, the data type is inferred from the values.

        Parameters
        ----------
        name : str
            The name of the attribute to add.
        values : np.ndarray or List[float or int or datetime] or str
            The values to set for the attribute.
            When a list is passed, the values are converted to a numpy.ndarray with the appropriate data type, with integers, it will choose the smallest data type that can hold all the values.
        data_type : DataType or None, optional
            The data type of the attribute. If None, the data type is inferred from the values. (Default is None)

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
        >>> cdf = CDF()
        >>> cdf.add_variable("var1", np.arange(10, dtype=np.int32), DataType.CDF_INT4)
        var1:
          shape: [ 10 ]
          type: CDF_INT1
          record varry: True
          compression: None
          ...
        >>> cdf["var1"].add_attribute("attr1", np.arange(10, dtype=np.int32), DataType.CDF_INT4)
        attr1: [ [ [ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 ] ] ]
        """
        v, t = _attribute_values_view_and_type(values, data_type)
        return self._add_attribute(name=name, values=v, data_type=t)

    Variable.add_attribute = _add_attribute_wrapper


def _patch_add_cdf_attribute():
    def _add_attribute_wrapper(self, name: str, entries_values: List[np.ndarray or List[float or int or datetime] or str], entries_types: List[DataType or None] or None = None):
        """Adds a new attribute with the given values to the CDF object.
        If entries_types is not None, the attribute is created with the given data type. Otherwise, the data type is inferred from the values.

        Parameters
        ----------
        name : str
            The name of the attribute to add.
        entries_values : List[np.ndarray or List[float or int or datetime] or str]
            The values entries to set for the attribute.
            When a list is passed, the values are converted to a numpy.ndarray with the appropriate data type, with integers, it will choose the smallest data type that can hold all the values.
        entries_types : List[DataType] or None, optional
            The data type for each entry of the attribute. If None, the data type is inferred from the values. (Default is None)

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
        >>> cdf.add_attribute("attr1", [np.arange(10, dtype=np.int32)], [DataType.CDF_INT4])
        attr1: [ [ [ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 ] ] ]
        >>> cdf.add_attribute("multi", [np.arange(2, dtype=np.int32), [1.,2.,3.], "hello", [datetime(2010,1,1), datetime(2020,1,1)]])

        """
        entries_types = entries_types or [None]*len(entries_values)
        v, t = [list(l) for l in zip(*[_attribute_values_view_and_type(values, data_type)
                                       for values, data_type in zip(entries_values, entries_types)])]
        return self._add_attribute(name=name, entries_values=v, entries_types=t)
    CDF.add_attribute = _add_attribute_wrapper

def _patch_attribute_set_values():
    def _attribute_set_values(self, entries_values: List[np.ndarray or List[float or int or datetime] or str], entries_types: List[DataType or None] or None = None):
        """Sets the values of the attribute with the given values.
        If entries_types is not None, the attribute is created with the given data type. Otherwise, the data type is inferred from the values.

        Parameters
        ----------
        entries_values : List[np.ndarray or List[float or int or datetime] or str]
            The values entries to set for the attribute.
            When a list is passed, the values are converted to a numpy.ndarray with the appropriate data type, with integers, it will choose the smallest data type that can hold all the values.
        entries_types : List[DataType] or None, optional
            The data type for each entry of the attribute. If None, the data type is inferred from the values. (Default is None)

        """
        entries_types = entries_types or [None]*len(entries_values)
        v, t = [list(l) for l in zip(*[_attribute_values_view_and_type(values, data_type)
                                       for values, data_type in zip(entries_values, entries_types)])]
        self._set_values(v, t)
    Attribute.set_values = _attribute_set_values

def _patch_var_attribute_set_value():
    def _attribute_set_value(self, value: np.ndarray or List[float or int or datetime] or str, data_type=None):
        """Sets the value of the attribute with the given value.
        If data_type is not None, the attribute is created with the given data type. Otherwise, the data type is inferred from the values.

        Parameters
        ----------
        values : np.ndarray or List[float or int or datetime] or str
            The values to set for the attribute.
            When a list is passed, the values are converted to a numpy.ndarray with the appropriate data type, with integers, it will choose the smallest data type that can hold all the values.
        data_type : DataType or None, optional
            The data type of the attribute. If None, the data type is inferred from the values. (Default is None)

        """
        v, t = _attribute_values_view_and_type(value, data_type)
        self._set_value(v, t)
    VariableAttribute.set_value = _attribute_set_value

_patch_add_cdf_attribute()
_patch_add_variable_attribute()
_patch_set_values()
_patch_add_variable()
_patch_attribute_set_values()
_patch_var_attribute_set_value()


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

