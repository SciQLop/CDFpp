#!/usr/bin/env python3
"""Builds pathological_times.cdf: one Epoch/Epoch16/TT2000 variable each,
carrying every known-or-imaginable time value that risks crashing
repr()/str() in include/cdfpp/cdf-repr.hpp - not just the fill/pad
sentinels, but also values that fall through those exact-literal checks.

Real-world trigger (2026-09): a JASON3 CDF's Epoch variable had
VALIDMIN=1950-01-01 (a genuine pre-1970 date) as a plain VALIDMIN
attribute. str() on it threw "time_t value out of range" on Windows only,
because the old repr code formatted via fmt::format(), which calls the
platform's gmtime() - glibc accepts negative time_t, the Windows CRT
rejects it outright. Fixed by dropping the gmtime()/fmt::chrono dependency
entirely (see cdf-repr.hpp's _repr_details::civil_from_days).

Each variable gets standard FILLVAL/VALIDMIN/VALIDMAX attributes (the
exact shape that crashed originally: a plain attribute entry on the time
variable itself), plus a PATHOLOGICAL_CASES attribute holding every case
below as one multi-element entry - the same "attribute entry is a list of
scalars" shape Speasy's _fix_value_type() recurses through - so a single
str()/repr() sweep over that list exercises everything at once.

Only FILL/PAD/PRE_1970/SAFE_2100 have a platform-independent expected
string (asserted by the accompanying tests). The others deliberately feed
values that overflow the internal int64-nanosecond time_point (a separate,
known, NOT-yet-fixed bug in cdf::to_time_point()'s double/int64 casts) -
they must never crash, but what they print depends on hardware-specific
out-of-range float->int conversion behavior (x86 SSE vs. ARM NEON differ),
so no exact string is asserted for them.
"""
import math
import pycdfpp as p
import numpy as np

# Canonical reference points, cross-derived from ONE common time_point via
# CDFpp's own to_time_point()/to_cdf_time<T>() so all three types agree
# exactly on "1950-01-01T00:00:00Z" and "2100-12-31T23:59:59.999Z".
# EPOCH_PRE_1970 also matches the real JASON3 file's VALIDMIN bit-for-bit.
EPOCH_PRE_1970 = 61536067200000.0
EPOCH_SAFE_2100 = 66301199999999.0
EPOCH16_PRE_1970 = (61536067200.0, 0.0)
EPOCH16_SAFE_2100 = (66301199999.0, 999000000000.0)
TT2000_PRE_1970 = -1577879967816000000
TT2000_SAFE_2100 = 3187252869183000000

INT64_MIN = -9223372036854775808
INT64_MAX = 9223372036854775807
DBL_MAX = 1.7976931348623157e308

EPOCH_CASES = [
    ("FILL", p.epoch(-1e31)),
    ("PAD_YEAR0", p.epoch(0.0)),
    ("PRE_1970", p.epoch(EPOCH_PRE_1970)),
    ("SAFE_2100", p.epoch(EPOCH_SAFE_2100)),
    ("NEAR_FILL_NOT_EXACT", p.epoch(-9.9e30)),
    ("GENUINE_9999_NON_SENTINEL", p.epoch(315569519999999.0)),
    ("NAN", p.epoch(math.nan)),
    ("DBL_MAX", p.epoch(DBL_MAX)),
]

EPOCH16_CASES = [
    ("FILL", p.epoch16(-1e31, -1e31)),
    ("PAD_YEAR0", p.epoch16(0.0, 0.0)),
    ("PRE_1970", p.epoch16(*EPOCH16_PRE_1970)),
    ("SAFE_2100", p.epoch16(*EPOCH16_SAFE_2100)),
    ("PARTIAL_FILL_MISMATCH", p.epoch16(-1e31, 0.0)),
    ("GENUINE_9999_NON_SENTINEL", p.epoch16(315569519999.0, 999000000000.0)),
    ("NAN", p.epoch16(math.nan, math.nan)),
    ("DBL_MAX", p.epoch16(DBL_MAX, DBL_MAX)),
]

TT2000_CASES = [
    ("FILL", p.tt2000_t(INT64_MIN)),
    ("PADVALUE", p.tt2000_t(INT64_MIN + 1)),
    ("MYSTERY_ALIAS", p.tt2000_t(INT64_MIN + 3)),
    ("UNHANDLED_GAP", p.tt2000_t(INT64_MIN + 2)),
    ("PRE_1970", p.tt2000_t(TT2000_PRE_1970)),
    ("SAFE_2100", p.tt2000_t(TT2000_SAFE_2100)),
    ("INT64_MAX", p.tt2000_t(INT64_MAX)),
]


def add_time_variable(cdf, name, data_type, dummy_value, cases, fillval, validmin, validmax):
    var = cdf.add_variable(
        name,
        values=np.array([dummy_value], dtype="datetime64[ns]"),
        data_type=data_type,
    )
    var.add_attribute("CATDESC", "Pathological time test variable")
    var.add_attribute("VAR_TYPE", "support_data")
    var.add_attribute("FILLVAL", [fillval])
    var.add_attribute("VALIDMIN", [validmin])
    var.add_attribute("VALIDMAX", [validmax])
    var.add_attribute("PATHOLOGICAL_CASE_LABELS", ",".join(label for label, _ in cases))
    var.add_attribute("PATHOLOGICAL_CASES", [value for _, value in cases])
    return var


def make_pathological_time_cdf(path):
    cdf = p.CDF()
    add_time_variable(
        cdf, "Epoch", p.DataType.CDF_EPOCH, "2000-01-01",
        EPOCH_CASES,
        fillval=p.epoch(-1e31), validmin=p.epoch(EPOCH_PRE_1970), validmax=p.epoch(EPOCH_SAFE_2100),
    )
    add_time_variable(
        cdf, "Epoch16", p.DataType.CDF_EPOCH16, "2000-01-01",
        EPOCH16_CASES,
        fillval=p.epoch16(-1e31, -1e31), validmin=p.epoch16(*EPOCH16_PRE_1970),
        validmax=p.epoch16(*EPOCH16_SAFE_2100),
    )
    add_time_variable(
        cdf, "TT2000", p.DataType.CDF_TIME_TT2000, "2000-01-01",
        TT2000_CASES,
        fillval=p.tt2000_t(INT64_MIN), validmin=p.tt2000_t(TT2000_PRE_1970),
        validmax=p.tt2000_t(TT2000_SAFE_2100),
    )
    assert p.save(cdf, path)


if __name__ == "__main__":
    make_pathological_time_cdf("pathological_times.cdf")
