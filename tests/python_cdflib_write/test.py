"""pycdfpp.cdflib.cdfwrite.CDF: files it writes read back right through the real cdflib and pycdfpp."""
import os
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

import numpy as np

import cdflib
import pycdfpp
from pycdfpp.cdflib import cdfwrite

RESOURCES = Path(__file__).resolve().parent.parent / "resources"
CORPUS = sorted(p.name for p in RESOURCES.glob("*.cdf") if p.name != "not_a_cdf.cdf")
# Values cdflib's reader gives differently from NASA's library (see tests/python_cdflib_read): files rebuilt
# from what cdflib reads hold cdflib's values.
READ_DIFFERENTLY_BY_CDFLIB = {("sparse_records.cdf", "pad_with_fillval"), ("sparse_records.cdf", "prev_with_fillval"),
                              ("testutf8.cdf", "Temp")}
TYPES = {"CDF_INT1": "i1", "CDF_INT2": "i2", "CDF_INT4": "i4", "CDF_INT8": "i8", "CDF_UINT1": "u1",
         "CDF_UINT2": "u2", "CDF_UINT4": "u4", "CDF_REAL4": "f4", "CDF_REAL8": "f8", "CDF_BYTE": "i1",
         "CDF_FLOAT": "f4", "CDF_DOUBLE": "f8", "CDF_EPOCH": "f8", "CDF_EPOCH16": "c16",
         "CDF_TIME_TT2000": "i8"}
NUMBERS = {name: number for number, name in ((1, "CDF_INT1"), (2, "CDF_INT2"), (4, "CDF_INT4"), (8, "CDF_INT8"),
                                              (11, "CDF_UINT1"), (12, "CDF_UINT2"), (14, "CDF_UINT4"),
                                              (21, "CDF_REAL4"), (22, "CDF_REAL8"), (31, "CDF_EPOCH"),
                                              (32, "CDF_EPOCH16"), (33, "CDF_TIME_TT2000"), (41, "CDF_BYTE"),
                                              (44, "CDF_FLOAT"), (45, "CDF_DOUBLE"), (51, "CDF_CHAR"),
                                              (52, "CDF_UCHAR"))}


def find_cdfvalidate():
    candidates = (os.environ.get("CDFVALIDATE"), shutil.which("cdfvalidate"),
                  str(Path.home() / ".local" / "cdf" / "bin" / "cdfvalidate"))
    return next((c for c in candidates if c and Path(c).is_file()), None)


def spec(name, data_type, dims=(), num_elements=1, **extra):
    return {"Variable": name, "Data_Type": data_type, "Num_Elements": num_elements, "Rec_Vary": True,
            "Dim_Sizes": list(dims), **extra}


def same(testcase, ours, theirs, where):
    if isinstance(theirs, np.ndarray):
        testcase.assertIsInstance(ours, np.ndarray, where)
        testcase.assertEqual((ours.dtype, ours.shape), (theirs.dtype, theirs.shape), where)
        testcase.assertTrue(np.array_equal(ours, theirs, equal_nan=theirs.dtype.kind in "fc"),
                            f"{where}: {ours!r} != {theirs!r}")
    elif isinstance(theirs, dict):
        testcase.assertEqual(sorted(ours), sorted(theirs), where)
        for key in theirs:
            same(testcase, ours[key], theirs[key], f"{where}[{key!r}]")
    elif isinstance(theirs, list):
        testcase.assertEqual(len(ours), len(theirs), where)
        for index, (a, b) in enumerate(zip(ours, theirs)):
            same(testcase, a, b, f"{where}[{index}]")
    else:
        testcase.assertIs(type(ours), type(theirs), f"{where}: {ours!r} vs {theirs!r}")
        testcase.assertTrue(ours == theirs or (ours != ours and theirs != theirs), f"{where}: {ours!r} != {theirs!r}")


def typed_global_entries(source, name):
    """{entry number: [value, type name]} of a global attribute, as attget reads them."""
    last = source.attinq(name).max_gr_entry
    entries = {}
    for number in range(last + 1):
        try:
            entry = source.attget(name, number)
        except (KeyError, ValueError):
            continue
        entries[number] = [entry.Data, entry.Data_Type]
    return entries


def typed_variable_attributes(source, variable):
    return {name: [value, source.attget(name, variable).Data_Type]
            for name, value in source.varattsget(variable).items()}


def cdf_spec_of(info):
    return {"Majority": info.Majority, "Encoding": info.Encoding, "Checksum": info.Checksum,
            "rDim_sizes": info.rDim_sizes, "Compressed": info.Compressed}


def var_spec_of(inquiry):
    return {key: getattr(inquiry, key) for key in ("Variable", "Data_Type", "Num_Elements", "Rec_Vary", "Var_Type",
                                                   "Dim_Sizes", "Dim_Vary", "Sparse", "Compress", "Block_Factor",
                                                   "Pad")}


def var_data_of(data, inquiry):
    if data is None or inquiry.Sparse == "No_sparse":
        return data
    return [list(range(len(data))), data]


def rebuild(source_path, out_path):
    """Writes source_path again through our writer, from what the real cdflib reads in it."""
    source = cdflib.CDF(source_path)
    info = source.cdf_info()
    with cdfwrite.CDF(out_path, cdf_spec=cdf_spec_of(info)) as out:
        out.write_globalattrs({name: typed_global_entries(source, name) for name in source.globalattsget()})
        for variable in info.zVariables + info.rVariables:
            inquiry = source.varinq(variable)
            out.write_var(var_spec_of(inquiry), typed_variable_attributes(source, variable),
                          var_data_of(source.varget(variable), inquiry))


def varying_dims(inquiry):
    if inquiry.Var_Type == "zVariable":
        return inquiry.Dim_Sizes
    return [size for size, vary in zip(inquiry.Dim_Sizes, inquiry.Dim_Vary) if vary]


class CorpusRoundTrip(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.folder = tempfile.TemporaryDirectory()
        cls.rebuilt = {name: Path(cls.folder.name) / name for name in CORPUS}
        for name, path in cls.rebuilt.items():
            rebuild(RESOURCES / name, path)

    @classmethod
    def tearDownClass(cls):
        cls.folder.cleanup()

    def test_cdflib_reads_back_what_it_read(self):
        for name in CORPUS:
            with self.subTest(name):
                original, rebuilt = cdflib.CDF(RESOURCES / name), cdflib.CDF(self.rebuilt[name])
                before, after = original.cdf_info(), rebuilt.cdf_info()
                self.assertEqual(after.zVariables, before.zVariables + before.rVariables)
                self.assertEqual((after.Majority, after.Encoding, after.Checksum, after.Compressed),
                                 (before.Majority, before.Encoding, before.Checksum, before.Compressed))
                same(self, rebuilt.globalattsget(), original.globalattsget(), f"{name}: globalattsget")
                for variable in after.zVariables:
                    self.compare_variable(original, rebuilt, variable, f"{name}:{variable}")

    def compare_variable(self, original, rebuilt, variable, where):
        before, after = original.varinq(variable), rebuilt.varinq(variable)
        self.assertEqual((after.Data_Type, after.Num_Elements, bool(after.Rec_Vary), after.Sparse),
                         (before.Data_Type, before.Num_Elements, bool(before.Rec_Vary), before.Sparse), where)
        self.assertEqual(after.Dim_Sizes, varying_dims(before), where)
        same(self, rebuilt.varget(variable), original.varget(variable), f"{where}: varget")
        same(self, rebuilt.varattsget(variable), original.varattsget(variable), f"{where}: varattsget")

    def test_pycdfpp_reads_back_the_same_values(self):
        for name in CORPUS:
            with self.subTest(name):
                original, rebuilt = pycdfpp.load(RESOURCES / name), pycdfpp.load(self.rebuilt[name])
                for variable in original:
                    if (name, variable) not in READ_DIFFERENTLY_BY_CDFLIB:
                        self.assertSameValues(original[variable].values_encoded, rebuilt[variable].values_encoded,
                                              variable)

    def assertSameValues(self, a, b, where):
        """Every value, ignoring the record shape: rVariables lose their non varying dimensions."""
        self.assertEqual(a.dtype, b.dtype, where)
        if a.dtype.kind == "V":
            self.assertEqual(a.tobytes(), b.tobytes(), where)
        else:
            self.assertTrue(np.array_equal(a.ravel(), b.ravel(), equal_nan=a.dtype.kind in "fc"), where)

    @unittest.skipUnless(find_cdfvalidate(), "NASA's cdfvalidate not found")
    def test_nasa_cdfvalidate_accepts_every_file(self):
        cdfvalidate = find_cdfvalidate()
        for name, path in self.rebuilt.items():
            with self.subTest(name):
                result = subprocess.run([cdfvalidate, str(path)], capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                self.assertNotIn("error", result.stdout.lower(), result.stdout)


class Writer(unittest.TestCase):
    def setUp(self):
        self.folder = tempfile.TemporaryDirectory()
        self.path = Path(self.folder.name) / "out.cdf"

    def tearDown(self):
        self.folder.cleanup()

    def write(self, *var_specs_and_data, cdf_spec=None, global_attrs=None):
        with cdfwrite.CDF(self.path, cdf_spec=cdf_spec) as out:
            if global_attrs is not None:
                out.write_globalattrs(global_attrs)
            for var_spec, var_data in var_specs_and_data:
                out.write_var(var_spec, var_data=var_data)
        return cdflib.CDF(self.path)

    def test_every_data_type_as_number_and_name(self):
        for type_name, dtype in TYPES.items():
            for data_type in (NUMBERS[type_name], type_name):
                with self.subTest(data_type):
                    values = np.arange(6).reshape(3, 2).astype(dtype)
                    read = self.write((spec("v", data_type, [2]), values))
                    self.assertEqual(read.varinq("v").Data_Type, NUMBERS[type_name])
                    same(self, read.varget("v"), values, type_name)
                    self.path.unlink()

    def test_strings(self):
        for type_name in ("CDF_CHAR", "CDF_UCHAR"):
            with self.subTest(type_name):
                read = self.write((spec("s", type_name, num_elements=5), ["ab", "cdefg", "h"]))
                self.assertEqual(read.varinq("s").Num_Elements, 5)
                same(self, read.varget("s"), np.array(["ab", "cdefg", "h"]), type_name)
                self.path.unlink()

    def test_string_longer_than_num_elements_raises(self):
        with self.assertRaises(ValueError):
            self.write((spec("s", "CDF_CHAR", num_elements=2), ["abc"]))

    def test_data_as_list_scalar_and_array(self):
        read = self.write((spec("list", 45), [1.0, 2.0, 3.0]), (spec("scalar", 4), 7),
                          (spec("array", 4, [3]), np.arange(6, dtype=np.int32).reshape(2, 3)),
                          (spec("bytes", 4), np.arange(3, dtype=np.int32).tobytes()))
        same(self, read.varget("list"), np.array([1.0, 2.0, 3.0]), "list")
        same(self, read.varget("scalar"), np.array([7], dtype=np.int32), "scalar")
        same(self, read.varget("array"), np.arange(6, dtype=np.int32).reshape(2, 3), "array")
        same(self, read.varget("bytes"), np.arange(3, dtype=np.int32), "bytes")

    def test_datetime64_into_tt2000_keeps_the_instants(self):
        """cdflib stores datetime64 as nanoseconds since 1970: its dates come back 30 years late."""
        times = np.array(["2016-12-31T23:59:59.5", "2024-02-29T12:00:00.123456789"], dtype="datetime64[ns]")
        read = self.write((spec("Epoch", 33), times))
        same(self, cdflib.cdfepoch.to_datetime(read.varget("Epoch")), times, "Epoch")

    def test_raw_time_values_are_stored_as_they_are(self):
        tt2000 = np.array([-9223372036854775808, 0, 504878469184000000], dtype=np.int64)
        epoch16 = np.array([63745056000 + 5j, -1e31 - 1e31j])
        read = self.write((spec("tt", 33), tt2000), (spec("e16", 32), epoch16), (spec("e", 31), [-1e31, 6.3e13]))
        same(self, read.varget("tt"), tt2000, "tt")
        same(self, read.varget("e16"), epoch16, "e16")
        same(self, read.varget("e"), np.array([-1e31, 6.3e13]), "e")

    def test_compression(self):
        read = self.write((spec("v", 45, Compress=6), np.arange(1000.)), (spec("plain", 45, Compress=0), [1.0]),
                          cdf_spec={"Compressed": True})
        self.assertTrue(read.cdf_info().Compressed)
        self.assertEqual((read.varinq("v").Compress, read.varinq("plain").Compress), (6, 0))
        same(self, read.varget("v"), np.arange(1000.), "v")

    def test_checksum(self):
        self.write((spec("v", 45), np.arange(10.)), cdf_spec={"Checksum": True})
        read = cdflib.CDF(self.path, validate=True)
        self.assertTrue(read.cdf_info().Checksum)
        same(self, read.varget("v"), np.arange(10.), "v")

    def test_big_endian_encoding(self):
        for encoding in ("NETWORK_ENCODING", 1):
            with self.subTest(encoding):
                read = self.write((spec("v", 22, [2]), np.arange(4.).reshape(2, 2)), cdf_spec={"Encoding": encoding})
                self.assertEqual(read.cdf_info().Encoding, 1)
                same(self, read.varget("v"), np.arange(4.).reshape(2, 2), "v")
                self.path.unlink()

    def test_bad_encodings_raise(self):
        for encoding in (-1, "nonsense", "VAX_ENCODING"):
            with self.subTest(encoding), self.assertRaises(OSError):
                cdfwrite.CDF(self.path, cdf_spec={"Encoding": encoding})

    def test_column_major_keeps_values(self):
        """cdflib writes C-ordered bytes into column major files: its records come back transposed."""
        values = np.arange(12, dtype=np.int32).reshape(2, 2, 3)
        read = self.write((spec("v", 4, [2, 3]), values), cdf_spec={"Majority": "Column_major"})
        self.assertEqual(read.cdf_info().Majority, "Column_major")
        same(self, read.varget("v"), values, "v")

    def test_bad_majority_raises(self):
        with self.assertRaises(RuntimeError):
            cdfwrite.CDF(self.path, cdf_spec={"Majority": "diagonal"})

    def test_non_record_varying(self):
        read = self.write(({**spec("labels", 51, [3], num_elements=2), "Rec_Vary": False}, ["Bx", "By", "Bz"]),
                          ({**spec("energy", 44, [3]), "Rec_Vary": False}, np.array([1., 10., 100.], np.float32)))
        self.assertFalse(read.varinq("energy").Rec_Vary)
        same(self, read.varget("labels"), np.array(["Bx", "By", "Bz"]), "labels")
        same(self, read.varget("energy"), np.array([1., 10., 100.], np.float32), "energy")

    def test_empty_variable_keeps_its_shape(self):
        read = self.write((spec("empty", 21, [3]), None))
        self.assertEqual((read.varinq("empty").Dim_Sizes, read.varinq("empty").Last_Rec), ([3], -1))

    def test_sparse_records_with_virtual_ones(self):
        data = np.arange(6.)
        read = self.write((spec("v", 45, Sparse="pad_sparse", Pad=np.array([-5.0])), [[0, 1, 4], data[[0, 1, 4]]]),
                          (spec("w", 45, Sparse="prev_sparse"), [[0, 2], data[:3]]))
        self.assertEqual((read.varinq("v").Sparse, read.varinq("w").Sparse), ("Pad_sparse", "Prev_sparse"))
        same(self, read.varget("v"), np.array([0., 1., -5., -5., 4.]), "v")
        same(self, read.varget("w"), np.array([0., 0., 2.]), "w")

    def test_global_attribute_entries(self):
        read = self.write(global_attrs={"G": {0: "text", 3: [12, "CDF_INT4"]}, "H": {0: 1.5, 1: [1, 2]},
                                        "Empty": None})
        same(self, read.globalattsget(), {"G": ["text", np.int32(12)], "H": [np.float64(1.5), np.array([1, 2])]},
             "globalattsget")
        self.assertEqual(read.attget("G", 1).Data_Type, "CDF_INT4")
        self.assertEqual(read.attinq("Empty").num_gr_entry, 0)

    def test_untyped_values_cdflib_cannot_store_are_skipped(self):
        """No wrong number: uint64 (also Python ints from 2**63), long doubles, numpy booleans and
        complex64 have no CDF type cdflib guesses, it skips them; small unsigned keep their size."""
        with self.assertLogs("cdflib.logging", "WARNING"):
            with cdfwrite.CDF(self.path) as out:
                out.write_var(spec("v", 44), {"U64": np.uint64(2**40 + 5), "U64S": np.array([2**33], np.uint64),
                                              "BIG": 2**63 + 1, "BIGS": [1, 2**63], "LD": np.longdouble(1) / 3,
                                              "B": np.bool_(True), "C64": np.complex64(1j), "U8": np.uint8(7),
                                              "U16": np.array([7], np.uint16), "U32": np.uint32(2**31 + 1),
                                              "PYBOOL": True}, np.arange(3.))
        same(self, cdflib.CDF(self.path).varattsget("v"),
             {"U8": np.uint8(7), "U16": np.uint16(7), "U32": np.uint32(2**31 + 1), "PYBOOL": np.int64(1)}, "v")

    @unittest.expectedFailure
    def test_several_strings_in_one_entry(self):
        """pycdfpp's saver counts the strings of an entry by '\\n', NASA separates them with "\\N ":
        cdflib reads one joined string. Needs a C++ fix."""
        with cdfwrite.CDF(self.path) as out:
            out.write_var(spec("v", 44), None, np.arange(3.))
            out.write_variableattrs({"TEXTS": {"v": [["a", "b"], "CDF_CHAR"]}})
        same(self, cdflib.CDF(self.path).attget("TEXTS", "v").Data, np.array(["a", "b"]), "TEXTS")

    def test_string_pad_list_uses_its_first_item(self):
        read = self.write((spec("s", 51, num_elements=3, Pad=["xy", "z"]), ["abc"]))
        self.assertEqual(read.varinq("s").Pad, "xy")

    def test_sparse_data_shorter_than_its_records_raises(self):
        with cdfwrite.CDF(self.path) as out:
            for records, data in (([0, 1, 4], [1.0, 2.0]), ([0, 5], [1.0, 2.0, 3.0])):
                with self.subTest(records), self.assertRaisesRegex(ValueError, "less data than"):
                    out.write_var(spec(f"v{len(data)}", 45, Sparse="pad_sparse"), var_data=[records, data])

    def test_dropped_open_writer_warns(self):
        out = cdfwrite.CDF(self.path)
        with self.assertWarns(ResourceWarning):
            del out
        self.assertFalse(self.path.exists())

    def test_global_attribute_twice_raises(self):
        with cdfwrite.CDF(self.path) as out:
            out.write_globalattrs({"G": {0: "a"}})
            with self.assertRaises(ValueError):
                out.write_globalattrs({"G": {0: "b"}})

    def test_variable_attributes(self):
        with cdfwrite.CDF(self.path) as out:
            out.write_var(spec("v", 44), {"UNITS": "nT", "FILLVAL": [-1e31, "CDF_FLOAT"], "SCALE": 2.5, "N": 3,
                                          "LIST": [1, 2, 3], "TEXTS": ["a", "b"], "NONE": None}, np.arange(3.))
            out.write_var(spec("w", 44), None, np.arange(3.))
            out.write_variableattrs({"UNITS": {"w": "K", 0: "nT2"}, "OTHER": {1: [7, "CDF_INT2"]}})
        read = cdflib.CDF(self.path)
        same(self, read.varattsget("v"), {"UNITS": "nT2", "FILLVAL": np.float32(-1e31), "SCALE": np.float64(2.5),
                                          "N": np.int64(3), "LIST": np.array([1, 2, 3]), "TEXTS": "a\\N b",
                                          "NONE": ""}, "v")
        same(self, read.varattsget("w"), {"UNITS": "K", "OTHER": np.int16(7)}, "w")

    def test_variable_attributes_errors(self):
        with cdfwrite.CDF(self.path) as out:
            out.write_var(spec("v", 44), None, np.arange(3.))
            out.write_globalattrs({"G": {0: "a"}})
            for attrs, error in (({"UNITS": {"nope": "x"}}, KeyError), ({"UNITS": {5: "x"}}, ValueError),
                                 ({"G": {"v": "x"}}, ValueError), ({3: {"v": "x"}}, ValueError),
                                 ([], ValueError)):
                with self.subTest(attrs), self.assertRaises(error):
                    out.write_variableattrs(attrs)

    def test_write_var_errors(self):
        with cdfwrite.CDF(self.path) as out:
            out.write_var(spec("v", 44), None, np.arange(3.))
            for var_spec, error in ((spec("v", 44), ValueError), ([], TypeError),
                                    ({"Variable": "x", "Data_Type": 44}, ValueError),
                                    (spec("x", 44, num_elements=2), ValueError), (spec("x", 51, num_elements=0), ValueError),
                                    ({**spec("x", 44), "Dim_Sizes": None}, ValueError),
                                    (spec("x", 99), ValueError), (spec("x", "CDF_NOPE"), ValueError)):
                with self.subTest(var_spec), self.assertRaises(error):
                    out.write_var(var_spec)

    def test_rvariables_are_written_as_zvariables(self):
        with cdfwrite.CDF(self.path, cdf_spec={"rDim_sizes": [3, 2]}) as out:
            out.write_var({"Variable": "r", "Data_Type": 4, "Num_Elements": 1, "Rec_Vary": True,
                           "Var_Type": "rVariable", "Dim_Vary": [True, False]},
                          {"UNITS": "m"}, np.arange(6, dtype=np.int32).reshape(2, 3))
            out.write_variableattrs({"OTHER": {0: "x"}})
            self.assertEqual((out.rvars, out.zvars, out.num_rdim), (["r"], [], 2))
        read = cdflib.CDF(self.path)
        self.assertEqual((read.varinq("r").Var_Type, read.varinq("r").Dim_Sizes), ("zVariable", [3]))
        same(self, read.varget("r"), np.arange(6, dtype=np.int32).reshape(2, 3), "r")
        same(self, read.varattsget("r"), {"UNITS": "m", "OTHER": "x"}, "r")

    def test_exception_in_with_block_writes_nothing(self):
        with self.assertRaises(ZeroDivisionError):
            with cdfwrite.CDF(self.path) as out:
                out.write_var(spec("v", 45), var_data=[1.0])
                1 / 0
        self.assertFalse(self.path.exists())

    def test_never_closed_writes_nothing(self):
        out = cdfwrite.CDF(self.path)
        out.write_var(spec("v", 45), var_data=[1.0])
        self.assertFalse(self.path.exists())
        out.close()
        self.assertTrue(self.path.exists())
        self.assertTrue(out.is_closed)

    def test_closed_writer_raises(self):
        out = cdfwrite.CDF(self.path)
        out.close()
        out.close()
        with self.assertRaises(OSError):
            out.write_var(spec("v", 45))

    def test_existing_path(self):
        self.write((spec("v", 45), [1.0]))
        with self.assertRaises(OSError):
            cdfwrite.CDF(self.path)
        with cdfwrite.CDF(self.path, delete=True) as out:
            out.write_var(spec("w", 45), var_data=[2.0])
        self.assertEqual(cdflib.CDF(self.path).cdf_info().zVariables, ["w"])

    def test_cdf_suffix_is_added(self):
        with cdfwrite.CDF(Path(self.folder.name) / "no_suffix") as out:
            self.assertEqual(out.path, Path(self.folder.name) / "no_suffix.cdf")
        self.assertTrue((Path(self.folder.name) / "no_suffix.cdf").exists())


if __name__ == "__main__":
    unittest.main()
