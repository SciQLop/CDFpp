"""cdflib 1.3.16 ``cdfread.CDF`` on pycdfpp.

Values come from ``pycdfpp.load``; the record level details cdflib exposes (variable numbers,
dimension variances, pad values, attribute entry numbers, ...) from the descriptor records, read here.
"""
import hashlib
import io
import mmap
import urllib.request
from functools import cached_property
from pathlib import Path
from typing import Any, Dict, Iterator, List, Optional, Tuple, Union

import numpy as np

import pycdfpp
from cdflib.dataclasses import AEDR, VDR, ADRInfo, AttData, CDFInfo, CDRInfo, GDRInfo, VDRInfo  # noqa: F401

from . import _types
from . import _records
from . import epochs as epoch
from .s3 import S3object  # noqa: F401
from .epochs import CDFepoch as _CDFepoch

__all__ = ["CDF"]

_MAGIC_NUMBERS = {"cdf30001": 3, "cdf26002": 2, "0000ffff": 2}
_BIG_ENDIAN_ENCODINGS = (1, 2, 5, 7, 9, 11, 12, 18)
_NOT_IEEE_ENCODINGS = (3, 14, 15, 20, 21)
_SCOPE_NAMES = {1: "Global", 2: "Variable"}
_VARIABLE_KIND_NAMES = {8: "zVariable", 3: "rVariable"}
_SPARSE_NAMES = {0: "No_sparse", 1: "Pad_sparse", 2: "Prev_sparse"}
_TIME_TYPES = (31, 32, 33)
_RECORD_VARY, _PAD, _COMPRESSED = 1, 2, 4
_ROW_MAJOR, _SINGLE_FILE, _CHECKSUM, _MD5 = 1, 2, 4, 8


def _local_path(path: Union[str, Path]) -> Path:
    path = Path(path).resolve().expanduser()
    if not path.is_file() and not path.suffix:
        path = path.with_suffix(".cdf")
    if not path.is_file():
        raise FileNotFoundError(f"{path} not found")
    return path


def _mapped(path: Path):
    """The file's bytes, read on demand: opening a CDF reads only its descriptor records."""
    with path.open("rb") as file:
        return mmap.mmap(file.fileno(), 0, access=mmap.ACCESS_READ) if path.stat().st_size else b""


def s3_fetchall(obj) -> io.BytesIO:
    return io.BytesIO(obj["Body"].read())


def _fetch_url(url: str) -> bytes:
    return urllib.request.urlopen(urllib.request.Request(url)).read()


def _fetch_s3(url: str) -> bytes:
    try:
        import boto3
    except ImportError:
        raise ImportError("boto3/botocore package not installed")
    bucket, _, key = url[len("s3://"):].partition("/")
    try:
        obj = boto3.client("s3").get_object(Bucket=bucket, Key=key)
    except Exception:  # public buckets refuse signed requests, as in cdflib
        from botocore import UNSIGNED
        from botocore.client import Config
        obj = boto3.client("s3", config=Config(signature_version=UNSIGNED)).get_object(Bucket=bucket, Key=key)
    return s3_fetchall(obj).getvalue()


_REMOTE = {"s3://": ("s3", _fetch_s3), "http://": ("url", _fetch_url), "https://": ("url", _fetch_url)}


def _remote_kind(path) -> Optional[Tuple[str, Any]]:
    return next((kind for prefix, kind in _REMOTE.items() if str(path).startswith(prefix)), None)


def _cdf_version(buffer, path) -> int:
    magic = bytes(buffer[:4]).hex()
    if magic not in _MAGIC_NUMBERS:
        raise OSError(f"{path} is not a CDF file or a non-supported CDF!")
    return _MAGIC_NUMBERS[magic]


def _md5_matches(buffer) -> bool:
    with memoryview(buffer) as content:
        return hashlib.md5(content[:-16]).digest() == content[-16:]


def _key(name: bytes) -> bytes:
    """A name as pycdfpp's variable map holds it: bytes up to the first NUL."""
    return name.partition(b"\x00")[0]


def _decode_string(raw: bytes, encoding: str) -> str:
    return raw.split(b"\x00")[0].decode(encoding, errors="ignore")


def _decode(raw: bytes, cdf_type: _types.CdfType, num_elements: int, byte_order: str, encoding: str):
    """One attribute entry or pad value, as cdflib's _read_data gives it."""
    if _types.is_string(cdf_type):
        return _decode_string(raw[:num_elements], encoding)
    return np.frombuffer(raw, cdf_type.dtype.newbyteorder(byte_order), count=num_elements).astype(cdf_type.dtype)


def _decode_strings(values: np.ndarray, encoding: str) -> np.ndarray:
    strings = [value.decode(encoding, errors="ignore").replace("\x00", "") for value in values.ravel().tolist()]
    return np.array(strings).reshape(values.shape)


def _squeeze_or_scalar(values: np.ndarray):
    values = np.squeeze(values)
    return values[()] if values.ndim == 0 else values


def _unwrap(entry):
    """cdflib's attribute dictionaries give one-element arrays as their element."""
    if isinstance(entry, np.ndarray) and len(entry) == 1:
        return entry[0]
    return entry


def _matches(name: str, wanted: str) -> bool:
    return name.strip().lower() == wanted.strip().lower()


def _first_named(records: List[dict], name: str) -> Optional[dict]:
    return next((record for record in records if _matches(record["Name"], name)), None)


def _index_named(records: List[dict], name: str) -> int:
    return next((index for index, record in enumerate(records) if _matches(record["Name"], name)), -1)


def _numbered(entries: List[dict], number: int) -> Optional[dict]:
    return next((fields for fields in entries if fields["Num"] == number), None)


def _record_shape(vdr: VDR) -> tuple:
    """The varying dimensions. pycdfpp keeps a dimension of size 1 for rVariables that vary along none."""
    if len(vdr.dim_sizes) != len(vdr.dim_vary):  # version 3 rVariables: cdflib lists only the varying sizes
        return tuple(vdr.dim_sizes)
    return tuple(size for size, vary in zip(vdr.dim_sizes, vdr.dim_vary) if vary)


def _checked_record_range(vdr: VDR, startrec: int, endrec: Optional[int]):
    if startrec:
        if startrec < 0:
            raise ValueError("Invalid start record")
        if not vdr.record_vary:
            startrec = 0
        if startrec > vdr.max_rec:
            raise ValueError("Invalid start record")
    if endrec is None:
        return startrec, vdr.max_rec
    if endrec < 0 or endrec > vdr.max_rec or endrec < startrec:
        raise ValueError("Invalid end record")
    return startrec, endrec if vdr.record_vary else 0


class CDF:
    """Read a CDF file, like cdflib.cdfread.CDF."""

    def __init__(self, path: Union[str, Path], validate: bool = False, string_encoding: str = "ascii",
                 s3_read_method: int = 1):
        remote = _remote_kind(path)
        self.ftype, fetch = remote or ("file", None)
        self.file: Union[str, Path] = str(path) if remote else _local_path(path)
        self.string_encoding = string_encoding
        self.compressed_file = None
        self.temp_file: Optional[Path] = None
        self._buffer = fetch(self.file) if fetch else _mapped(self.file)
        self.cdfversion = _cdf_version(self._buffer, self.file)
        self._open(self._buffer, validate)

    def _open(self, buffer, validate: bool) -> None:
        """Read the descriptor records of a CDF held in buffer (bytes, mmap, ...)."""
        self._descriptors = _records.Descriptors(buffer, self.cdfversion)
        self._cdr = self._cdr_info(self._descriptors.cdr)
        if self._cdr.md5 and validate and not _md5_matches(buffer):
            raise OSError("This file fails the md5 checksum.")
        self._check()
        self._byte_order = ">" if self._cdr.encoding in _BIG_ENDIAN_ENCODINGS else "<"

    @cached_property
    def _cdf(self) -> pycdfpp.CDF:
        """Loaded on the first values read: opening reads only descriptor records, as cdflib does.
        Raw string bytes: string_encoding decodes them, as in cdflib."""
        return pycdfpp.load(self._buffer if self.ftype != "file" else self.file, iso_8859_1_to_utf8=False, lazy_load=True)

    @cached_property
    def _zvdrs(self) -> List[dict]:
        return [self._named(vdr) for vdr in self._descriptors.zvdrs]

    @cached_property
    def _rvdrs(self) -> List[dict]:
        return [self._named(vdr) for vdr in self._descriptors.rvdrs]

    @cached_property
    def _adrs(self) -> List[dict]:
        return [self._named(adr) for adr in self._descriptors.adrs]

    def _named(self, record: dict) -> dict:
        """The record with its name decoded as cdflib decodes it, and its raw name as pycdfpp's key."""
        name = record["Name"].rstrip(b"\x00")
        return {**record, "Name": name.decode(self.string_encoding).replace("\x00", ""), "key": name}

    def __del__(self) -> None:
        if isinstance(getattr(self, "_buffer", None), mmap.mmap):
            self._buffer.close()

    def _cdr_info(self, cdr: dict) -> CDRInfo:
        return CDRInfo(
            encoding=cdr["Encoding"],
            copyright_=self._descriptors.copyright.decode(self.string_encoding).replace("\x00", ""),
            version=f"{cdr['Version']}.{cdr['Release']}.{cdr['Increment']}",
            majority=1 if cdr["Flags"] & _ROW_MAJOR else 2,
            format_=bool(cdr["Flags"] & _SINGLE_FILE),
            md5=cdr["Flags"] & (_CHECKSUM | _MD5) == _CHECKSUM | _MD5,
            post25=self.cdfversion == 3 or (cdr["Version"] == 2 and cdr["Release"] >= 5),
        )

    def _check(self) -> None:
        if not self._cdr.format_:
            raise OSError("This package does not support multi-format CDF")
        if self._cdr.encoding in _NOT_IEEE_ENCODINGS:
            raise OSError(f"This package does not support CDFs with encoding {self._cdr.encoding}")

    def __getitem__(self, variable: str) -> Union[str, np.ndarray]:
        return self.varget(variable)

    def __enter__(self) -> "CDF":
        return self

    def __exit__(self, exc_type: Any, exc_val: Any, exc_tb: Any) -> None:
        return

    def cdf_info(self) -> CDFInfo:
        return CDFInfo(
            self.file,
            self._cdr.version,
            self._cdr.encoding,
            "Row_major" if self._cdr.majority == 1 else "Column_major",
            [vdr["Name"] for vdr in self._rvdrs],
            [vdr["Name"] for vdr in self._zvdrs],
            [{adr["Name"]: _SCOPE_NAMES[adr["scope"]]} for adr in self._adrs],
            self._cdr.copyright_,
            self._cdr.md5,
            self._descriptors.gdr["rNumDims"],
            list(self._descriptors.gdr["rDimSizes"]),
            self._descriptors.compressed,
        )

    def varinq(self, variable: str) -> VDRInfo:
        vdr = self.vdr_info(variable)
        return VDRInfo(vdr.name, vdr.variable_number, _VARIABLE_KIND_NAMES[vdr.section_type], vdr.data_type,
                       _types.by_number[vdr.data_type].name, vdr.num_elements, vdr.num_dims, vdr.dim_sizes,
                       _SPARSE_NAMES[vdr.sparse], vdr.max_rec, vdr.record_vary, vdr.dim_vary, vdr.compression_level,
                       vdr.pad, vdr.blocking_factor)

    def attinq(self, attribute: Union[str, int]) -> ADRInfo:
        if isinstance(attribute, str):
            adr = _first_named(self._adrs, attribute)
            if adr is None:
                raise KeyError(f"No attribute {attribute}")
            return self._adr_info(adr)
        if isinstance(attribute, int):
            return self._adr_info(self._adr_by_number(attribute))
        raise ValueError("attribute keyword must be a string or integer")

    def attget(self, attribute: Union[str, int], entry: Optional[Union[str, int]] = None) -> AttData:
        adr = self._adr_for(attribute, entry)
        if adr["scope"] == 1:
            if not isinstance(entry, int):
                raise ValueError('"entry" must be an integer')
            in_z, entry_num = False, entry
        else:
            in_z, entry_num = self._variable_entry(entry)
        entries = self._z_entries(adr) if in_z else self._gr_entries(adr)
        max_entry = adr["MAXzEntries"] if in_z else adr["MAXgrEntries"]
        if entry_num > max_entry:
            raise ValueError("The entry does not exist")
        found = _numbered(entries, entry_num)
        if found is None:
            raise KeyError("The entry does not exist")
        return self._att_data(self._aedr(found))

    def varget(self, variable: Optional[str] = None, epoch: Optional[str] = None,
               starttime: Optional[epoch.epoch_types] = None, endtime: Optional[epoch.epoch_types] = None,
               startrec: int = 0, endrec: Optional[int] = None) -> Union[str, np.ndarray]:
        if isinstance(variable, int) and self._has_both_kinds():
            raise ValueError("This CDF has both r and z variables. Use variable name instead")
        if (starttime is not None or endtime is not None) and (startrec != 0 or endrec is not None):
            raise ValueError("Can't specify both time and record range")
        fields, section_type = self._vdr_fields(variable)
        vdr = self._vdr(fields, section_type)
        if vdr.max_rec < 0:
            return self._no_records(vdr)
        startrec, endrec = _checked_record_range(vdr, startrec, endrec)
        if not vdr.record_vary:
            return self._record_values(_key(fields["key"]), vdr, 0, 1)[0]
        if starttime is not None or endtime is not None:
            records = self._time_records(vdr.name, starttime, endtime, epoch)
            if records is None or len(records) == 0:
                return None
            startrec, endrec = records[0], records[-1]
        return self._record_values(_key(fields["key"]), vdr, startrec, endrec + 1)

    def vdr_info(self, variable: Union[str, int]) -> VDR:
        return self._vdr(*self._vdr_fields(variable))

    def globalattsget(self) -> Dict[str, List[Union[str, np.ndarray]]]:
        return {adr["Name"]: [_unwrap(self._aedr(fields).entry) for fields in self._gr_entries(adr)]
                for adr in self._adrs if adr["scope"] == 1 and adr["NgrEntries"] != 0}

    def varattsget(self, variable: Union[str, int]) -> Dict[str, Union[None, str, np.ndarray]]:
        if isinstance(variable, int) and self._has_both_kinds():
            raise ValueError("This CDF has both r and z variables. Use variable name")
        if isinstance(variable, str):
            found = self._vdr_named(variable)
            if found is None:
                raise ValueError(f"No variable by this name: {variable}")
            fields, section_type = found
            return self._variable_attributes(fields["Num"], section_type == 8)
        if isinstance(variable, int):
            _, section_type = self._vdr_by_number(variable)
            return self._variable_attributes(variable, section_type == 8)
        return None

    def _vdr_fields(self, variable: Union[str, int]) -> Tuple[dict, int]:
        if isinstance(variable, int) and self._has_both_kinds():
            raise ValueError("This CDF has both r and z variables. Use variable name instead")
        if isinstance(variable, str):
            found = self._vdr_named(variable)
            if found is None:
                raise ValueError(f"Variable name '{variable}' not found.")
            return found
        if isinstance(variable, int):
            return self._vdr_by_number(variable)
        raise ValueError("Please set variable keyword equal to the name or number of an variable")

    def _has_both_kinds(self) -> bool:
        return bool(self._zvdrs) and bool(self._rvdrs)

    def _vdr_named(self, name: str) -> Optional[Tuple[dict, int]]:
        """A variable's VDR fields and section type: zVariables first, as cdflib looks them up."""
        for vdrs, section_type in ((self._zvdrs, 8), (self._rvdrs, 3)):
            fields = _first_named(vdrs, name)
            if fields is not None:
                return fields, section_type
        return None

    def _vdr_by_number(self, number: int) -> Tuple[dict, int]:
        vdrs, section_type = (self._zvdrs, 8) if self._zvdrs else (self._rvdrs, 3)
        if not 0 <= number < len(vdrs):
            raise ValueError(f"No variable by this number: {number}")
        return vdrs[number], section_type

    def _vdr(self, fields: dict, section_type: int) -> VDR:
        dim_sizes, dim_vary = self._dimensions(fields, section_type == 8)
        cdf_type = _types.by_number[fields["DataType"]]
        compressed = bool(fields["Flags"] & _COMPRESSED)
        return VDR(data_type=cdf_type.number, section_type=section_type, next_vdr_location=0,
                   variable_number=fields["Num"], head_vxr=0, last_vxr=0, max_rec=fields["MaxRec"],
                   name=fields["Name"], num_dims=len(dim_sizes), dim_sizes=dim_sizes, compression_bool=compressed,
                   compression_level=self._compression_level(fields) if compressed else 0,
                   blocking_factor=fields["BlockingFactor"], dim_vary=dim_vary,
                   record_vary=bool(fields["Flags"] & _RECORD_VARY), num_elements=fields["NumElems"],
                   sparse=fields["SRecords"], pad=self._pad(fields, cdf_type))

    def _dimensions(self, fields: dict, is_z: bool):
        """Dimension sizes and variances as cdflib reports them: it drops the zVariable dimensions that
        don't vary, and the rVariable ones only for version 3 files."""
        varys = fields["DimVarys"]
        if is_z:
            kept = [(size, vary) for size, vary in zip(fields["zDimSizes"], varys) if vary != 0]
            return [size for size, _ in kept], [vary if self.cdfversion == 3 else True for _, vary in kept]
        sizes = self._descriptors.gdr["rDimSizes"]
        if self.cdfversion == 3:
            return [size for size, vary in zip(sizes, varys) if vary != 0], list(varys)
        return list(sizes), [vary != 0 for vary in varys]

    def _compression_level(self, fields: dict) -> int:
        return self._descriptors.compression_parameter(fields["CPRorSPRoffset"])

    def _pad(self, fields: dict, cdf_type: _types.CdfType):
        raw = self._descriptors.bytes_between(*fields["pad"])
        if not fields["Flags"] & _PAD or len(raw) < cdf_type.dtype.itemsize * fields["NumElems"]:
            return None
        return _decode(raw, cdf_type, fields["NumElems"], self._byte_order, self.string_encoding)

    def _record_values(self, key: bytes, vdr: VDR, start: int, stop: int) -> np.ndarray:
        """Records start to stop - 1, in a new array."""
        values = self._cdf[key].values[start:stop]
        cdf_type = _types.by_number[vdr.data_type]
        if _types.is_string(cdf_type):
            values = _decode_strings(values, self.string_encoding)
        else:
            values = values.view(cdf_type.dtype).copy()
        return values.reshape((len(values), *_record_shape(vdr)))

    @staticmethod
    def _no_records(vdr: VDR) -> np.ndarray:
        cdf_type = _types.by_number[vdr.data_type]
        return np.empty((0, *_record_shape(vdr)), np.float64 if _types.is_string(cdf_type) else cdf_type.dtype)

    def _time_variable(self, name: str, epoch: Optional[str]) -> str:
        if epoch is not None or self.varinq(name).Data_Type in _TIME_TYPES:
            return epoch or name
        depend = self.attget("DEPEND_0", name).Data
        if not isinstance(depend, str):
            raise ValueError(f"DEPEND_0 of {name} is not a variable name")
        return depend

    def _time_records(self, name: str, starttime, endtime, epoch: Optional[str]):
        time_variable = self._time_variable(name, epoch)
        if self.varinq(time_variable).Data_Type not in _TIME_TYPES:
            raise ValueError(f"{time_variable}, the time variable of {name}, is not a CDF epoch type")
        return _CDFepoch.findepochrange(self.varget(time_variable), starttime, endtime)

    def _adr_by_number(self, number: int) -> dict:
        if not 0 <= number < len(self._adrs):
            raise KeyError(f"No attribute {number}")
        return self._adrs[number]

    def _adr_for(self, attribute: Union[str, int], entry) -> dict:
        """The attribute attget reads: a string entry names a variable, so it skips global attributes."""
        if isinstance(attribute, str):
            adr = next((adr for adr in self._adrs if _matches(adr["Name"], attribute)
                        and not (isinstance(entry, str) and adr["scope"] == 1)), None)
            if adr is None:
                raise KeyError(f"No attribute {attribute} for entry {entry}")
            return adr
        if isinstance(attribute, int):
            adr = self._adr_by_number(attribute)
            if not isinstance(entry, int):
                raise TypeError(f"{entry} has to be a number.")
            return adr
        raise ValueError("Please set attribute keyword equal to the name or number of an attribute")

    def _variable_entry(self, entry):
        """Whether a variable attribute entry is a zVariable's, and its number."""
        if isinstance(entry, str):
            z_index, r_index = _index_named(self._zvdrs, entry), _index_named(self._rvdrs, entry)
            if z_index == -1 and r_index == -1:
                raise ValueError(f"No variable by this name: {entry}")
            return (True, z_index) if z_index != -1 else (False, r_index)
        if self._has_both_kinds():
            raise ValueError("This CDF has both r and z variables. Use variable name instead")
        return bool(self._zvdrs), entry

    def _adr_info(self, adr: dict) -> ADRInfo:
        return ADRInfo(adr["scope"], 0, adr["num"], adr["NgrEntries"], adr["MAXgrEntries"], adr["NzEntries"],
                       adr["MAXzEntries"], 0, 0, adr["Name"])

    def _gr_entries(self, adr: dict) -> Iterator[dict]:
        return self._descriptors.entries(adr["AgrEDRhead"], adr["NgrEntries"])

    def _z_entries(self, adr: dict) -> Iterator[dict]:
        return self._descriptors.entries(adr["AzEDRhead"], adr["NzEntries"])

    def _aedr(self, fields: dict) -> AEDR:
        cdf_type = _types.by_number[fields["DataType"]]
        value = self._descriptors.bytes_between(fields["tail"], fields["end"])
        entry = _decode(value, cdf_type, fields["NumElements"], self._byte_order, self.string_encoding)
        num_strings = max(fields["NumStrings"], 1) if self.cdfversion == 3 else None
        return AEDR(entry, cdf_type.number, fields["NumElements"], 0, fields["Num"], num_strings)

    @staticmethod
    def _att_data(aedr: AEDR) -> AttData:
        cdf_type = _types.by_number[aedr.data_type]
        if not isinstance(aedr.entry, str):
            return AttData(cdf_type.dtype.itemsize, cdf_type.name, aedr.num_elements, _squeeze_or_scalar(aedr.entry))
        if aedr.num_strings is None:
            return AttData(aedr.num_elements, cdf_type.name, aedr.num_elements, aedr.entry)
        data = np.array(aedr.entry.split("\\N ")) if aedr.num_strings > 1 else aedr.entry
        return AttData(aedr.num_elements, cdf_type.name, aedr.num_strings, data)

    def _variable_attributes(self, variable_number: int, in_z: bool) -> Dict[str, Union[None, str, np.ndarray]]:
        attributes = {}
        for adr in self._adrs:
            if adr["scope"] == 1:
                continue
            entries = self._z_entries(adr) if in_z else self._gr_entries(adr)
            found = _numbered(entries, variable_number)
            if found is not None:
                attributes[adr["Name"]] = _unwrap(self._aedr(found).entry)
        return attributes
