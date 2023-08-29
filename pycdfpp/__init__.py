"""
pycdfpp
-------
.. currentmodule:: pycdfpp

.. toctree::
    :maxdepth: 3

#..automodule::pycdfpp._pycdfpp
# : members:
# : undoc - members:
# : show - inheritance:

Indices and tables
------------------
* :ref:`genindex`
* :ref:`modindex`
* :ref:`search`
"""


import numpy as np
from ._pycdfpp import *
from . import _pycdfpp
from typing import ByteString, Mapping, List, Any
import sys
import os
__here__ = os.path.dirname(os.path.abspath(__file__))
sys.path.append(__here__)
if sys.platform == 'win32' and sys.version_info[0] == 3 and sys.version_info[1] >= 8:
    os.add_dll_directory(__here__)

__all__ = ['tt2000_t', 'epoch', 'epoch16', 'save', 'CDF', 'Variable',
           'Attribute', 'to_datetime64', 'to_datetime']


_NUMPY_TO_CDF_TYPE_ = (
    CDF_NONE,
    CDF_INT1,
    CDF_UINT1,
    CDF_INT2,
    CDF_UINT2,
    CDF_INT4,
    CDF_UINT4,
    CDF_INT8,
    CDF_NONE,
    CDF_NONE,
    CDF_NONE,
    CDF_FLOAT,
    CDF_DOUBLE,
    CDF_NONE,
    CDF_NONE,
    CDF_NONE,
    CDF_NONE,
    CDF_NONE,
    CDF_CHAR,
    CDF_NONE,
    CDF_NONE,
    CDF_TIME_TT2000
)


def _values_view_and_type(values: np.ndarray  or list, cdf_type=None):
    if type(values) is list:
        values = np.array(values)
        if values.dtype.num == 19:
            values = np.char.encode(values)
        return _values_view_and_type(values, cdf_type)
    else:
        if values.dtype.num == 21:
            if cdf_type in (None, CDF_TIME_TT2000, CDF_EPOCH, CDF_EPOCH16):
                return (values.view(np.uint64),
                        cdf_type or CDF_TIME_TT2000)
        else:
            return (values, cdf_type or _NUMPY_TO_CDF_TYPE_[
                values.dtype.num])


def _patch_set_values():
    def _set_values_wrapper(self, values: np.ndarray, cdf_type=None):
        self._set_values(*_values_view_and_type(values, cdf_type))

    Variable.set_values = _set_values_wrapper


def _patch_add_variable():
    def _add_variable_wrapper(self, name: str, values: np.ndarray or None = None, cdf_type=None, is_nrv: bool = False,
                              compression: CDF_compression_type = CDF_compression_type.no_compression,
                              attributes: Mapping[str, List[Any]] or None = None):
        if values is not None:
            v, t = _values_view_and_type(values, cdf_type)
            var = self._add_variable(
                name=name, values=v, cdf_type=t, is_nrv=is_nrv, compression=compression)
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
