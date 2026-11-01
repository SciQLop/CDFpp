"""pycdfpp.cdflib.cdfread.CDF opening http(s):// and s3:// paths, against local reads."""
import functools
import http.server
import io
import sys
import threading
import types
import unittest
from pathlib import Path

import numpy as np

from pycdfpp.cdflib import cdfread

RESOURCES = Path(__file__).resolve().parent.parent / "resources"
NAME = "a_cdf.cdf"
VARIABLE = "var"


def served(directory: Path) -> http.server.ThreadingHTTPServer:
    handler = functools.partial(http.server.SimpleHTTPRequestHandler, directory=str(directory))
    handler.log_message = lambda *args: None
    return http.server.ThreadingHTTPServer(("127.0.0.1", 0), handler)


def fake_boto3(content: bytes, calls: list) -> types.ModuleType:
    class Client:
        def get_object(self, Bucket, Key):
            calls.append((Bucket, Key))
            return {"Body": io.BytesIO(content)}

    module = types.ModuleType("boto3")
    module.client = lambda service, **kwargs: Client()
    return module


def first_variable(cdf):
    return cdf.cdf_info().zVariables[0]


class RemoteReads(unittest.TestCase):
    def setUp(self):
        self.local = cdfread.CDF(RESOURCES / NAME)
        self.variable = first_variable(self.local)
        self.content = (RESOURCES / NAME).read_bytes()

    def assert_reads_like_local(self, remote):
        np.testing.assert_array_equal(remote.varget(self.variable), self.local.varget(self.variable))
        self.assertEqual(remote.cdf_info().zVariables, self.local.cdf_info().zVariables)
        self.assertEqual(repr(remote.globalattsget()), repr(self.local.globalattsget()))

    def test_http(self):
        server = served(RESOURCES)
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        self.addCleanup(thread.join)
        self.addCleanup(server.server_close)
        self.addCleanup(server.shutdown)
        url = f"http://127.0.0.1:{server.server_address[1]}/{NAME}"
        remote = cdfread.CDF(url)
        self.assertEqual((remote.file, remote.ftype), (url, "url"))
        self.assert_reads_like_local(remote)

    def test_s3_read_methods(self):
        for method in (1, 2, 3):
            with self.subTest(s3_read_method=method):
                calls = []
                sys.modules["boto3"] = fake_boto3(self.content, calls)
                self.addCleanup(sys.modules.pop, "boto3", None)
                remote = cdfread.CDF("s3://a-bucket/some/key.cdf", s3_read_method=method)
                self.assertEqual(calls, [("a-bucket", "some/key.cdf")])
                self.assertEqual((remote.file, remote.ftype), ("s3://a-bucket/some/key.cdf", "s3"))
                self.assert_reads_like_local(remote)

    def test_s3_without_boto3(self):
        sys.modules["boto3"] = None
        self.addCleanup(sys.modules.pop, "boto3", None)
        with self.assertRaises(ImportError):
            cdfread.CDF("s3://a-bucket/key.cdf")

    def test_s3_object_and_fetchall(self):
        body = io.BytesIO(b"abc")
        self.assertEqual(cdfread.s3_fetchall({"Body": body}).read(), b"abc")
        self.assertTrue(hasattr(cdfread, "S3object"))


if __name__ == "__main__":
    unittest.main()
