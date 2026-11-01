"""cdflib 1.3.16 ``cdfread.CDF`` on pycdfpp.

Values come from ``pycdfpp.load``; the record level details cdflib exposes (variable numbers,
dimension variances, pad values, attribute entry numbers, ...) come from ``pycdfpp.debug``.
"""
import hashlib
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple, Union

import numpy as np

import pycdfpp
from pycdfpp.debug import for_each_record
from cdflib.dataclasses import AEDR, VDR, ADRInfo, AttData, CDFInfo, CDRInfo, GDRInfo, VDRInfo  # noqa: F401

from . import _types
from . import epochs as epoch
from .epochs import CDFepoch as _CDFepoch

__all__ = ["CDF"]

_MAGIC_NUMBERS = {"cdf30001": 3, "cdf26002": 2, "0000ffff": 2}
_BIG_ENDIAN_ENCODINGS = (1, 2, 5, 7, 9, 11, 12, 18)
_NOT_IEEE_ENCODINGS = (3, 14, 15, 20, 21)
_SCOPES = {"global": 1, "variable": 2, "global (assumed)": 3, "variable (assumed)": 4}
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


def _cdf_version(path: Path) -> int:
    with path.open("rb") as file:
        magic = file.read(4).hex()
    if magic not in _MAGIC_NUMBERS:
        raise OSError(f"{path} is not a CDF file or a non-supported CDF!")
    return _MAGIC_NUMBERS[magic]


def _md5_matches(path: Path) -> bool:
    content = path.read_bytes()
    return hashlib.md5(content[:-16]).digest() == content[-16:]


def _chain(records: Dict[int, dict], head: int, next_field: str, count: int) -> List[dict]:
    chain = []
    for _ in range(count):
        chain.append(records[head])
        head = records[head][next_field]
    return chain


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
        self.file: Union[str, Path] = _local_path(path)
        self.ftype = "file"
        self.string_encoding = string_encoding
        self.cdfversion = _cdf_version(self.file)
        self.compressed_file = None
        self.temp_file: Optional[Path] = None
        self._read_records(for_each_record(str(self.file)))
        self._check(validate)
        # Raw string bytes: string_encoding decodes them, as in cdflib.
        self._cdf = pycdfpp.load(self.file, iso_8859_1_to_utf8=False, lazy_load=True)

    def _read_records(self, records: List[tuple]) -> None:
        self._records = {offset: fields for offset, _, fields in records}
        by_type = {record_type: fields for _, record_type, fields in records}
        self._compressed = "CCR" in by_type
        gdr = by_type["GDR"]
        self._cdr = self._cdr_info(by_type["CDR"])
        self._gdr = self._gdr_info(gdr)
        self._zvdrs = _chain(self._records, gdr["zVDRhead"], "VDRnext", gdr["NzVars"])
        self._rvdrs = _chain(self._records, gdr["rVDRhead"], "VDRnext", gdr["NrVars"])
        self._adrs = _chain(self._records, gdr["ADRhead"], "ADRnext", gdr["NumAttr"])
        self._byte_order = ">" if self._cdr.encoding in _BIG_ENDIAN_ENCODINGS else "<"

    def _cdr_info(self, cdr: dict) -> CDRInfo:
        return CDRInfo(
            encoding=pycdfpp.Encoding.__members__[cdr["Encoding"]].value,
            copyright_=cdr["copyright"].replace("\x00", ""),
            version=f"{cdr['Version']}.{cdr['Release']}.{cdr['Increment']}",
            majority=1 if cdr["Flags"] & _ROW_MAJOR else 2,
            format_=bool(cdr["Flags"] & _SINGLE_FILE),
            md5=cdr["Flags"] & (_CHECKSUM | _MD5) == _CHECKSUM | _MD5,
            post25=self.cdfversion == 3 or (cdr["Version"] == 2 and cdr["Release"] >= 5),
        )

    @staticmethod
    def _gdr_info(gdr: dict) -> GDRInfo:
        return GDRInfo(first_zvariable=0, first_rvariable=0, first_adr=0, num_zvariables=gdr["NzVars"],
                       num_rvariables=gdr["NrVars"], num_attributes=gdr["NumAttr"],
                       rvariables_num_dims=gdr["rNumDims"], rvariables_dim_sizes=list(gdr["rDimSizes"]), eof=0,
                       leapsecond_updated=gdr["LeapSecondLastUpdated"])

    def _check(self, validate: bool) -> None:
        if self._cdr.md5 and validate and not _md5_matches(self.file):
            raise OSError("This file fails the md5 checksum.")
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
            [{adr["Name"]: _SCOPE_NAMES[_SCOPES[adr["scope"]]]} for adr in self._adrs],
            self._cdr.copyright_,
            self._cdr.md5,
            self._gdr.rvariables_num_dims,
            list(self._gdr.rvariables_dim_sizes),
            self._compressed,
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
        if _SCOPES[adr["scope"]] == 1:
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
        vdr = self.vdr_info(variable)
        if vdr.max_rec < 0:
            return self._no_records(vdr)
        startrec, endrec = _checked_record_range(vdr, startrec, endrec)
        if not vdr.record_vary:
            return self._values(vdr)[0].copy()
        if starttime is not None or endtime is not None:
            records = self._time_records(vdr.name, starttime, endtime, epoch)
            if records is None or len(records) == 0:
                return None
            startrec, endrec = records[0], records[-1]
        return self._values(vdr)[startrec:endrec + 1].copy()

    def vdr_info(self, variable: Union[str, int]) -> VDR:
        if isinstance(variable, int) and self._has_both_kinds():
            raise ValueError("This CDF has both r and z variables. Use variable name instead")
        if isinstance(variable, str):
            found = self._vdr_named(variable)
            if found is None:
                raise ValueError(f"Variable name '{variable}' not found.")
            return self._vdr(*found)
        if isinstance(variable, int):
            return self._vdr(*self._vdr_by_number(variable))
        raise ValueError("Please set variable keyword equal to the name or number of an variable")

    def globalattsget(self) -> Dict[str, List[Union[str, np.ndarray]]]:
        return {adr["Name"]: [_unwrap(self._aedr(fields).entry) for fields in self._gr_entries(adr)]
                for adr in self._adrs if _SCOPES[adr["scope"]] == 1 and adr["NgrEntries"] != 0}

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
        cdf_type = _types.by_name[fields["DataType"]]
        compressed = bool(fields["Flags"] & _COMPRESSED)
        return VDR(data_type=cdf_type.number, section_type=section_type, next_vdr_location=0,
                   variable_number=fields["Num"], head_vxr=0, last_vxr=0, max_rec=fields["MaxRec"],
                   name=fields["Name"], num_dims=len(dim_sizes), dim_sizes=dim_sizes, compression_bool=compressed,
                   compression_level=self._records[fields["CPRorSPRoffset"]]["cParms"][0] if compressed else 0,
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
        sizes = self._gdr.rvariables_dim_sizes
        if self.cdfversion == 3:
            return [size for size, vary in zip(sizes, varys) if vary != 0], list(varys)
        return list(sizes), [vary != 0 for vary in varys]

    def _pad(self, fields: dict, cdf_type: _types.CdfType):
        raw = fields["PadValues"]
        if not fields["Flags"] & _PAD or len(raw) < cdf_type.dtype.itemsize * fields["NumElems"]:
            return None
        return _decode(raw, cdf_type, fields["NumElems"], self._byte_order, self.string_encoding)

    def _values(self, vdr: VDR) -> np.ndarray:
        values = self._cdf[vdr.name].values
        cdf_type = _types.by_number[vdr.data_type]
        if _types.is_string(cdf_type):
            values = _decode_strings(values, self.string_encoding)
        else:
            values = values.view(cdf_type.dtype)
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
                        and not (isinstance(entry, str) and _SCOPES[adr["scope"]] == 1)), None)
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
        return ADRInfo(_SCOPES[adr["scope"]], 0, adr["num"], adr["NgrEntries"], adr["MAXgrEntries"], adr["NzEntries"],
                       adr["MAXzEntries"], 0, 0, adr["Name"])

    def _gr_entries(self, adr: dict) -> List[dict]:
        return _chain(self._records, adr["AgrEDRhead"], "AEDRnext", adr["NgrEntries"])

    def _z_entries(self, adr: dict) -> List[dict]:
        return _chain(self._records, adr["AzEDRhead"], "AEDRnext", adr["NzEntries"])

    def _aedr(self, fields: dict) -> AEDR:
        cdf_type = _types.by_name[fields["DataType"]]
        entry = _decode(fields["Value"], cdf_type, fields["NumElements"], self._byte_order, self.string_encoding)
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
            if _SCOPES[adr["scope"]] == 1:
                continue
            entries = self._z_entries(adr) if in_z else self._gr_entries(adr)
            found = _numbered(entries, variable_number)
            if found is not None:
                attributes[adr["Name"]] = _unwrap(self._aedr(found).entry)
        return attributes
