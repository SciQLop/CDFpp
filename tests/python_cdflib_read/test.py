"""pycdfpp.cdflib.cdfread.CDF against the real cdflib.cdfread.CDF on the test corpus."""
import dataclasses
import shutil
import tempfile
import unittest
from unittest import mock
from pathlib import Path

import numpy as np

import cdflib
import pycdfpp
from pycdfpp.cdflib import cdfread

RESOURCES = Path(__file__).resolve().parent.parent / "resources"
REJECTED_BY_CDFLIB = ("not_a_cdf.cdf",)
CORPUS = sorted(p.name for p in RESOURCES.glob("*.cdf") if p.name not in REJECTED_BY_CDFLIB)
OFFSET_FIELDS = ("next_vdr_location", "head_vxr", "last_vxr", "next_adr_loc", "first_z_entry", "first_gr_entry")
UTF8_FILES = ("testutf8.cdf",)
# Records a sparse variable doesn't store: cdflib gives the pad value where NASA's library (3.8+) gives FILLVAL,
# and zeros between pad values when a record holds several values. pycdfpp's own values are NASA's.
FOLLOWS_NASA = {("sparse_records.cdf", "pad_with_fillval"), ("sparse_records.cdf", "prev_with_fillval"),
                ("testutf8.cdf", "Temp")}


def raised_or_returned(call):
    try:
        return call()
    except Exception as error:  # the type is what both libraries must agree on
        return ("raised", type(error))


def _same_array(a, b):
    if a.dtype.kind in "fc":
        return np.array_equal(a, b, equal_nan=True)
    return np.array_equal(a, b)


def _same_scalar(a, b):
    if isinstance(a, (float, complex, np.floating, np.complexfloating)):
        return a == b or (np.isnan(a) and np.isnan(b))
    return a == b


class SameMixin:
    def assertSame(self, ours, theirs, where=""):
        if dataclasses.is_dataclass(theirs) and not isinstance(theirs, type):
            self.assertIs(type(ours), type(theirs), where)
            for field in dataclasses.fields(theirs):
                if field.name not in OFFSET_FIELDS:
                    self.assertSame(getattr(ours, field.name), getattr(theirs, field.name), f"{where}.{field.name}")
        elif isinstance(theirs, dict):
            self.assertIsInstance(ours, dict, where)
            self.assertEqual(list(ours), list(theirs), where)
            for key in theirs:
                self.assertSame(ours[key], theirs[key], f"{where}[{key!r}]")
        elif isinstance(theirs, (list, tuple)):
            self.assertIs(type(ours), type(theirs), where)
            self.assertEqual(len(ours), len(theirs), where)
            for index, (a, b) in enumerate(zip(ours, theirs)):
                self.assertSame(a, b, f"{where}[{index}]")
        elif isinstance(theirs, np.ndarray):
            self.assertIsInstance(ours, np.ndarray, where)
            self.assertEqual((ours.dtype, ours.shape), (theirs.dtype, theirs.shape), where)
            self.assertTrue(_same_array(ours, theirs), f"{where}: {ours!r} != {theirs!r}")
        else:
            self.assertIs(type(ours), type(theirs), f"{where}: {ours!r} vs {theirs!r}")
            self.assertTrue(_same_scalar(ours, theirs), f"{where}: {ours!r} != {theirs!r}")

    def assertSameCall(self, ours, theirs, call, where):
        self.assertSame(raised_or_returned(lambda: call(ours)), raised_or_returned(lambda: call(theirs)), where)
    def compare_values(self, ours, theirs, name, variable):
        if (name, variable) in FOLLOWS_NASA:
            self.assertSame(ours.varget(variable), pycdfpp.load(RESOURCES / name)[variable].values, variable)
        else:
            self.assertSameCall(ours, theirs, lambda c: c.varget(variable), f"{name}:varget({variable})")


def latin1_named_cdf(folder):
    """A CDF whose variable and attributes are named "café" in ISO-8859-1, as old files can be."""
    cdf = pycdfpp.CDF()
    cdf.add_variable("cafe", values=np.arange(3, dtype=np.float64))
    cdf["cafe"].add_attribute("units", "cafe")
    cdf.add_attribute("cafe_attr", ["cafe"])
    path = folder / "latin1.cdf"
    pycdfpp.save(cdf, str(path))
    path.write_bytes(path.read_bytes().replace(b"cafe", b"caf\xe9"))
    return path


def open_both(name, **kwargs):
    path = RESOURCES / name
    return cdfread.CDF(path, **kwargs), cdflib.CDF(path, **kwargs)


def encoding_of(name):
    return "utf-8" if name in UTF8_FILES else "ascii"


class Corpus(SameMixin, unittest.TestCase):
    def compare_file(self, name, string_encoding):
        ours, theirs = open_both(name, string_encoding=string_encoding)
        info = theirs.cdf_info()
        self.compare_info(ours, theirs, name)
        for variable in info.rVariables + info.zVariables:
            for method in ("varinq", "varattsget", "vdr_info"):
                self.assertSameCall(ours, theirs, lambda c: getattr(c, method)(variable), f"{name}:{method}({variable})")
            self.compare_values(ours, theirs, name, variable)
        for attribute in [next(iter(a)) for a in info.Attributes]:
            self.compare_attribute(ours, theirs, attribute, info, name)
        self.assertSameCall(ours, theirs, lambda c: c.globalattsget(), f"{name}:globalattsget")

    def compare_info(self, ours, theirs, name):
        our_info, their_info = ours.cdf_info(), theirs.cdf_info()
        self.assertEqual(our_info.CDF, (RESOURCES / name).resolve())
        if not their_info.Compressed:  # cdflib reports the path of a temporary decompressed copy
            self.assertEqual(our_info.CDF, their_info.CDF)
        self.assertSame(dataclasses.replace(our_info, CDF=None), dataclasses.replace(their_info, CDF=None), name)

    def compare_attribute(self, ours, theirs, attribute, info, name):
        self.assertSameCall(ours, theirs, lambda c: c.attinq(attribute), f"{name}:attinq({attribute})")
        adr = theirs.attinq(attribute)
        entries = range(-1, adr.max_gr_entry + 2) if adr.scope == 1 else info.rVariables + info.zVariables
        for entry in entries:
            self.assertSameCall(ours, theirs, lambda c: c.attget(attribute, entry), f"{name}:attget({attribute}, {entry})")

    def test_corpus(self):
        for name in CORPUS:
            with self.subTest(name):
                self.compare_file(name, encoding_of(name))

    def test_corpus_by_number(self):
        for name in CORPUS:
            with self.subTest(name):
                ours, theirs = open_both(name)
                info = theirs.cdf_info()
                count = max(len(info.rVariables), len(info.zVariables))
                for number in range(-1, count + 1):
                    for method in ("varinq", "varattsget"):
                        self.assertSameCall(ours, theirs, lambda c: getattr(c, method)(number), f"{name}:{method}({number})")
                for number in range(count):
                    self.assertSame(ours.varget(number), ours.varget(theirs.varinq(number).Variable), f"{name}:{number}")
                self.compare_attributes_by_number(ours, theirs, info, name)

    def compare_attributes_by_number(self, ours, theirs, info, name):
        """cdflib checks attinq numbers against the zVariable count: compared with attinq by name instead."""
        for number, attribute in enumerate(next(iter(a)) for a in info.Attributes):
            self.assertSame(ours.attinq(number), theirs.attinq(attribute), f"{name}:attinq({number})")
        for number in (-1, len(info.Attributes)):
            with self.assertRaises(KeyError):
                ours.attinq(number)

    def test_files_cdflib_rejects(self):
        for name in REJECTED_BY_CDFLIB:
            with self.subTest(name):
                with self.assertRaises(OSError):
                    cdflib.CDF(RESOURCES / name)
                with self.assertRaises(OSError):
                    cdfread.CDF(RESOURCES / name)


def read_everything(path):
    """Opens, reads and drops the reader: Windows can't delete a file a live reader holds."""
    cdf = cdfread.CDF(path)
    cdf.cdf_info(), cdf.varinq(0), cdf.varattsget(0)


class Targeted(SameMixin, unittest.TestCase):
    def test_record_ranges(self):
        ours, theirs = open_both("a_cdf.cdf")
        ranges = [dict(startrec=0, endrec=0), dict(startrec=50, endrec=50), dict(startrec=100, endrec=100),
                  dict(startrec=10), dict(endrec=20), dict(startrec=3, endrec=200), dict(startrec=-1),
                  dict(startrec=20, endrec=10), dict(endrec=-1)]
        for variable in ("var", "var3d", "epoch16", "var_recvary_string", "var_string", "var2d_string"):
            for record_range in ranges:
                self.assertSameCall(ours, theirs, lambda c: c.varget(variable, **record_range),
                                    f"varget({variable}, {record_range})")

    def test_time_ranges(self):
        ours, theirs = open_both("ac_h2_sis_20101105_v06.cdf")
        epochs = theirs.varget("Epoch")
        start, end = list(cdflib.cdfepoch.breakdown(epochs[3])), list(cdflib.cdfepoch.breakdown(epochs[7]))
        before = [1990, 1, 1, 0, 0, 0, 0]
        calls = {
            "depend_0": lambda c: c.varget("H_lo", starttime=start, endtime=end),
            "explicit epoch": lambda c: c.varget("H_lo", epoch="Epoch", starttime=start, endtime=end),
            "epoch itself": lambda c: c.varget("Epoch", starttime=start),
            "only end": lambda c: c.varget("H_lo", endtime=end),
            "no record": lambda c: c.varget("H_lo", starttime=before, endtime=before),
            "time and records": lambda c: c.varget("H_lo", starttime=start, startrec=1),
        }
        for label, call in calls.items():
            self.assertSameCall(ours, theirs, call, label)

    def test_unknown_names_and_numbers(self):
        ours, theirs = open_both("a_cdf.cdf")
        calls = {
            "varget name": lambda c: c.varget("nope"), "varget none": lambda c: c.varget(),
            "varget number": lambda c: c.varget(999), "varinq name": lambda c: c.varinq("nope"),
            "vdr_info float": lambda c: c.vdr_info(1.5), "varattsget name": lambda c: c.varattsget("nope"),
            "varattsget number": lambda c: c.varattsget(-1), "attinq name": lambda c: c.attinq("nope"),
            "attinq negative": lambda c: c.attinq(-1), "attinq float": lambda c: c.attinq(1.5),
            "attget name": lambda c: c.attget("nope", 0), "attget number": lambda c: c.attget(999, 0),
            "attget float": lambda c: c.attget(1.5, 0), "attget entry not int": lambda c: c.attget(0, "var"),
            "global entry not int": lambda c: c.attget("attr", "var"), "global entry none": lambda c: c.attget("attr"),
            "global entry too big": lambda c: c.attget("attr", 5), "unknown variable entry": lambda c: c.attget("var_attr", "nope"),
            "variable entry without value": lambda c: c.attget("var_attr", "epoch"),
            "case insensitive": lambda c: c.attget(" ATTR ", 0), "case insensitive variable": lambda c: c.varget(" VAR "),
            "variable attribute by number": lambda c: c.attget("var_attr", 0),
        }
        for label, call in calls.items():
            self.assertSameCall(ours, theirs, call, label)

    def test_both_variable_kinds_by_number(self):
        ours, theirs = open_both("ac_h0_mfi_00000000_v01.cdf")
        for label, call in {"varget": lambda c: c.varget(0), "attget": lambda c: c.attget("FIELDNAM", 0)}.items():
            self.assertSameCall(ours, theirs, call, label)

    def test_string_encoding(self):
        for encoding in ("ascii", "utf-8", "latin-1"):
            with self.subTest(encoding):
                ours, theirs = open_both("testutf8.cdf", string_encoding=encoding)
                for variable in theirs.cdf_info().zVariables:
                    self.compare_values(ours, theirs, "testutf8.cdf", variable)
                    self.assertSameCall(ours, theirs, lambda c: c.varattsget(variable), variable)
                self.assertSameCall(ours, theirs, lambda c: c.globalattsget(), "globalattsget")

    def test_validate(self):
        ours, theirs = open_both("checksum.cdf", validate=True)
        self.assertSame(ours.varget(0), theirs.varget(0))

    def test_validate_flipped_byte(self):
        with tempfile.TemporaryDirectory() as folder:
            corrupted = Path(folder) / "checksum.cdf"
            shutil.copy(RESOURCES / "checksum.cdf", corrupted)
            content = bytearray(corrupted.read_bytes())
            content[-100] ^= 0xFF
            corrupted.write_bytes(bytes(content))
            for reader in (cdflib.CDF, cdfread.CDF):
                with self.subTest(reader.__module__):
                    with self.assertRaises(OSError):
                        reader(corrupted, validate=True)
                    reader(corrupted)

    def test_paths(self):
        path = RESOURCES / "a_cdf.cdf"
        for opened in (str(path), path, RESOURCES / "a_cdf", str(RESOURCES / "a_cdf")):
            with self.subTest(str(opened)):
                self.assertEqual(cdfread.CDF(opened).file, cdflib.CDF(opened).file)

    def test_missing_file(self):
        for reader in (cdflib.CDF, cdfread.CDF):
            with self.subTest(reader.__module__):
                with self.assertRaises(FileNotFoundError):
                    reader(RESOURCES / "missing.cdf")

    def test_latin1_names(self):
        with tempfile.TemporaryDirectory() as folder:
            path = latin1_named_cdf(Path(folder))
            for encoding in ("latin-1", "ascii"):
                with self.subTest(encoding):
                    ours, theirs = cdfread.CDF(path, string_encoding=encoding), cdflib.CDF(path, string_encoding=encoding)
                    calls = {"info": lambda c: c.cdf_info().zVariables, "varget": lambda c: c.varget("caf\xe9"),
                             "varattsget": lambda c: c.varattsget(0), "globalattsget": lambda c: c.globalattsget(),
                             "attget": lambda c: c.attget("caf\xe9_attr", 0)}
                    for label, call in calls.items():
                        self.assertSameCall(ours, theirs, call, label)
                    del ours, theirs  # both hold the file: Windows can't delete it while they live

    def test_descriptors_from_bytes(self):
        from pycdfpp.cdflib import _records
        for name in ("a_cdf.cdf", "a_compressed_cdf.cdf", "ge_k0_cpi_19921231_v02.cdf"):
            with self.subTest(name):
                content = (RESOURCES / name).read_bytes()
                descriptors = _records.Descriptors(content, 3 if content[:4].hex() == "cdf30001" else 2)
                info = cdflib.CDF(RESOURCES / name).cdf_info()
                names = [vdr["Name"].split(b"\x00")[0].decode() for vdr in descriptors.rvdrs + descriptors.zvdrs]
                self.assertEqual(names, info.rVariables + info.zVariables)

    def test_with_block_and_getitem(self):
        with cdfread.CDF(RESOURCES / "a_cdf.cdf") as cdf:
            self.assertSame(cdf["var"], cdflib.CDF(RESOURCES / "a_cdf.cdf").varget("var"))

    def test_rvariable_with_a_non_varying_first_dimension(self):
        """cdflib reads it as (4,), the first value of each record: the values cdflib's own writer was given are lost."""
        written = np.arange(12, dtype=np.float64).reshape(4, 3)
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "r.cdf"
            writer = cdflib.cdfwrite.CDF(path, cdf_spec={"rDim_sizes": [2, 3]})
            writer.write_var({"Variable": "r", "Data_Type": 45, "Num_Elements": 1, "Rec_Vary": True,
                              "Var_Type": "rVariable", "Dim_Vary": [False, True]}, None, written)
            writer.close()
            cdf = cdfread.CDF(path)
            self.assertSame(cdf.varget("r"), written)
            del cdf  # releases the mapped file before the folder goes

    def test_compressed_file_is_decompressed_once(self):
        """Reading the descriptors decompressed the file: pycdfpp loads those bytes, not the compressed file."""
        loaded, real_load = [], pycdfpp.load

        def load(source, *args, **kwargs):
            loaded.append(bytes(source[:8]) if isinstance(source, (bytes, bytearray, memoryview)) else source)
            return real_load(source, *args, **kwargs)

        with mock.patch.object(cdfread.pycdfpp, "load", load):
            cdf = cdfread.CDF(RESOURCES / "a_compressed_cdf.cdf", validate=True)
            values = cdf.varget("var")
        self.assertEqual(loaded, [bytes.fromhex("cdf300010000ffff")])
        self.assertSame(values, cdflib.CDF(RESOURCES / "a_compressed_cdf.cdf").varget("var"))

    def test_truncated_file_raises_ValueError(self):
        """As cdflib, except a compressed file cut to its magic numbers: cdflib raises OverflowError."""
        with tempfile.TemporaryDirectory() as folder:
            for name in ("a_cdf.cdf", "a_compressed_cdf.cdf"):
                for size in (8, 40, 100, 400, 1000, 3000):
                    with self.subTest(name, size=size):
                        path = Path(folder) / f"{size}_{name}"
                        path.write_bytes((RESOURCES / name).read_bytes()[:size])
                        with self.assertRaises(ValueError):
                            read_everything(path)

    def test_varget_gives_a_new_array_each_time(self):
        cdf = cdfread.CDF(RESOURCES / "a_cdf.cdf")
        cdf.varget("var")[:] = 0
        self.assertNotEqual(cdf.varget("var")[1], 0)


if __name__ == "__main__":
    unittest.main()
