"""CDF data types: cdflib's number and name, pycdfpp's DataType and the numpy type of one element."""
from typing import NamedTuple

import numpy as np

from pycdfpp import DataType


class CdfType(NamedTuple):
    number: int
    name: str
    data_type: DataType
    dtype: np.dtype


CDF_TYPES = tuple(
    CdfType(data_type.value, data_type.name, data_type, np.dtype(dtype))
    for data_type, dtype in (
        (DataType.CDF_INT1, "i1"), (DataType.CDF_INT2, "i2"), (DataType.CDF_INT4, "i4"),
        (DataType.CDF_INT8, "i8"), (DataType.CDF_UINT1, "u1"), (DataType.CDF_UINT2, "u2"),
        (DataType.CDF_UINT4, "u4"), (DataType.CDF_REAL4, "f4"), (DataType.CDF_REAL8, "f8"),
        (DataType.CDF_EPOCH, "f8"), (DataType.CDF_EPOCH16, "c16"), (DataType.CDF_TIME_TT2000, "i8"),
        (DataType.CDF_BYTE, "i1"), (DataType.CDF_FLOAT, "f4"), (DataType.CDF_DOUBLE, "f8"),
        (DataType.CDF_CHAR, "S1"), (DataType.CDF_UCHAR, "S1"),
    )
)

by_number = {t.number: t for t in CDF_TYPES}
by_name = {t.name: t for t in CDF_TYPES}
by_data_type = {t.data_type: t for t in CDF_TYPES}


def is_string(cdf_type: CdfType) -> bool:
    return cdf_type.dtype.kind == "S"
