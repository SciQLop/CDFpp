"""cdflib 1.3.16's cdfwrite.CDF on pycdfpp: the file is built in memory and written by close()."""
import warnings
from functools import wraps
from pathlib import Path
from typing import Any, Dict, List, NamedTuple, Optional, Union

import numpy as np

import pycdfpp
from pycdfpp import CompressionType, DataType, SparseRecords

from . import epochs as cdfepoch
from . import _types
from .logging import logger

__all__ = ["CDF"]


def is_open(func):
    @wraps(func)
    def ensure_open(self, *args, **kwargs):
        if self.is_closed:
            raise OSError("This file is already closed, and can no longer be modified.")
        return func(self, *args, **kwargs)

    return ensure_open


_MAJORITIES = {"ROW_MAJOR": 1, "COLUMN_MAJOR": 2}
_HOST_ENCODING = 8
_ENCODINGS = {**{f"{e.name.upper()}_ENCODING": e.value for e in pycdfpp.Encoding}, "HOST_ENCODING": _HOST_ENCODING}
_SPARSE = {"no_sparse": SparseRecords.no_sparse_records, "pad_sparse": SparseRecords.pad_sparse_records,
           "prev_sparse": SparseRecords.prev_sparse_records}
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


_ENCODED_DTYPES = {data_type: _encoded_dtype(data_type) for data_type in _RAW_TIMES}


class _Variable(NamedTuple):
    name: str
    cdf_type: _types.CdfType
    num_elements: int
    rec_vary: bool
    is_z: bool
    dims: List[int]
    dim_vary: list


def _is_sequence(value) -> bool:
    return hasattr(value, "__len__") and not isinstance(value, (str, bytes))


def _majority(value) -> int:
    major = _MAJORITIES.get(value.upper(), 0) if isinstance(value, str) else value
    if major not in (1, 2):
        raise RuntimeError(f"Bad major: {value}")
    return major


def _set_encoding(cdf: pycdfpp.CDF, value):
    """Host encoding is IBMPC, as in cdflib. pycdfpp refuses the encodings without IEEE 754 floats."""
    number = _ENCODINGS.get(value.upper(), 0) if isinstance(value, str) else value
    try:
        cdf.encoding = pycdfpp.Encoding(pycdfpp.Encoding.IBMPC.value if number == _HOST_ENCODING else number)
    except ValueError as error:
        raise OSError(f"Bad encoding: {value}") from error


def _compression_level(value) -> int:
    if isinstance(value, int):
        return value if 0 <= value <= 9 else 0
    return 6 if value else 0


def _new_file_path(path, delete: bool) -> Path:
    path = Path(path).expanduser()
    if path.suffix != ".cdf":
        path = path.with_suffix(".cdf")
    if len(str(path)) > CDF.CDF_PATHNAME_LEN:
        raise OSError(f"CDF: {path} longer than allowed length.")
    if path.is_file() and not delete:
        raise OSError(f"file: {path} already exists. Delete it or pass delete=True.")
    return path


def _cdf_type(data_type) -> _types.CdfType:
    """A CDF type given as cdflib's number or as its name."""
    if isinstance(data_type, str):
        found = _types.by_name.get(data_type.upper())
    else:
        found = _types.by_number.get(int(data_type))
    if found is None:
        raise ValueError(f"Bad data type: {data_type}")
    return found


def _required_fields(var_spec: dict):
    try:
        return var_spec["Variable"], _cdf_type(var_spec["Data_Type"]), int(var_spec["Num_Elements"]), \
            bool(var_spec["Rec_Vary"])
    except Exception as error:
        raise ValueError("Missing/invalid required spec for creating variable.") from error


def _check_num_elements(cdf_type: _types.CdfType, num_elements: int):
    if _types.is_string(cdf_type) and num_elements < 1:
        raise ValueError("Invalid Num_Elements for string data type variable")
    if not _types.is_string(cdf_type) and num_elements != 1:
        raise ValueError("Invalid Num_Elements for numeric data type variable")


def _z_dims(var_spec: dict):
    try:
        dims = [int(size) for size in var_spec["Dim_Sizes"]]
    except Exception as error:
        raise ValueError("Missing/invalid required spec for creating variable.") from error
    return dims, [True] * len(dims)


def _r_dims(var_spec: dict, rdim_sizes: List[int]):
    """An rVariable keeps its varying dimensions only: that is all the data it holds."""
    try:
        dim_vary = list(var_spec["Dim_Vary"])
        if len(dim_vary) != len(rdim_sizes):
            raise ValueError("Invalid Dim_Vary size for the rVariable.")
    except Exception as error:
        raise ValueError("Missing/invalid required spec for Dim_Vary for rVariable") from error
    return [size for size, vary in zip(rdim_sizes, dim_vary) if vary], dim_vary


def _variable(var_spec: dict, rdim_sizes: List[int]) -> _Variable:
    name, cdf_type, num_elements, rec_vary = _required_fields(var_spec)
    is_z = var_spec.setdefault("Var_Type", "zvariable").lower() == "zvariable"
    if not is_z:
        var_spec["Var_Type"] = "rVariable"
    _check_num_elements(cdf_type, num_elements)
    dims, dim_vary = _z_dims(var_spec) if is_z else _r_dims(var_spec, rdim_sizes)
    return _Variable(name, cdf_type, num_elements, rec_vary, is_z, dims, dim_vary)


def _text(value) -> str:
    """cdflib joins several strings of one attribute entry with NASA's "\\N " separator."""
    if value is None:
        return ""
    if _is_sequence(value):
        return "\\N ".join(_text(item) for item in value)
    return value.decode("utf-8") if isinstance(value, bytes) else str(value)


def _numbers(values, dtype: np.dtype) -> np.ndarray:
    """values cast to dtype as cdflib does (C casts); numbers written as text are parsed first."""
    values = np.asarray(values)
    if values.dtype.kind in "US":
        values = values.astype(np.float64)
    return values.astype(dtype)


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
    return [wrap(raw) for raw in values.astype(raw_dtype)]


def _entry_value(value, cdf_type: _types.CdfType):
    if _types.is_string(cdf_type):
        return _text(value)
    if cdf_type.data_type in _RAW_TIMES:
        return _time_entry(value, cdf_type.data_type)
    return np.atleast_1d(_numbers(value, cdf_type.dtype))


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


def _attribute_entry(value):
    """An attribute entry as pycdfpp takes it, (value, DataType), or None when its type is unknown."""
    data, cdf_type = _named_type(value)
    cdf_type = cdf_type or _guessed_type(data)
    if cdf_type is None:
        return None
    return _entry_value(data, cdf_type), cdf_type.data_type


def _pad_value(pad, variable: _Variable):
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
        return _numbers(values, cdf_type.dtype)
    if values.dtype.kind == "M":
        return values
    return np.ascontiguousarray(_numbers(values, cdf_type.dtype)).view(_ENCODED_DTYPES[cdf_type.data_type])


def _records(data, variable: _Variable) -> np.ndarray:
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


def _sparse_values(var_data, variable: _Variable):
    if not isinstance(var_data, (list, tuple)) or len(var_data) != 2:
        logger.warning("Sparse record #s and data are not of list/tuple form:\n"
                       " [ [rec_#1, rec_#2, rec_#3,    ],\n   [data_#1, data_#2, data_#3, ....] ]")
        return _records(None, variable), None
    return _with_virtual_records(var_data[0], _records(var_data[1], variable))


class CDF:
    """
    Creates an empty CDF file, as cdflib.cdfwrite.CDF does.

    The file is built in memory and written by close(), or at the end of a with block that
    raises no exception. cdf_spec keys: Majority, Encoding, Checksum, rDim_sizes, Compressed.
    """

    version, release, increment = 3, 9, 0
    CDF_VAR_NAME_LEN256 = 256
    CDF_ATTR_NAME_LEN256 = 256
    CDF_COPYRIGHT_LEN = 256
    CDF_PATHNAME_LEN = 512

    CDF_INT1, CDF_INT2, CDF_INT4, CDF_INT8 = 1, 2, 4, 8
    CDF_UINT1, CDF_UINT2, CDF_UINT4 = 11, 12, 14
    CDF_REAL4, CDF_REAL8 = 21, 22
    CDF_EPOCH, CDF_EPOCH16, CDF_TIME_TT2000 = 31, 32, 33
    CDF_BYTE, CDF_FLOAT, CDF_DOUBLE = 41, 44, 45
    CDF_CHAR, CDF_UCHAR = 51, 52

    NETWORK_ENCODING, SUN_ENCODING, VAX_ENCODING, DECSTATION_ENCODING = 1, 2, 3, 4
    SGi_ENCODING, IBMPC_ENCODING, IBMRS_ENCODING, HOST_ENCODING = 5, 6, 7, 8
    PPC_ENCODING, HP_ENCODING, NeXT_ENCODING, ALPHAOSF1_ENCODING = 9, 11, 12, 13
    ALPHAVMSd_ENCODING, ALPHAVMSg_ENCODING, ALPHAVMSi_ENCODING = 14, 15, 16
    ARM_LITTLE_ENCODING, ARM_BIG_ENCODING = 17, 18

    VARY, NOVARY = -1, 0
    ROW_MAJOR, COLUMN_MAJOR = 1, 2
    NO_CHECKSUM, MD5_CHECKSUM, OTHER_CHECKSUM = 0, 1, 2
    GLOBAL_SCOPE, VARIABLE_SCOPE = 1, 2
    GZIP_COMPRESSION = 5
    NO_SPARSERECORDS, PAD_SPARSERECORDS, PREV_SPARSERECORDS = 0, 1, 2

    V3magicNUMBER_1 = "cdf30001"
    V3magicNUMBER_2 = "0000ffff"
    V3magicNUMBER_2c = "cccc0001"
    CDR_, GDR_, rVDR_, ADR_, AgrEDR_, VXR_, VVR_ = 1, 2, 3, 4, 5, 6, 7
    zVDR_, AzEDR_, CCR_, CPR_, SPR_, CVVR_ = 8, 9, 10, 11, 12, 13
    NUM_VXR_ENTRIES = 7
    NUM_VXRlvl_ENTRIES = 3

    UUIR_BASE_SIZE64 = 12
    UIR_BASE_SIZE64 = 28
    CDR_BASE_SIZE64 = 56
    GDR_BASE_SIZE64 = 84
    zVDR_BASE_SIZE64 = 88 + CDF_VAR_NAME_LEN256
    rVDR_BASE_SIZE64 = 84 + CDF_VAR_NAME_LEN256
    VXR_BASE_SIZE64 = 28
    VVR_BASE_SIZE64 = 12
    ADR_BASE_SIZE64 = 68 + CDF_ATTR_NAME_LEN256
    AEDR_BASE_SIZE64 = 56
    CCR_BASE_SIZE64 = 32
    CPR_BASE_SIZE64 = 24
    SPR_BASE_SIZE64 = 24
    CVVR_BASE_SIZE64 = 24

    BLOCKING_BYTES = 65536

    level = 0

    def __init__(self, path: Union[str, Path], cdf_spec: Optional[Dict[str, Any]] = None, delete: bool = False):
        spec = cdf_spec or {}
        self._cdf = pycdfpp.CDF()
        self.majority = _majority(spec.get("Majority", 1))
        self._cdf.majority = pycdfpp.Majority.row if self.majority == 1 else pycdfpp.Majority.column
        _set_encoding(self._cdf, spec.get("Encoding", _HOST_ENCODING))
        self.checksum = spec.get("Checksum", False)
        if not isinstance(self.checksum, bool):
            raise ValueError("Bad checksum.")
        self._cdf.checksum = pycdfpp.Checksum.md5_checksum if self.checksum else pycdfpp.Checksum.no_checksum
        self.compression = _compression_level(spec.get("Compressed", 0))
        if self.compression:
            self._cdf.compression = CompressionType.gzip_compression
            self._cdf.compression_level = int(self.compression)
        self.rdim_sizes: Optional[List[int]] = spec.get("rDim_sizes", None)
        self.num_rdim = len(self.rdim_sizes) if self.rdim_sizes is not None else 0
        self.path = _new_file_path(path, delete)
        # No file exists before close(): no temporary file, no record offsets.
        self.compressed_file = None
        self.cdr_head = self.gdr_head = self.offset = 0
        self.zvarsinfo: Dict[int, tuple] = {}
        self.rvarsinfo: Dict[int, tuple] = {}
        self.attrsinfo: Dict[int, tuple] = {}
        self.gattrs: List[str] = []
        self.vattrs: List[str] = []
        self.attrs: List[str] = []
        self.zvars: List[str] = []
        self.rvars: List[str] = []
        self.is_closed = False

    def __del__(self):
        if not getattr(self, "is_closed", True):
            warnings.warn(f"{self.path} was never closed: nothing was written", ResourceWarning, stacklevel=2)

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        if exc_type is None:
            self.close()
        else:
            self.is_closed = True

    def close(self) -> None:
        """Writes the file. Calling it again does nothing."""
        if self.is_closed:
            return
        self._cdf.declared_variable_attributes = list(self.vattrs)
        pycdfpp.save(self._cdf, self.path)
        self.is_closed = True

    @is_open
    def write_globalattrs(self, globalAttrs):
        """
        Writes global attributes: {name: {entry number: value or [value, 'CDF_TYPE']}}.
        """
        if not isinstance(globalAttrs, dict):
            raise ValueError("Global attribute(s) not in dictionary form")
        for name, entries in globalAttrs.items():
            if name in self.gattrs:
                raise ValueError(f"Global attribute: {name} already exists.")
            if name in self.vattrs:
                logger.warning(f"Attribute: {name} already defined as a variable attribute.")
                continue
            self._declare_attribute(name, is_global=True)
            converted = [_attribute_entry(value) for _, value in sorted((entries or {}).items())]
            kept = [entry for entry in converted if entry is not None]
            self._cdf.add_attribute(name, [value for value, _ in kept], [data_type for _, data_type in kept])

    @is_open
    def write_variableattrs(self, variableAttrs):
        """
        Writes variable attributes: {name: {variable name or number: value or [value, 'CDF_TYPE']}}.
        """
        if not isinstance(variableAttrs, dict):
            raise ValueError("Variable attribute(s) not in dictionary form")
        for name, entries in variableAttrs.items():
            if not isinstance(name, str):
                raise ValueError("Attribute name must be a string")
            if name in self.gattrs:
                raise ValueError(f"Variable attribute: {name} is already a global variable")
            self._declare_attribute(name, is_global=False)
            if entries is None:
                continue
            if not isinstance(entries, dict):
                raise ValueError("An attribute's attribute(s) not in dictionary form")
            for variable, value in entries.items():
                self._set_variable_attribute(self._variable_name(variable), name, value)

    @is_open
    def write_var(self, var_spec, var_attrs=None, var_data=None):
        """
        Writes a variable, its attributes and its data, as cdflib's write_var does.
        """
        if not isinstance(var_spec, dict):
            raise TypeError("Variable should be in dictionary form.")
        variable = _variable(var_spec, self.rdim_sizes or [])
        sparse = _SPARSE.get(str(var_spec.get("Sparse", "no_sparse")).lower(), SparseRecords.no_sparse_records)
        level = _compression_level(var_spec.get("Compress", 6))
        pad = var_spec.get("Pad", None)
        if variable.name in self.zvars or variable.name in self.rvars:
            raise ValueError(f"{variable.name} already exists")
        if sparse == SparseRecords.no_sparse_records or var_data is None:
            values, virtual = _records(var_data, variable), None
        else:
            values, virtual = _sparse_values(var_data, variable)
        self._cdf.add_variable(variable.name, values, variable.cdf_type.data_type, is_nrv=not variable.rec_vary,
                               compression=CompressionType.gzip_compression if level else CompressionType.no_compression,
                               compression_level=int(level) or 6, sparse_records=sparse,
                               pad_value=_pad_value(pad, variable), virtual_records=virtual)
        self._register_variable(variable)
        if var_attrs is not None:
            self._write_var_attrs(variable.name, var_attrs)

    def _register_variable(self, variable: _Variable):
        names, info = (self.zvars, self.zvarsinfo) if variable.is_z else (self.rvars, self.rvarsinfo)
        dims = variable.dims if variable.is_z else self.rdim_sizes
        info[len(names)] = (variable.name, 0, len(dims or []), dims, variable.dim_vary)
        names.append(variable.name)

    def _declare_attribute(self, name: str, is_global: bool):
        if name in self.attrs:
            return
        self.attrsinfo[len(self.attrs)] = (name, self.GLOBAL_SCOPE if is_global else self.VARIABLE_SCOPE, 0)
        self.attrs.append(name)
        (self.gattrs if is_global else self.vattrs).append(name)

    def _write_var_attrs(self, variable: str, var_attrs: Dict[str, Any]):
        if not isinstance(var_attrs, dict):
            raise TypeError("Variable attribute(s) should be in dictionary form.")
        for name, value in var_attrs.items():
            if name in self.gattrs:
                logger.warning(f"Attribute: {name} already defined as a global attribute... Skipping attribute.")
                continue
            self._declare_attribute(name, is_global=False)
            self._set_variable_attribute(variable, name, value)

    def _variable_name(self, variable) -> str:
        """A variable named, or numbered among the zVariables (or the rVariables when there are none)."""
        if isinstance(variable, str):
            if variable not in self.zvars and variable not in self.rvars:
                raise KeyError(f"{variable} not found in the CDF")
            return variable
        if self.zvars and self.rvars:
            raise ValueError(f"{variable} can not be used as the CDF has both zVariables and rVariables")
        names = self.zvars or self.rvars
        if not 0 <= int(variable) < len(names):
            raise ValueError(f"Variable id: {variable} not found")
        return names[int(variable)]

    def _set_variable_attribute(self, variable: str, name: str, value):
        entry = _attribute_entry(value)
        if entry is None:
            return
        attributes = self._cdf[variable].attributes
        if name in attributes:
            attributes[name].set_value(*entry)
        else:
            self._cdf[variable].add_attribute(name, *entry)
