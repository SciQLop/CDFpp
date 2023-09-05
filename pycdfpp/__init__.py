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
from ._pycdfpp import *
from datetime import datetime
from . import _pycdfpp
from typing import ByteString, Mapping, List, Any
import sys
import os
__here__ = os.path.dirname(os.path.abspath(__file__))
sys.path.append(__here__)
if sys.platform == 'win32' and sys.version_info[0] == 3 and sys.version_info[1] >= 8:
    os.add_dll_directory(__here__)

__all__ = ['tt2000_t', 'epoch', 'epoch16', 'save', 'CDF', 'Variable',
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
    DataType.CDF_NONE: (DataType.CDF_CHAR,DataType.CDF_UCHAR,DataType.CDF_INT1,DataType.CDF_BYTE,DataType.CDF_UINT1,DataType.CDF_UINT2,DataType.CDF_UINT4,DataType.CDF_INT1,DataType.CDF_INT2,DataType.CDF_INT4,DataType.CDF_INT8,DataType.CDF_FLOAT,DataType.CDF_REAL4,DataType.CDF_DOUBLE,DataType.CDF_REAL8,DataType.CDF_TIME_TT2000,DataType.CDF_EPOCH,DataType.CDF_EPOCH16),
    DataType.CDF_CHAR: (DataType.CDF_CHAR,DataType.CDF_UCHAR),
    DataType.CDF_UCHAR: (DataType.CDF_CHAR,DataType.CDF_UCHAR),
    DataType.CDF_BYTE: (DataType.CDF_INT1,DataType.CDF_BYTE),
    DataType.CDF_INT1: (DataType.CDF_INT1,DataType.CDF_BYTE),
    DataType.CDF_UINT1: (DataType.CDF_UINT1,),
    DataType.CDF_INT2: (DataType.CDF_INT2,),
    DataType.CDF_UINT2: (DataType.CDF_UINT2,),
    DataType.CDF_INT4: (DataType.CDF_INT4,),
    DataType.CDF_UINT4: (DataType.CDF_UINT4,),
    DataType.CDF_INT8: (DataType.CDF_INT8,),
    DataType.CDF_FLOAT: (DataType.CDF_FLOAT,DataType.CDF_REAL4),
    DataType.CDF_REAL4: (DataType.CDF_FLOAT,DataType.CDF_REAL4),
    DataType.CDF_DOUBLE: (DataType.CDF_DOUBLE,DataType.CDF_REAL8),
    DataType.CDF_REAL8: (DataType.CDF_DOUBLE,DataType.CDF_REAL8),
    DataType.CDF_TIME_TT2000: (DataType.CDF_TIME_TT2000,),
    DataType.CDF_EPOCH: (DataType.CDF_EPOCH,),
    DataType.CDF_EPOCH16: (DataType.CDF_EPOCH16,),
}

def _holds_datetime(values:list):
    if len(values):
        if type(values[0]) is list:
            return _holds_datetime(values[0])
        if type(values[0]) is datetime:
            return True
    return False


def _values_view_and_type(values: np.ndarray or list, data_type=None):
    if type(values) is list:
        if _holds_datetime(values):
            values = np.array(values, dtype="datetime64[ns]")
        else:
            values = np.array(values)
        if values.dtype.num == 19:
            values = np.char.encode(values, encoding='utf-8')
        return _values_view_and_type(values, data_type)
    else:
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
    def _set_values_wrapper(self, values: np.ndarray, data_type=None):
        values, data_type = _values_view_and_type(values, data_type)
        if data_type not in _CDF_TYPES_COMPATIBILITY_TABLE_[self.type]:
            raise ValueError(
                f"Can't set variable of type {self.type} with values of type {data_type}")
        if self.type != DataType.CDF_NONE and self.shape[1:] != values.shape[1:]:
            raise ValueError(
                f"Can't sat variable of shape {self.shape} with values of shape {values.shape}")
        self._set_values(values, data_type)

    Variable.set_values = _set_values_wrapper


def _patch_add_variable():
    def _add_variable_wrapper(self, name: str, values: np.ndarray or None = None, data_type=None, is_nrv: bool = False,
                              compression: CompressionType = CompressionType.no_compression,
                              attributes: Mapping[str, List[Any]] or None = None):
        """Adds a new variable to the CDF. If values is None, the variable is created with no values. Otherwise, the variable is created with the given values. If attributes is not None, the variable is created with the given attributes.
        If data_type is not None, the variable is created with the given data type. Otherwise, the data type is inferred from the values.

        Parameters
        ----------
        name : str
            The name of the variable to add.
        values : numpy.ndarray or None, optional
            The values to set for the variable. If None, the variable is created with no values. Otherwise, the variable is created with the given values. (Default is None)
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

        Example
        -------
        >>> from pycdfpp import CDF, DataType, CompressionType
        >>> import numpy as np
        >>> cdf = CDF()
        >>> cdf.add_variable("var1", np.arange(10, dtype=np.int32), DataType.CDF_INT4, compression=CompressionType.gzip_compression)
        var1:
          shape: [ 10 ]
          type: CDF_INT1
          record varry: True
          compression: GNU GZIP

          Attributes:

        """
        if values is not None:
            v, t = _values_view_and_type(values, data_type)
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


_patch_set_values()
_patch_add_variable()


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
