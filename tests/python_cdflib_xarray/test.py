"""pycdfpp.cdflib.xarray.cdf_to_xarray against the real cdflib.xarray.cdf_to_xarray on the test corpus."""
import itertools
import logging
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
CORPUS = sorted(p.name for p in RESOURCES.glob("*.cdf"))
OPTIONS = [dict(zip(("to_datetime", "to_unixtime", "fillval_to_nan"), flags))
           for flags in itertools.product((True, False), repeat=3)]
# Variables our reader reads as NASA's library does, unlike cdflib (see docs/cdflib.rst): their values differ,
# everything else (dims, coords, attributes) must be identical.
READ_DIFFERENTLY_BY_CDFLIB = {("sparse_records.cdf", "pad_with_fillval"), ("sparse_records.cdf", "prev_with_fillval"),
                              ("testutf8.cdf", "Temp")}


def convert(module, name, options):
    try:
        return module.cdf_to_xarray(str(RESOURCES / name), **options)
    except Exception as error:  # the type is what both libraries must agree on
        return error


def ours_module():
    from pycdfpp.cdflib import xarray as ours
    return ours


def without_values(dataset, names):
    return dataset.assign({name: dataset[name].copy(data=np.zeros(dataset[name].shape))
                           for name in names if name in dataset.variables})


def epoch_milliseconds_cut(dataset):
    """cdflib cuts CDF_EPOCH values to whole milliseconds when it converts them to datetime64."""
    return dataset.assign({name: variable.copy(data=variable.values.astype("datetime64[ms]").astype(variable.dtype))
                           for name, variable in dataset.variables.items()
                           if variable.attrs.get("CDF_DATA_TYPE") == "CDF_EPOCH" and variable.dtype.kind == "M"})


def epoch16_variables(name):
    return [variable for variable, values in pycdfpp.load(str(RESOURCES / name)).items()
            if values.type == pycdfpp.DataType.CDF_EPOCH16]


def converts_times(options):
    return options["to_datetime"] or options["to_unixtime"]


def comparable(dataset, name, options):
    """cdflib keeps the first 7 records of CDF_EPOCH16 variables it doesn't convert to times: left out here."""
    differing = [variable for file, variable in READ_DIFFERENTLY_BY_CDFLIB if file == name]
    truncated = [] if converts_times(options) else epoch16_variables(name)
    return epoch_milliseconds_cut(without_values(dataset, differing)).drop_vars(truncated, errors="ignore")


@ddt
class CdfToXarray(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        logging.getLogger("cdflib.logging").setLevel(logging.ERROR)
        warnings.simplefilter("ignore", RuntimeWarning)

    @data(*CORPUS)
    def test_same_dataset_as_cdflib(self, name):
        for options in OPTIONS:
            with self.subTest(**options):
                ours, theirs = convert(ours_module(), name, options), convert(cdflib.xarray, name, options)
                if isinstance(theirs, Exception):
                    self.assertTrue(isinstance(ours, xr.Dataset) or type(ours) is type(theirs), f"{ours!r} {theirs!r}")
                else:
                    self.assertIsInstance(ours, xr.Dataset, repr(ours))
                    xr.testing.assert_identical(comparable(ours, name, options), comparable(theirs, name, options))

    def test_epoch_keeps_fractional_milliseconds(self):
        from pycdfpp.cdflib import cdfwrite
        with tempfile.TemporaryDirectory(dir=".") as folder:
            path = str(Path(folder) / "epoch.cdf")
            writer = cdfwrite.CDF(path)
            writer.write_var({"Variable": "epoch", "Data_Type": 31, "Num_Elements": 1, "Rec_Vary": True,
                              "Dim_Sizes": []}, var_data=np.array([63745056000000.5, 63745056000001.25]))
            writer.close()
            dataset = ours_module().cdf_to_xarray(path)
        expected = np.array(["2020-01-01T00:00:00.000500", "2020-01-01T00:00:00.001250"], dtype="datetime64[ns]")
        np.testing.assert_array_equal(dataset["epoch"].values, expected)

    def test_epoch16_without_time_conversion_is_epoch_of_every_record(self):
        dataset = ours_module().cdf_to_xarray(str(RESOURCES / "a_cdf.cdf"), to_datetime=False)
        from pycdfpp.cdflib.epochs import CDFepoch
        expected = pycdfpp.to_datetime64(pycdfpp.load(str(RESOURCES / "a_cdf.cdf"))["epoch16"]).astype("datetime64[ms]")
        self.assertEqual(dataset["epoch16"].dtype, np.float64)
        np.testing.assert_array_equal(CDFepoch.to_datetime(dataset["epoch16"].values), expected)

    def test_epoch16_fields_cdflib_rejects(self):
        dataset = ours_module().cdf_to_xarray(str(RESOURCES / "time_special_values.cdf"), to_datetime=False)
        self.assertEqual(dataset["epoch16"].shape, (16,))
        self.assertEqual(dataset["epoch16"].values[0], -1e31)

    def test_label_of_a_missing_depend_is_ignored(self):
        from pycdfpp.cdflib import cdfwrite
        with tempfile.TemporaryDirectory(dir=".") as folder:
            path = str(Path(folder) / "label.cdf")
            writer = cdfwrite.CDF(path)
            writer.write_var({"Variable": "b", "Data_Type": 45, "Num_Elements": 1, "Rec_Vary": True, "Dim_Sizes": [3]},
                             var_attrs={"DEPEND_1": "missing", "LABL_PTR_1": "b_labels"}, var_data=np.zeros((2, 3)))
            writer.close()
            with self.assertRaises(KeyError):
                cdflib.xarray.cdf_to_xarray(path)
            dataset = ours_module().cdf_to_xarray(path)
        self.assertEqual(dataset["b"].shape, (2, 3))

    def test_unixtime_wins_over_datetime(self):
        dataset = ours_module().cdf_to_xarray(str(RESOURCES / "a_cdf.cdf"), to_datetime=True, to_unixtime=True)
        self.assertEqual(dataset["epoch"].dtype, np.float64)


if __name__ == "__main__":
    unittest.main()
