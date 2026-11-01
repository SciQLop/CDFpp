"""CDF descriptor records (CDR, GDR, VDR, ADR, AEDR, CPR), read from a file's bytes.

Only the records describing the file are read, never the data records (VXR, VVR, CVVR): the values
come from ``pycdfpp.load``. Record layouts: https://cdf.gsfc.nasa.gov/docs/cdf_internal_format.pdf
(and section "CDF V2.x" for version 2 files). Offsets are from the start of the record, big-endian.
"""
import gzip
import struct
import zlib
from functools import cached_property
from typing import Iterator, List, NamedTuple, Optional, Tuple

# One layout per record type and version: (field, offset, struct format). "tail" is where the
# variable length part starts (copyright, dimension sizes, pad value, entry value, compressed data).
_V3 = {
    "CDR": (("GDRoffset", 12, "q"), ("Version", 20, "i"), ("Release", 24, "i"), ("Encoding", 28, "i"),
            ("Flags", 32, "i"), ("Increment", 44, "i"), ("tail", 56, None)),
    "GDR": (("rVDRhead", 12, "q"), ("zVDRhead", 20, "q"), ("ADRhead", 28, "q"), ("NrVars", 44, "i"),
            ("NumAttr", 48, "i"), ("rNumDims", 56, "i"), ("NzVars", 60, "i"), ("tail", 84, None)),
    "VDR": (("VDRnext", 12, "q"), ("DataType", 20, "i"), ("MaxRec", 24, "i"), ("Flags", 44, "i"),
            ("SRecords", 48, "i"), ("NumElems", 64, "i"), ("Num", 68, "i"), ("CPRorSPRoffset", 72, "q"),
            ("BlockingFactor", 80, "i"), ("Name", 84, "256s"), ("tail", 340, None)),
    "ADR": (("ADRnext", 12, "q"), ("AgrEDRhead", 20, "q"), ("scope", 28, "i"), ("num", 32, "i"),
            ("NgrEntries", 36, "i"), ("MAXgrEntries", 40, "i"), ("AzEDRhead", 48, "q"), ("NzEntries", 56, "i"),
            ("MAXzEntries", 60, "i"), ("Name", 68, "256s")),
    "AEDR": (("AEDRnext", 12, "q"), ("DataType", 24, "i"), ("Num", 28, "i"), ("NumElements", 32, "i"),
             ("NumStrings", 36, "i"), ("tail", 56, None)),
    "CPR": (("cType", 12, "i"), ("cParm", 24, "i")),
    "CCR": (("CPRoffset", 12, "q"), ("tail", 32, None)),
}
_V2 = {
    "CDR": (("GDRoffset", 8, "i"), ("Version", 12, "i"), ("Release", 16, "i"), ("Encoding", 20, "i"),
            ("Flags", 24, "i"), ("Increment", 36, "i"), ("tail", 48, None)),
    "GDR": (("rVDRhead", 8, "i"), ("zVDRhead", 12, "i"), ("ADRhead", 16, "i"), ("NrVars", 24, "i"),
            ("NumAttr", 28, "i"), ("rNumDims", 36, "i"), ("NzVars", 40, "i"), ("tail", 60, None)),
    "VDR": (("VDRnext", 8, "i"), ("DataType", 12, "i"), ("MaxRec", 16, "i"), ("Flags", 28, "i"),
            ("SRecords", 32, "i"), ("NumElems", 48, "i"), ("Num", 52, "i"), ("CPRorSPRoffset", 56, "i"),
            ("BlockingFactor", 60, "i"), ("Name", 64, "64s"), ("tail", 128, None)),
    "ADR": (("ADRnext", 8, "i"), ("AgrEDRhead", 12, "i"), ("scope", 16, "i"), ("num", 20, "i"),
            ("NgrEntries", 24, "i"), ("MAXgrEntries", 28, "i"), ("AzEDRhead", 36, "i"), ("NzEntries", 40, "i"),
            ("MAXzEntries", 44, "i"), ("Name", 52, "64s")),
    "AEDR": (("AEDRnext", 8, "i"), ("DataType", 16, "i"), ("Num", 20, "i"), ("NumElements", 24, "i"),
             ("tail", 48, None)),
    "CPR": (("cType", 8, "i"), ("cParm", 20, "i")),
    "CCR": (("CPRoffset", 8, "i"), ("tail", 20, None)),
}
# Before version 2.5, VDRs had 128 more reserved bytes before NumElems.
_V2_PRE_25_VDR = tuple((name, offset + 128 if offset >= 48 else offset, fmt) for name, offset, fmt in _V2["VDR"])
_RECORD_SIZE_FORMAT = {3: "q", 2: "i"}
_GZIP, _RLE = 5, 1
_UNCOMPRESSED_MAGIC = bytes.fromhex("0000ffff")


def _uncompress_rle(data: bytes) -> bytes:
    """CDF run-length encoding: a zero byte followed by n stands for n + 1 zeros."""
    out = bytearray()
    index = 0
    while index < len(data):
        if data[index] == 0:
            index += 1
            out += b"\x00" * (data[index] + 1)
        else:
            out.append(data[index])
        index += 1
    return bytes(out)


_UNCOMPRESS = {_GZIP: gzip.decompress, _RLE: _uncompress_rle}


def _unpacked(unpacker: struct.Struct, buffer, offset: int) -> tuple:
    """A truncated or corrupted file raises ValueError, as in cdflib."""
    try:
        return unpacker.unpack_from(buffer, offset)
    except struct.error as error:
        raise ValueError(f"Truncated or corrupted CDF: no {unpacker.size} bytes at offset {offset}") from error


class _Layout(NamedTuple):
    """A layout compiled to one struct: a record is read with a single unpack."""
    unpacker: struct.Struct
    names: Tuple[str, ...]
    tail: Optional[int]


def _compiled(layout: tuple, size_format: str) -> _Layout:
    fields = (("size", 0, size_format),) + tuple(field for field in layout if field[2] is not None)
    fmt, position = ">", 0
    for _, at, field_format in fields:
        fmt += f"{at - position}x{field_format}"
        position = at + struct.calcsize(">" + field_format)
    tail = next((at for _, at, field_format in layout if field_format is None), None)
    return _Layout(struct.Struct(fmt), tuple(name for name, _, _ in fields), tail)


_LAYOUTS = {version: {record_type: _compiled(layout, _RECORD_SIZE_FORMAT[version])
                      for record_type, layout in layouts.items()}
            for version, layouts in ((3, _V3), (2, _V2))}
_V2_PRE_25_LAYOUTS = {**_LAYOUTS[2], "VDR": _compiled(_V2_PRE_25_VDR, _RECORD_SIZE_FORMAT[2])}


class Descriptors:
    """The descriptor records of a CDF held in ``buffer`` (bytes, mmap, ...). Version is 2 or 3."""

    def __init__(self, buffer, version: int):
        self.version = version
        self._layouts = _LAYOUTS[version]
        self.compressed = buffer[4:8] != _UNCOMPRESSED_MAGIC
        self._buffer = self._uncompressed(buffer) if self.compressed else buffer
        self.cdr = self._record(8, "CDR")
        if version == 2 and not (self.cdr["Version"] == 2 and self.cdr["Release"] >= 5):
            self._layouts = _V2_PRE_25_LAYOUTS
        self.gdr = self._record(self.cdr["GDRoffset"], "GDR")
        self.gdr["rDimSizes"] = self._ints(self.gdr["tail"], self.gdr["rNumDims"])

    def _uncompressed(self, buffer) -> bytes:
        """The whole file, as if it were never compressed: what the CCR holds, after the magic numbers."""
        ccr = self._record(8, "CCR", buffer)
        codec = self._record(ccr["CPRoffset"], "CPR", buffer)["cType"]
        if codec not in _UNCOMPRESS:
            raise OSError("Decompression was unsuccessful.  Only GZIP compression is currently supported.")
        try:
            content = _UNCOMPRESS[codec](bytes(buffer[ccr["tail"]:ccr["end"]]))
        except (EOFError, IndexError, zlib.error) as error:
            raise ValueError("Truncated or corrupted compressed CDF") from error
        return bytes(buffer[:4]) + _UNCOMPRESSED_MAGIC + content

    def _record(self, offset: int, record_type: str, buffer=None) -> dict:
        layout = self._layouts[record_type]
        values = _unpacked(layout.unpacker, self._buffer if buffer is None else buffer, offset)
        record = dict(zip(layout.names, values))
        record["end"] = offset + record.pop("size")
        if layout.tail is not None:
            record["tail"] = offset + layout.tail
        return record

    def _ints(self, offset: int, count: int) -> List[int]:
        return list(_unpacked(struct.Struct(f">{count}i"), self._buffer, offset))

    def _chain(self, head: int, next_field: str, count: int, record_type: str) -> Iterator[dict]:
        for _ in range(count):
            record = self._record(head, record_type)
            yield record
            head = record[next_field]

    @property
    def content(self):
        """The whole file, uncompressed."""
        return self._buffer

    def bytes_between(self, start: int, end: int) -> bytes:
        return bytes(self._buffer[start:end])

    @cached_property
    def copyright(self) -> bytes:
        return self.bytes_between(self.cdr["tail"], self.cdr["end"])

    @cached_property
    def zvdrs(self) -> List[dict]:
        return [self._with_z_dimensions(vdr) for vdr in self._chain(self.gdr["zVDRhead"], "VDRnext",
                                                                   self.gdr["NzVars"], "VDR")]

    @cached_property
    def rvdrs(self) -> List[dict]:
        return [self._with_r_dimensions(vdr) for vdr in self._chain(self.gdr["rVDRhead"], "VDRnext",
                                                                   self.gdr["NrVars"], "VDR")]

    @cached_property
    def adrs(self) -> List[dict]:
        return list(self._chain(self.gdr["ADRhead"], "ADRnext", self.gdr["NumAttr"], "ADR"))

    def _with_z_dimensions(self, vdr: dict) -> dict:
        count = self._ints(vdr["tail"], 1)[0]
        sizes, varys = self._ints(vdr["tail"] + 4, count), self._ints(vdr["tail"] + 4 + 4 * count, count)
        return {**vdr, "zDimSizes": sizes, "DimVarys": varys, "pad": (vdr["tail"] + 4 + 8 * count, vdr["end"])}

    def _with_r_dimensions(self, vdr: dict) -> dict:
        count = self.gdr["rNumDims"]
        return {**vdr, "DimVarys": self._ints(vdr["tail"], count), "pad": (vdr["tail"] + 4 * count, vdr["end"])}

    def entries(self, head: int, count: int) -> Iterator[dict]:
        """An attribute's chain of entries (AEDRs), read as iterated; a value lies between "tail" and "end"."""
        return self._chain(head, "AEDRnext", count, "AEDR")

    def compression_parameter(self, cpr_offset: int) -> int:
        return self._record(cpr_offset, "CPR")["cParm"]
