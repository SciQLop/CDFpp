import os
import subprocess
import dataclasses
import inspect
import sys
import tempfile
import types
import typing
import unittest
from pathlib import Path

import api_parity


# Loop variables of the class body that reads the leap second file: not API, and lsreader's
# repr holds a memory address.
LOADER_LEFTOVERS = ("lsfile", "lsreader", "csv_row", "row", "r")
CDFEPOCH_LOADER_LEFTOVERS = tuple("CDFepoch." + name for name in LOADER_LEFTOVERS)
# gzip_inflate is imported from cdflib's private _gzip_wrapper: not API.
CDFREAD_NOT_OURS = ("gzip_inflate",)
# xarray_to_cdf comes with the writer half of the xarray converters.
XARRAY_NOT_YET = ("xarray_to_cdf",)
RESOURCES = Path(__file__).resolve().parent.parent / "resources"


class CdflibApi(unittest.TestCase):
    def test_version(self):
        import pycdfpp.cdflib as cdflib
        self.assertEqual(cdflib.__version__, "1.3.16")

    def test_dataclasses_api(self):
        api_parity.assert_same_api(self, *api_parity.module_pair("dataclasses"))

    def test_dataclasses_is_the_real_module(self):
        import cdflib.dataclasses
        from pycdfpp.cdflib import dataclasses as ours
        self.assertIs(ours, cdflib.dataclasses)
        self.assertIs(ours.CDFInfo, cdflib.dataclasses.CDFInfo)

    def test_epochs_astropy_api(self):
        api_parity.assert_same_api(self, *api_parity.module_pair("epochs_astropy"))

    def test_epochs_api(self):
        api_parity.assert_same_api(self, *api_parity.module_pair("epochs"), ignored=CDFEPOCH_LOADER_LEFTOVERS)

    def test_cdfread_api(self):
        api_parity.assert_same_api(self, *api_parity.module_pair("cdfread"), ignored=CDFREAD_NOT_OURS)
        ours, theirs = api_parity.module_pair("cdfread")
        self.assertEqual(inspect.signature(ours.CDF.__del__), inspect.signature(theirs.CDF.__del__))

    def test_s3_utils_logging_api(self):
        for name in ("s3", "utils", "logging"):
            with self.subTest(name):
                api_parity.assert_same_api(self, *api_parity.module_pair(name))

    def test_cdfread_instance_attributes(self):
        import cdflib
        from pycdfpp.cdflib import cdfread
        for name in ("a_cdf.cdf", "a_compressed_cdf.cdf"):
            with self.subTest(name):
                path = RESOURCES / name
                api_parity.assert_same_instance_attributes(self, cdfread.CDF(path), cdflib.cdfread.CDF(path))

    def test_cdfwrite_api(self):
        api_parity.assert_same_api(self, *api_parity.module_pair("cdfwrite"))

    def test_cdfwrite_instance_attributes(self):
        import cdflib.cdfwrite
        from pycdfpp.cdflib import cdfwrite
        with tempfile.TemporaryDirectory() as folder:
            ours = cdfwrite.CDF(Path(folder) / "ours.cdf")
            theirs = cdflib.cdfwrite.CDF(Path(folder) / "theirs.cdf")
            api_parity.assert_same_instance_attributes(self, ours, theirs)
            ours.close()
            theirs.close()

    def test_xarray_api(self):
        api_parity.assert_same_api(self, *api_parity.module_pair("xarray"), ignored=XARRAY_NOT_YET)
        api_parity.assert_same_api(self, *api_parity.module_pair("xarray.cdf_to_xarray"),
                                   ignored=tuple("cdfepoch." + name for name in LOADER_LEFTOVERS))

    def test_package_exports_reader(self):
        import pycdfpp.cdflib as ours
        self.assertIs(ours.CDF, ours.cdfread.CDF)

    def test_missing_cdflib_message(self):
        code = "import sys; sys.modules['cdflib'] = None; import pycdfpp.cdflib"
        result = subprocess.run([sys.executable, "-c", code], capture_output=True, text=True,
                                env=dict(os.environ))
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("ImportError", result.stderr)
        self.assertIn("pip install pycdfpp[cdflib]", result.stderr)


def own(obj):
    obj.__module__ = "cdflib.fake"
    return obj


def fake_module(**names):
    module = types.ModuleType("cdflib.fake")
    vars(module).update(names)
    return module


@own
def func(a, b=1):
    pass


@own
def func_other_default(a, b=2):
    pass


@own
def func_other_name(a, c=1):
    pass


@own
class WithMethod:
    def method(self, x):
        pass


@own
class WithoutMethod:
    pass


@own
@dataclasses.dataclass
class Fields:
    a: int


@own
@dataclasses.dataclass
class OtherFields:
    b: int


Alias = typing.Union[int, str]
VARIANTS = {
    "missing function": ({}, {"f": func}),
    "differing default": ({"f": func_other_default}, {"f": func}),
    "differing parameter name": ({"f": func_other_name}, {"f": func}),
    "missing class method": ({"C": WithoutMethod}, {"C": WithMethod}),
    "differing dataclass field": ({"C": OtherFields}, {"C": Fields}),
    "missing type alias": ({}, {"Alias": Alias}),
    "missing constant": ({}, {"CONSTANT": 3}),
    "differing constant": ({"CONSTANT": 4}, {"CONSTANT": 3}),
}


class ParityCanFail(unittest.TestCase):
    def test_identical_modules_pass(self):
        api_parity.assert_same_api(self, fake_module(f=func, Alias=Alias, CONSTANT=3),
                                   fake_module(f=func, Alias=Alias, CONSTANT=3))

    def test_every_variant_is_detected(self):
        for label, (ours, theirs) in VARIANTS.items():
            with self.subTest(label):
                with self.assertRaises(AssertionError):
                    api_parity.assert_same_api(self, fake_module(**ours), fake_module(**theirs))

    def test_ignored_names_are_dropped(self):
        api_parity.assert_same_api(self, fake_module(C=WithoutMethod), fake_module(C=WithMethod),
                                   ignored=("C.method",))
        with self.assertRaises(AssertionError):
            api_parity.assert_same_api(self, fake_module(C=WithoutMethod), fake_module(C=WithMethod),
                                       ignored=("C.other",))

    def test_reimported_typing_names_are_ignored(self):
        api_parity.assert_same_api(self, fake_module(Union=typing.Union, List=typing.List), fake_module())


if __name__ == "__main__":
    unittest.main()
