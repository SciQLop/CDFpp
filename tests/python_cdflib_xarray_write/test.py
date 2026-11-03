"""pycdfpp.cdflib.xarray.xarray_to_cdf against the real cdflib.xarray.xarray_to_cdf: same checks, same files."""
import collections
import contextlib
import logging
import os
import shutil
import subprocess
import tempfile
import unittest
import warnings
from pathlib import Path

import numpy as np
import xarray as xr
from ddt import data, ddt

import cdflib.xarray
import pycdfpp

RESOURCES = Path(__file__).resolve().parent.parent / "resources"
UNREADABLE_BY_CDFLIB = ("not_a_cdf.cdf", "pathological_times.cdf", "time_special_values.cdf")
CORPUS = sorted(p.name for p in RESOURCES.glob("*.cdf") if p.name not in UNREADABLE_BY_CDFLIB)
# (cdf_to_xarray options, xarray_to_cdf options); "skip_first" stands for the first data variable's name.
OPTIONS = {
    "default": ({}, {}),
    "unix_time_to_cdf_time": ({"to_unixtime": True}, {"unix_time_to_cdf_time": True}),
    "compression": ({}, {"compression": 6, "compression_skip_vars": "skip_first"}),
    "nan_to_fillval": ({"fillval_to_nan": True}, {}),
    "no_nan_to_fillval": ({"fillval_to_nan": True}, {"nan_to_fillval": False}),
    "record_dimensions": ({}, {"record_dimensions": ["record0", "record1", "dim0"]}),
    "no_auto_fix_depends": ({}, {"auto_fix_depends": False}),
    "no_istp": ({}, {"istp": False}),
}
# cdflib writes NaT as TT2000 0 (this instant): we write the fill value, read back as NaT.
TT2000_ZERO = np.datetime64("2000-01-01T11:58:55.816", "ns")
RAW_FILLS = {"CDF_EPOCH": (-1e31,), "CDF_EPOCH16": (-1e31, -1e31), "CDF_TIME_TT2000": (np.iinfo(np.int64).min,)}
MAX_SIZE = 40  # values kept per dimension: cdflib converts times one by one


def find_cdfvalidate():
    candidates = (os.environ.get("CDFVALIDATE"), shutil.which("cdfvalidate"),
                  str(Path.home() / ".local" / "cdf" / "bin" / "cdfvalidate"))
    return next((c for c in candidates if c and Path(c).is_file()), None)


def ours_module():
    from pycdfpp.cdflib import xarray as ours
    return ours


# cdflib warnings that never raise, even with terminate_on_warning (auto fixes, data types, its writer).
CDFLIB_WARNING_ONLY = ("ISTP Compliance Error, need to add", "Auto correcting", "CONVERSION ERROR", "NOT SUPPORTED",
                       "Attribute:")


# Ours logs this one, cdflib prints it (docs/cdflib.rst): both raise Exception with terminate_on_warning.
PRINTED_BY_CDFLIB = "could not be checked"


def escalation(record, ours):
    """What a warning becomes with terminate_on_warning. Ours tags its records; cdflib's message decides it."""
    if ours and PRINTED_BY_CDFLIB in record.getMessage():
        return "warning only"
    if ours:
        return getattr(record, "cdflib_error", "warning only")
    message = record.getMessage()
    if message.startswith(CDFLIB_WARNING_ONLY):
        return "warning only"
    return "ISTPError" if "ISTP" in message else "Exception"


class Outcome:
    """What a call did: the exception it raised, the warnings it logged (what each would raise with
    terminate_on_warning), whether it wrote the file."""

    def __init__(self, module, dataset, path, **options):
        self.error = None
        with captured_warnings() as records:
            try:
                module.xarray_to_cdf(dataset, str(path), **options)
            except Exception as error:  # the type is what both libraries must agree on
                # Its traceback keeps cdflib's writer alive, with the file open: Windows can't delete it then.
                self.error = error.with_traceback(None)
        ours = module.__name__.startswith("pycdfpp")
        kinds = [escalation(record, ours) for record in records]
        self.warned = bool(kinds)
        self.escalating = sorted(collections.Counter(k for k in kinds if k != "warning only").items())
        self.written = Path(path).is_file()

    def summary(self):
        kind = type(self.error).__name__ if self.error is not None else None
        return kind, self.warned, self.escalating, self.written if self.error is None else None


class _Collect(logging.Handler):
    def __init__(self):
        super().__init__(logging.WARNING)
        self.records = []

    def emit(self, record):
        self.records.append(record)


@contextlib.contextmanager
def captured_warnings():
    """cdflib's logger and ours are the same logger: both are named cdflib.logging."""
    logger = logging.getLogger("cdflib.logging")
    handler, propagate, level = _Collect(), logger.propagate, logger.level
    logger.addHandler(handler)
    logger.propagate, logger.level = False, logging.WARNING
    try:
        yield handler.records
    finally:
        logger.removeHandler(handler)
        logger.propagate, logger.level = propagate, level


def comparable_attribute(value):
    """Our writer keeps unsigned attributes of 1 and 2 bytes, cdflib makes them CDF_UINT4 (docs/cdflib.rst)."""
    if isinstance(value, list):
        return [comparable_attribute(v) for v in value]
    if isinstance(value, (np.ndarray, np.generic)) and value.dtype.kind == "u":
        return value.astype(np.uint32)
    return value


def comparable_variable(variable):
    """cdflib writes datetimes as CDF_EPOCH16 to the second, one record late: CDF_EPOCH16 values are left out here
    and checked against the source Dataset instead."""
    attrs = {k: comparable_attribute(a) for k, a in variable.attrs.items()}
    if attrs.get("CDF_DATA_TYPE") == "CDF_EPOCH16":
        attrs = {k: a for k, a in attrs.items() if np.asarray(a).dtype.kind != "M"}
        return xr.Variable(variable.dims, np.zeros(variable.shape), attrs)
    return xr.Variable(variable.dims, variable.values, attrs)


def comparable(dataset):
    variables = {name: comparable_variable(v) for name, v in dataset.variables.items()}
    attrs = {k: comparable_attribute(a) for k, a in dataset.attrs.items()}
    return dataset.assign(variables).assign_attrs(attrs)


def read_back(path):
    return comparable(cdflib.xarray.cdf_to_xarray(str(path)))


def read_back_or_error(path):
    try:
        return read_back(path)
    except Exception as error:  # cdflib can't read every file it writes
        return error.with_traceback(None)  # the traceback keeps the reader alive, with the file open


def nat_where_cdflib_wrote_zero(theirs, ours):
    """cdflib writes NaT as TT2000 0: only where ours holds NaT, so a real 0 instant is still compared."""
    t, o = np.asarray(theirs), np.asarray(ours)
    if t.dtype.kind != "M" or o.dtype.kind != "M" or t.shape != o.shape:
        return theirs
    return np.where(np.isnat(o) & (t == TT2000_ZERO), o, t)


def with_typed_epoch_fill(theirs_attrs, our_attrs):
    """We write a float64 FILLVAL of a CDF_EPOCH variable as CDF_EPOCH (read back as NaT and listed in TIME_ATTRS),
    cdflib as CDF_DOUBLE."""
    fill = np.asarray(our_attrs.get("FILLVAL", 0))
    if fill.dtype.kind == "M" and np.isnat(fill).all() and np.all(np.asarray(theirs_attrs.get("FILLVAL")) == -1e31):
        return {**theirs_attrs, "FILLVAL": our_attrs["FILLVAL"], "TIME_ATTRS": our_attrs.get("TIME_ATTRS")}
    return theirs_attrs


def without_cdflib_zeros(theirs, ours):
    def variable(name, v):
        if name not in ours.variables:
            return v
        mine = ours[name]
        attrs = {k: nat_where_cdflib_wrote_zero(a, mine.attrs[k]) if k in mine.attrs else a for k, a in v.attrs.items()}
        attrs = with_typed_epoch_fill(attrs, mine.attrs)
        return xr.Variable(v.dims, nat_where_cdflib_wrote_zero(v.values, mine.values), attrs)
    return theirs.assign({name: variable(name, v) for name, v in theirs.variables.items()})


def assert_same_read_back(testcase, our_path, their_path):
    """Where cdflib can't read its own file back, it reads ours or fails the same way."""
    ours, theirs = read_back_or_error(our_path), read_back_or_error(their_path)
    if isinstance(theirs, Exception):
        testcase.assertTrue(isinstance(ours, xr.Dataset) or type(ours) is type(theirs), f"{ours!r} {theirs!r}")
    else:
        testcase.assertIsInstance(ours, xr.Dataset, repr(ours))
        xr.testing.assert_identical(ours, without_cdflib_zeros(theirs, ours))


def assert_epoch16_kept(testcase, our_path, source):
    """Our CDF_EPOCH16 values read back as the source Dataset holds them."""
    written = cdflib.xarray.cdf_to_xarray(str(our_path))
    for name, variable in source.variables.items():
        if variable.attrs.get("CDF_DATA_TYPE") == "CDF_EPOCH16" and variable.dtype.kind == "M" and name in written:
            np.testing.assert_array_equal(written[name].values.ravel(), variable.values.ravel(), err_msg=name)


def compressions(path):
    return {name: variable.compression for name, variable in pycdfpp.load(str(path)).items()}


def corpus_dataset(name, read_options):
    dataset = cdflib.xarray.cdf_to_xarray(str(RESOURCES / name), **read_options)
    return dataset.isel({dim: slice(0, MAX_SIZE) for dim in dataset.dims})


def write_options(options, dataset):
    skip = list(dataset.data_vars)[:1]
    return {k: skip if v == "skip_first" else v for k, v in options.items()}


@ddt
class CorpusRoundTrip(unittest.TestCase):
    """cdflib reads back the same Dataset from our file as from its own (it rarely is the first Dataset)."""

    @classmethod
    def setUpClass(cls):
        logging.getLogger("cdflib.logging").setLevel(logging.ERROR)
        warnings.simplefilter("ignore", RuntimeWarning)

    def setUp(self):
        self.folder = tempfile.TemporaryDirectory()
        self.addCleanup(self.folder.cleanup)
        self.root = Path(self.folder.name)

    def written(self, label, name, module):
        read_options, options = OPTIONS[label]
        try:
            dataset = corpus_dataset(name, read_options)
        except ValueError as error:  # cdflib reads year 0 dates as unix times: see python_cdflib_xarray
            self.skipTest(f"cdflib can't read {name} with {read_options}: {error}")
        path = self.root / f"{label}-{module.__name__.split('.')[0]}-{name}"
        return Outcome(module, dataset, path, **write_options(options, dataset)), path, dataset

    @data(*CORPUS)
    def test_same_file_content_as_cdflib(self, name):
        for label in OPTIONS:
            with self.subTest(label):
                ours, our_path, source = self.written(label, name, ours_module())
                theirs, their_path, _ = self.written(label, name, cdflib.xarray)
                if ours.written:
                    assert_epoch16_kept(self, our_path, source)
                if theirs.error is not None:
                    self.assertTrue(ours.written or type(ours.error) is type(theirs.error),
                                    f"{ours.error!r} {theirs.error!r}")
                    continue
                self.assertIsNone(ours.error, repr(ours.error))
                assert_same_read_back(self, our_path, their_path)
                self.assertEqual(compressions(our_path), compressions(their_path))

    def test_written_files_pass_nasa_cdfvalidate(self):
        cdfvalidate = find_cdfvalidate()
        if cdfvalidate is None:
            self.skipTest("NASA's cdfvalidate not found")
        for name in CORPUS:
            for label in ("default", "compression", "unix_time_to_cdf_time"):
                with self.subTest(name=name, option=label):
                    ours, path, _ = self.written(label, name, ours_module())
                    self.assertTrue(ours.written, repr(ours.error))
                    result = subprocess.run([cdfvalidate, str(path)], capture_output=True, text=True)
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                    self.assertNotIn("error", result.stdout.lower(), result.stdout)


GLOBAL_ATTRIBUTES = {name: "x" for name in (
    "Project", "Source_name", "Discipline", "Data_type", "Descriptor", "Data_version", "Logical_file_id", "PI_name",
    "PI_affiliation", "TEXT", "Instrument_type", "Mission_group", "Logical_source", "Logical_source_description")}
TIMES = np.array(["2020-01-01T00:00:00", "2020-01-01T00:00:01", "2020-01-01T00:00:02", "2020-01-01T00:00:03"],
                 dtype="datetime64[ns]")


def described(var_type, **attrs):
    return {"VAR_TYPE": var_type, "CATDESC": "c", "FIELDNAM": "f", "FORMAT": "F8.3", **attrs}


def istp_dataset():
    """An ISTP compliant Dataset: cdflib's checks find nothing to say about it."""
    epoch = xr.Variable(("Epoch",), TIMES, described("support_data", UNITS="ns", VALIDMIN=TIMES[0],
                                                       VALIDMAX=TIMES[-1], FILLVAL=np.datetime64("NaT", "ns")))
    b = xr.Variable(("Epoch", "comp"), np.arange(12, dtype=np.float32).reshape(4, 3),
                    described("data", DEPEND_0="Epoch", DEPEND_1="comp", LABL_PTR_1="b_label",
                              DISPLAY_TYPE="time_series", UNITS="nT", VALIDMIN=-1e3, VALIDMAX=1e3,
                              FILLVAL=np.float32(-1e31)))
    comp = xr.Variable(("comp",), np.arange(3, dtype=np.int32), described("support_data", UNITS="1"))
    label = xr.Variable(("comp",), np.array(["Bx", "By", "Bz"]), described("metadata", DEPEND_1="comp"))
    return xr.Dataset({"B": b, "b_label": label}, coords={"Epoch": epoch, "comp": comp}, attrs=GLOBAL_ATTRIBUTES)


def without(name, *keys):
    def change(ds):
        for key in keys:
            del ds[name].attrs[key]
        return ds
    return change


def with_attrs(name, **attrs):
    def change(ds):
        ds[name].attrs.update(attrs)
        return ds
    return change


def with_global_attrs(**attrs):
    def change(ds):
        ds.attrs.update(attrs)
        return ds
    return change


def without_global(name):
    def change(ds):
        del ds.attrs[name]
        return ds
    return change


def with_variable(name, dims, values, attrs):
    return lambda ds: ds.assign({name: xr.Variable(dims, values, attrs)})


def both(*changes):
    def change(ds):
        for c in changes:
            ds = c(ds)
        return ds
    return change


def renamed_epoch(new_name):
    return both(lambda ds: ds.rename({"Epoch": new_name}), with_attrs("B", DEPEND_0=new_name))


def epoch_values(values):
    return lambda ds: ds.assign_coords(Epoch=ds["Epoch"].copy(data=values))


def only_metadata(ds):
    return xr.Dataset({"m": xr.Variable(("n",), np.arange(2), described("metadata"))}, attrs=GLOBAL_ATTRIBUTES)


SUPPORT = described("support_data", UNITS="1")
DATA = described("data", DISPLAY_TYPE="time_series", LABLAXIS="l", UNITS="1", VALIDMIN=0, VALIDMAX=9, FILLVAL=-1,
                 DEPEND_0="Epoch")
TEXT_DATA = dict(DATA, VALIDMIN="a", VALIDMAX="z", FILLVAL=" ")
# One invalid Dataset per check of cdflib's xarray_to_cdf (and a few that must stay silent).
ISTP_CASES = {
    "compliant": lambda ds: ds,
    "invalid variable name": with_variable("bad-name", ("Epoch",), np.arange(4.), DATA),
    "invalid global attribute name": with_global_attrs(**{"bad name": "x"}),
    "invalid variable attribute name": with_attrs("comp", **{"bad-attr": 1}),
    "label pointer to a missing variable": with_attrs("B", LABL_PTR_1="missing"),
    "label without VAR_TYPE becomes metadata": without("b_label", "VAR_TYPE"),
    "missing VAR_TYPE": without("comp", "VAR_TYPE"),
    "invalid VAR_TYPE": with_attrs("comp", VAR_TYPE="foo"),
    "capitalised VAR_TYPE": with_attrs("comp", VAR_TYPE="Support_Data"),
    "DEPEND_1 to a missing variable": with_attrs("B", DEPEND_1="missing"),
    "DEPEND_1 of another size": both(with_variable("other", ("other",), np.arange(5), SUPPORT),
                                      with_attrs("B", DEPEND_1="other")),
    "DEPEND_1 to a scalar": both(with_variable("scalar", (), 1.0, SUPPORT), with_attrs("B", DEPEND_1="scalar")),
    "scalar with a DEPEND_1": with_variable("scalar", (), 1.0, dict(SUPPORT, DEPEND_1="comp")),
    "1D variable with a DEPEND_2": with_attrs("comp", DEPEND_2="b_label"),
    "DEPEND_1 with 3 dimensions": both(with_variable("cube", ("x", "y", "z"), np.zeros((2, 2, 3)), SUPPORT),
                                        with_attrs("B", DEPEND_1="cube")),
    "2D DEPEND_1 with other records": both(with_variable("table", ("t", "comp"), np.zeros((2, 3)), SUPPORT),
                                            with_attrs("B", DEPEND_1="table")),
    "2D DEPEND_1 with the same records": both(
        with_variable("table", ("Epoch", "comp"), np.zeros((4, 3)), dict(SUPPORT, DEPEND_0="Epoch", VALIDMIN=0,
                                                                         VALIDMAX=1, FILLVAL=-1.0)),
        with_attrs("B", DEPEND_1="table")),
    "DEPEND_2 beyond the data dimensions": with_attrs("B", DEPEND_2="comp", LABL_PTR_2="b_label"),
    "DEPEND_2 of support_data with DEPEND_0": both(
        with_variable("s", ("Epoch", "comp"), np.zeros((4, 3)), dict(SUPPORT, DEPEND_0="Epoch", DEPEND_2="comp",
                                                                     VALIDMIN=0, VALIDMAX=1, FILLVAL=-1.0)), ),
    "DEPEND_2 of a variable without VAR_TYPE": both(
        with_variable("s", ("Epoch", "comp"), np.zeros((4, 3)), {"DEPEND_0": "Epoch", "DEPEND_2": "comp"})),
    "lowercase depend_0": with_attrs("comp", depend_0="comp"),
    "data dimension without variable nor DEPEND": with_variable(
        "d", ("Epoch", "free"), np.zeros((4, 2)), DATA),
    "DEPEND_0 to a missing variable": with_attrs("B", DEPEND_0="missing"),
    "DEPEND_0 of another size": both(with_variable("t2", ("t2",), TIMES[:2], SUPPORT), with_attrs("B", DEPEND_0="t2")),
    "DEPEND_0 to an epoch named missing variable": with_variable("epoch_9", ("x",), TIMES[:3],
                                                                dict(SUPPORT, DEPEND_0="epoch_7")),
    "empty DEPEND_0": with_attrs("B", DEPEND_0=""),
    "non string DEPEND_0": with_attrs("comp", DEPEND_0=3),
    "missing DEPEND_0 is added": without("B", "DEPEND_0"),
    "missing DEPEND_1 is added": without("B", "DEPEND_1", "LABL_PTR_1"),
    "support_data 2D without DEPEND_0": with_variable("s2", ("Epoch", "comp"), np.zeros((4, 3)),
                                                      dict(SUPPORT, VALIDMIN=0, VALIDMAX=1, FILLVAL=-1.0)),
    "time variable not named Epoch": renamed_epoch("time"),
    "time variable named epoch_1": renamed_epoch("epoch_1"),
    "Epoch not monotonic": epoch_values(TIMES[::-1]),
    "no time variable": only_metadata,
    "missing global attribute": without_global("PI_name"),
    "attribute set to None": with_attrs("comp", NOTE=None),
    "missing CATDESC": without("comp", "CATDESC"),
    "missing DISPLAY_TYPE": without("B", "DISPLAY_TYPE"),
    "image without LABLAXIS": with_attrs("B", DISPLAY_TYPE="image"),
    "image with LABLAXIS": with_attrs("B", DISPLAY_TYPE="image", LABLAXIS="b"),
    "data without LABLAXIS nor LABL_PTR_1": with_variable(
        "n1", ("Epoch",), np.arange(4.), {k: v for k, v in DATA.items() if k != "LABLAXIS"}),
    "time_series without LABL_PTR_1": without("B", "LABL_PTR_1"),
    "LABL_PTR_1 and LABLAXIS": with_attrs("B", LABLAXIS="b"),
    "spectrogram without LABL_PTR_1": both(without("B", "LABL_PTR_1"),
                                          with_attrs("B", DISPLAY_TYPE="spectrogram", LABLAXIS="b")),
    "spectrogram with DEPEND_2 without label": with_variable(
        "spec", ("Epoch", "comp", "e"), np.zeros((4, 3, 2)),
        dict(DATA, DEPEND_1="comp", DEPEND_2="e", DISPLAY_TYPE="spectrogram")),
    "missing FIELDNAM": without("comp", "FIELDNAM"),
    "missing FORMAT": without("comp", "FORMAT"),
    "FORM_PTR to a variable": both(without("comp", "FORMAT"), with_attrs("comp", FORM_PTR="b_label")),
    "FORM_PTR to a missing variable": both(without("comp", "FORMAT"), with_attrs("comp", FORM_PTR="missing")),
    "missing UNITS": without("B", "UNITS"),
    "UNIT_PTR to a variable": both(without("B", "UNITS"), with_attrs("B", UNIT_PTR="b_label")),
    "UNIT_PTR to a missing variable": both(without("B", "UNITS"), with_attrs("B", UNIT_PTR="missing")),
    "data without VALIDMIN": without("B", "VALIDMIN"),
    "data without VALIDMAX": without("B", "VALIDMAX"),
    "record varying support_data without VALIDMIN": without("Epoch", "VALIDMIN"),
    "record varying support_data without VALIDMAX": without("Epoch", "VALIDMAX"),
    "data without FILLVAL": without("B", "FILLVAL"),
    "record varying support_data without FILLVAL": without("Epoch", "FILLVAL"),
    "support_data without UNITS": without("comp", "UNITS"),
    "uint64 data": with_variable("u", ("Epoch",), np.arange(4, dtype=np.uint64), DATA),
    "bool data": with_variable("flag", ("Epoch",), np.array([True, False, True, True]), TEXT_DATA),
    "string data": with_variable("s", ("Epoch",), np.array(["a", "bc", "d", "e"]), TEXT_DATA),
    "float16 data": with_variable("h", ("Epoch",), np.arange(4, dtype=np.float16), DATA),
    "complex data": with_variable("e16", ("Epoch",), np.arange(4, dtype=np.complex128), DATA),
    "object strings": with_variable("o", ("comp",), np.array(["a", "bcd", "ef"], dtype=object), described("metadata")),
    "bytes": with_variable("by", ("comp",), np.array([b"a", b"bcd", b"ef"]), described("metadata")),
    "CDF_DATA_TYPE override": with_attrs("B", CDF_DATA_TYPE="CDF_REAL8"),
    "NaN values": with_variable("n", ("Epoch",), np.array([1.0, np.nan, 2.0, 3.0]), DATA),
    "NaT values": epoch_values(np.array([TIMES[0], "NaT", TIMES[2], TIMES[3]], dtype="datetime64[ns]")),
    "BIN_LOCATION": with_attrs("Epoch", BIN_LOCATION=0.5),
}


# Cases where cdflib writes wrong values (docs/cdflib.rst), checked by their own tests below.
CONTENT_DIFFERS = {"bytes", "uint64 data"}


def build(case):
    return ISTP_CASES[case](istp_dataset())


@ddt
class IstpChecks(unittest.TestCase):
    """Each check gives cdflib's outcome: the exception type, whether it warned, whether it wrote the file."""

    def setUp(self):
        self.folder = tempfile.TemporaryDirectory()
        self.addCleanup(self.folder.cleanup)
        self.root = Path(self.folder.name)

    def outcomes(self, dataset, folder=None, **options):
        folder = folder or Path(tempfile.mkdtemp(dir=self.root))
        ours = Outcome(ours_module(), dataset, folder / "ours.cdf", **options)
        theirs = Outcome(cdflib.xarray, dataset, folder / "theirs.cdf", **options)
        return ours, theirs, folder

    def assert_same_outcome(self, dataset, same_content=True, **options):
        ours, theirs, folder = self.outcomes(dataset, **options)
        self.assertEqual(ours.summary(), theirs.summary(), f"{ours.error!r} {theirs.error!r}")
        if theirs.error is not None:
            self.assertFalse(ours.written, "an error leaves no file")
        elif theirs.written and same_content:
            assert_same_read_back(self, folder / "ours.cdf", folder / "theirs.cdf")

    @data(*ISTP_CASES)
    def test_same_outcome_as_cdflib(self, case):
        for options in ({}, {"terminate_on_warning": True}, {"auto_fix_depends": False}, {"istp": False}):
            with self.subTest(**options):
                self.assert_same_outcome(build(case), same_content=case not in CONTENT_DIFFERS, **options)

    def test_compliant_dataset_is_silent(self):
        ours, theirs, _ = self.outcomes(build("compliant"), terminate_on_warning=True)
        self.assertEqual((ours.summary(), theirs.summary()), (((None, False, [], True),) * 2))

    def test_existing_file(self):
        for options in ({}, {"terminate_on_warning": True}):
            with self.subTest(**options):
                for name in ("ours.cdf", "theirs.cdf"):
                    (self.root / name).write_bytes(b"")
                ours, theirs, _ = self.outcomes(build("compliant"), self.root, **options)
                self.assertEqual(ours.summary(), theirs.summary())
                self.assertEqual((self.root / "ours.cdf").read_bytes(), b"")

    def test_existing_file_written_without_cdf_suffix(self):
        ours = ours_module()
        ours.xarray_to_cdf(build("compliant"), self.root / "ours.dat")
        first = (self.root / "ours.cdf").read_bytes()
        self.assertFalse((self.root / "ours.dat").exists())
        with captured_warnings() as records:
            ours.xarray_to_cdf(build("compliant"), self.root / "ours.dat")
        self.assertEqual([r.getMessage() for r in records], [f"{self.root / 'ours.dat'} exists: nothing written"])
        self.assertEqual((self.root / "ours.cdf").read_bytes(), first)

    def test_ISTPError_is_an_Exception(self):
        from pycdfpp.cdflib.xarray.xarray_to_cdf import ISTPError
        self.assertTrue(issubclass(ISTPError, Exception))
        self.assertEqual(str(ISTPError()), "")

    def test_input_dataset_is_not_modified(self):
        dataset = build("missing DEPEND_0 is added")
        before = dataset.copy(deep=True)
        Outcome(ours_module(), dataset, self.root / "ours.cdf")
        xr.testing.assert_identical(dataset, before)

    def test_nat_is_written_as_fill_value(self):
        """cdflib writes NaT as TT2000 0 (2000-01-01T11:58:55.816)."""
        Outcome(ours_module(), build("NaT values"), self.root / "ours.cdf")
        epoch = pycdfpp.load(str(self.root / "ours.cdf"))["Epoch"]
        self.assertEqual(epoch.values_encoded.view(np.int64)[1], np.iinfo(np.int64).min)

    def test_validmin_keeps_the_variable_type_with_unix_times(self):
        """cdflib leaves VALIDMIN, VALIDMAX and FILLVAL untyped when unix_time_to_cdf_time is set."""
        Outcome(ours_module(), build("compliant"), self.root / "ours.cdf", unix_time_to_cdf_time=True)
        b = pycdfpp.load(str(self.root / "ours.cdf"))["B"]
        self.assertEqual(b.attributes["VALIDMIN"].type(), pycdfpp.DataType.CDF_FLOAT)

    def test_bytes_are_written_as_text(self):
        """cdflib writes str() of each value: "b'a'"."""
        Outcome(ours_module(), build("bytes"), self.root / "ours.cdf")
        self.assertEqual(read_back(self.root / "ours.cdf")["by"].values.tolist(), ["a", "bcd", "ef"])

    def test_datetimes_as_epoch16_keep_their_instants(self):
        """cdflib cuts them to the second and writes each one a record late."""
        times = np.array(["2004-11-29T15:55:23.030411522", "2005-12-29T16:56:24.031444555"], dtype="datetime64[ns]")
        dataset = xr.Dataset({"t": xr.Variable(("x",), times, {"CDF_DATA_TYPE": "CDF_EPOCH16"})})
        Outcome(ours_module(), dataset, self.root / "ours.cdf", istp=False)
        written = pycdfpp.load(str(self.root / "ours.cdf"))["t"]
        self.assertEqual(written.type, pycdfpp.DataType.CDF_EPOCH16)
        np.testing.assert_array_equal(pycdfpp.to_datetime64(written).ravel(), times)

    def test_raw_epoch16_read_back_keeps_its_instants_when_written_again(self):
        """Read without to_datetime, EPOCH16 values are CDF_EPOCH milliseconds: written back as CDF_EPOCH."""
        times = np.array(["2020-01-01T00:00:00.123", "2021-06-01T12:00:00.456"], dtype="datetime64[ns]")
        dataset = xr.Dataset({"t": xr.Variable(("x",), times, {"CDF_DATA_TYPE": "CDF_EPOCH16"})})
        ours = ours_module()
        Outcome(ours, dataset, self.root / "first.cdf", istp=False, record_dimensions=["t"])
        raw = ours.cdf_to_xarray(str(self.root / "first.cdf"), to_datetime=False)
        Outcome(ours, raw, self.root / "second.cdf", istp=False, record_dimensions=["t"])
        written = pycdfpp.load(str(self.root / "second.cdf"))["t"]
        np.testing.assert_array_equal(pycdfpp.to_datetime64(written).ravel(), times)

    def written_variable(self, name, values, attrs=None, **options):
        dataset = xr.Dataset({name: xr.Variable(("x",), values, attrs or {})})
        Outcome(ours_module(), dataset, self.root / "ours.cdf", istp=False, record_dimensions=[name], **options)
        return pycdfpp.load(str(self.root / "ours.cdf"))[name]

    def test_uint64_below_2_63_is_int8(self):
        """cdflib wraps uint64 into CDF_UINT4."""
        for top in (2**32 - 1, 2**62):
            with self.subTest(top=top):
                values = np.array([0, 7, top], dtype=np.uint64)
                written = self.written_variable("u", values)
                self.assertEqual(written.type, pycdfpp.DataType.CDF_INT8)
                np.testing.assert_array_equal(written.values, values.astype(np.int64))
                (self.root / "ours.cdf").unlink()

    def test_uint64_from_2_63_raises(self):
        dataset = xr.Dataset({"u": xr.Variable(("x",), np.array([1, 2**63], dtype=np.uint64))})
        outcome = Outcome(ours_module(), dataset, self.root / "ours.cdf", istp=False)
        self.assertIsInstance(outcome.error, ValueError)
        self.assertFalse(outcome.written)

    def test_integers_outside_the_given_type_raise(self):
        """cdflib wraps them: 2**63 as CDF_INT8 is the fill value, 300 as CDF_INT1 is 44."""
        for values, cdf_type in ((np.array([1, 2**63], dtype=np.uint64), "CDF_INT8"),
                                 (np.array([1, 300]), "CDF_INT1"), (np.array([1, 2**40]), "CDF_UINT4")):
            with self.subTest(cdf_type):
                dataset = xr.Dataset({"v": xr.Variable(("x",), values, {"CDF_DATA_TYPE": cdf_type})})
                outcome = Outcome(ours_module(), dataset, self.root / "ours.cdf", istp=False)
                self.assertIsInstance(outcome.error, ValueError)
                self.assertRegex(str(outcome.error), f"v.*{cdf_type}.*{values[-1]}")
                self.assertFalse(outcome.written)

    def test_floats_outside_the_given_integer_type_raise(self):
        """cdflib wraps them: 300.5 as CDF_INT1 is 44, NaN is 0, -1.0 as CDF_UINT2 is 65535."""
        for values, cdf_type in ((np.array([1.0, 300.5]), "CDF_INT1"), (np.array([1.0, np.nan]), "CDF_INT1"),
                                 (np.array([1.0, -1.0]), "CDF_UINT2")):
            with self.subTest(cdf_type, values=values):
                dataset = xr.Dataset({"v": xr.Variable(("x",), values, {"CDF_DATA_TYPE": cdf_type})})
                outcome = Outcome(ours_module(), dataset, self.root / "ours.cdf", istp=False, nan_to_fillval=False)
                self.assertIsInstance(outcome.error, ValueError)
                self.assertRegex(str(outcome.error), f"v.*{cdf_type}.*{values[-1]}")
                self.assertFalse(outcome.written)

    def test_istp_limits_outside_the_integer_type_are_not_wrapped(self):
        """A float FILLVAL of -1e31 on an integer variable (as in SolO RPW files) is the type's fill value. cdflib wraps
        it, and VALIDMIN or VALIDMAX, into the variable's type."""
        attrs = {"FILLVAL": np.float32(-1e31), "VALIDMIN": np.int64(-5), "VALIDMAX": np.float64(1e10)}
        dataset = xr.Dataset({"v": xr.Variable(("x",), np.array([1, 2, 3], dtype=np.int32), attrs)})
        outcome = Outcome(ours_module(), dataset, self.root / "ours.cdf", record_dimensions=["v"])
        self.assertIsNone(outcome.error)
        written = pycdfpp.load(str(self.root / "ours.cdf"))["v"].attributes
        self.assertEqual({key: (written[key].type(), written[key].value) for key in attrs},
                         {"FILLVAL": (pycdfpp.DataType.CDF_INT4, [np.iinfo(np.int32).min]),
                          "VALIDMIN": (pycdfpp.DataType.CDF_INT4, [-5]),
                          "VALIDMAX": (pycdfpp.DataType.CDF_DOUBLE, [1e10])})

    def test_fillval_of_float_time_types_has_their_type(self):
        """ISTP: FILLVAL, VALIDMIN and VALIDMAX have the variable's type. cdflib leaves raw CDF_EPOCH values
        (float64) untyped: CDF_DOUBLE. Its writer guesses CDF_EPOCH16 for complex values."""
        for cdf_type, unix in (("CDF_EPOCH", True), ("CDF_EPOCH", False), ("CDF_EPOCH16", False)):
            with self.subTest(cdf_type, unix=unix):
                raw = {"CDF_EPOCH": 6.3e13, "CDF_EPOCH16": complex(6.3e10, 0)}[cdf_type]
                values = np.array([0.0, 1.0, 2.0, 3.0]) if unix else np.array([raw] * 4)
                attrs = dict(DATA, CDF_DATA_TYPE=cdf_type, VALIDMIN=raw, VALIDMAX=raw)
                del attrs["FILLVAL"]
                dataset = with_variable("t", ("Epoch",), values, attrs)(istp_dataset())
                Outcome(ours_module(), dataset, self.root / "ours.cdf", unix_time_to_cdf_time=unix)
                written = pycdfpp.load(str(self.root / "ours.cdf"))["t"]
                for key in ("FILLVAL", "VALIDMIN", "VALIDMAX"):
                    self.assertEqual(written.attributes[key].type().name, cdf_type, key)
                del written  # its values are unread: pycdfpp still maps the file, Windows can't delete it
                (self.root / "ours.cdf").unlink()

    def test_big_endian_numbers_keep_their_type(self):
        """cdflib warns and writes them as text (one character per value without NaN filling)."""
        for values, data_type in (((np.arange(3) + 0.5).astype(">f8"), pycdfpp.DataType.CDF_DOUBLE),
                                  ((np.arange(3) - 1).astype(">i4"), pycdfpp.DataType.CDF_INT4)):
            for nan_to_fillval in (True, False):
                with self.subTest(values.dtype.name, nan_to_fillval=nan_to_fillval):
                    written = self.written_variable("v", values, nan_to_fillval=nan_to_fillval)
                    self.assertEqual(written.type, data_type)
                    np.testing.assert_array_equal(written.values, values)
                    (self.root / "ours.cdf").unlink()

    def test_missing_unix_times_are_fill_values(self):
        """cdflib raises OverflowError, or writes NaN or -1e31 taken as a unix time."""
        expected = np.array(["1970-01-01", "NaT", "2001-09-09T01:46:40"], dtype="datetime64[ns]")
        for cdf_type in ("CDF_EPOCH", "CDF_EPOCH16", "CDF_TIME_TT2000"):
            for nan_to_fillval in (True, False):
                with self.subTest(cdf_type=cdf_type, nan_to_fillval=nan_to_fillval):
                    written = self.written_variable("t", np.array([0.0, np.nan, 1e9]), {"CDF_DATA_TYPE": cdf_type},
                                                    unix_time_to_cdf_time=True, nan_to_fillval=nan_to_fillval)
                    self.assertEqual(written.type.name, cdf_type)
                    np.testing.assert_array_equal(pycdfpp.to_datetime64(written), expected)
                    self.assertEqual(written.values_encoded.tolist()[1], RAW_FILLS[cdf_type])
                    (self.root / "ours.cdf").unlink()

    def test_empty_dimension_besides_records_raises(self):
        """cdflib writes a dimension of size 0, a file it can't read back."""
        dataset = xr.Dataset({"e": xr.Variable(("x", "y"), np.zeros((3, 0)))})
        outcome = Outcome(ours_module(), dataset, self.root / "ours.cdf", istp=False, record_dimensions=["e"])
        self.assertIsInstance(outcome.error, ValueError)
        self.assertFalse(outcome.written)

    def test_compression_skips_epochs_and_listed_variables(self):
        Outcome(ours_module(), build("compliant"), self.root / "ours.cdf", compression=6,
                compression_skip_vars=["comp"])
        compressed = {name: c != pycdfpp.CompressionType.no_compression
                      for name, c in compressions(self.root / "ours.cdf").items()}
        self.assertEqual(compressed, {"B": True, "b_label": True, "Epoch": False, "comp": False})


if __name__ == "__main__":
    unittest.main()
