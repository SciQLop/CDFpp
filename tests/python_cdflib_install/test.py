import os
import subprocess
import sys
import textwrap
import unittest
from pathlib import Path

RESOURCES = Path(__file__).resolve().parent.parent / "resources"
CDF_FILE = RESOURCES / "a_cdf.cdf"


def run_fresh(code):
    """Every install test runs in a new interpreter: install() swaps sys.modules for good."""
    result = subprocess.run([sys.executable, "-c", textwrap.dedent(code)], capture_output=True,
                            text=True, env=dict(os.environ))
    if result.returncode != 0:
        raise AssertionError(result.stderr)
    return result.stdout.strip()


class Install(unittest.TestCase):
    def test_cdflib_modules_are_ours(self):
        run_fresh("""
            import pycdfpp.cdflib as ours
            ours.install()
            import cdflib
            from cdflib.epochs import CDFepoch
            from cdflib.xarray import cdf_to_xarray, xarray_to_cdf
            import cdflib.cdfread, cdflib.cdfwrite, cdflib.s3, cdflib.utils, cdflib.logging
            import sys
            cdf_to_xarray_module = sys.modules["cdflib.xarray.cdf_to_xarray"]
            xarray_to_cdf_module = sys.modules["cdflib.xarray.xarray_to_cdf"]
            assert cdflib is ours
            assert CDFepoch is ours.epochs.CDFepoch
            assert cdflib.cdfepoch is ours.cdfepoch
            assert cdflib.CDF is ours.CDF
            assert cdflib.cdfread is ours.cdfread and cdflib.cdfwrite is ours.cdfwrite
            assert cdf_to_xarray is ours.xarray.cdf_to_xarray
            assert xarray_to_cdf is ours.xarray.xarray_to_cdf
            assert cdf_to_xarray_module.__name__ == "pycdfpp.cdflib.xarray.cdf_to_xarray"
            assert xarray_to_cdf_module.__name__ == "pycdfpp.cdflib.xarray.xarray_to_cdf"
            for name in ("s3", "utils", "logging"):
                assert getattr(cdflib, name).__name__ == "pycdfpp.cdflib." + name, name
        """)

    def test_dataclasses_and_epochs_astropy_stay_real(self):
        run_fresh("""
            import cdflib.dataclasses as real_dataclasses
            import cdflib.epochs_astropy as real_epochs_astropy
            import pycdfpp.cdflib as ours
            ours.install()
            import cdflib
            import cdflib.dataclasses, cdflib.epochs_astropy
            from cdflib.dataclasses import CDFInfo
            assert cdflib.dataclasses is real_dataclasses
            assert cdflib.epochs_astropy is real_epochs_astropy
            assert CDFInfo is real_dataclasses.CDFInfo
            assert "pycdfpp" not in real_dataclasses.__file__
        """)

    def test_read_gives_the_same_values(self):
        out = run_fresh(f"""
            import cdflib
            import numpy as np
            def read():
                cdf = cdflib.CDF({str(CDF_FILE)!r})
                names = cdf.cdf_info().zVariables
                return {{name: np.asarray(cdf.varget(name)) for name in names}}
            before = read()
            import pycdfpp.cdflib as ours
            ours.install()
            import cdflib as swapped
            assert swapped.CDF is ours.CDF and swapped.CDF is not type(None)
            after = read()
            assert before.keys() == after.keys() and before
            for name in before:
                np.testing.assert_array_equal(before[name], after[name], err_msg=name)
            print(len(after))
        """)
        self.assertGreater(int(out), 0)

    def test_install_twice_is_harmless(self):
        run_fresh("""
            import sys
            import pycdfpp.cdflib as ours
            ours.install()
            first = dict(sys.modules)
            ours.install()
            import cdflib
            assert cdflib is ours
            for name in ("cdflib", "cdflib.cdfread", "cdflib.epochs", "cdflib.xarray",
                         "cdflib.dataclasses"):
                assert sys.modules[name] is first[name], name
        """)


if __name__ == "__main__":
    unittest.main()
