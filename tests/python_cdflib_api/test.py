import os
import subprocess
import sys
import unittest

import api_parity


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

    def test_missing_cdflib_message(self):
        code = "import sys; sys.modules['cdflib'] = None; import pycdfpp.cdflib"
        result = subprocess.run([sys.executable, "-c", code], capture_output=True, text=True,
                                env=dict(os.environ))
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("ImportError", result.stderr)
        self.assertIn("pip install pycdfpp[cdflib]", result.stderr)


if __name__ == "__main__":
    unittest.main()
