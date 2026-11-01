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
from pycdfpp.cdflib.s3 import S3object

RESOURCES = Path(__file__).resolve().parent.parent / "resources"
NAME = "a_cdf.cdf"
COMPRESSED = "a_compressed_cdf.cdf"
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


class FakeS3Handle:
    def __init__(self, content: bytes):
        self.content, self.content_length = content, len(content)

    def get(self, Range=None):
        first, last = (int(n) for n in Range[len("bytes="):].split("-")) if Range else (0, self.content_length - 1)
        return {"Body": io.BytesIO(self.content[first:last + 1])}


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

    def test_s3_compressed_file(self):
        content = (RESOURCES / COMPRESSED).read_bytes()
        sys.modules["boto3"] = fake_boto3(content, [])
        self.addCleanup(sys.modules.pop, "boto3", None)
        remote = cdfread.CDF("s3://a-bucket/compressed.cdf")
        local = cdfread.CDF(RESOURCES / COMPRESSED)
        variable = first_variable(local)
        np.testing.assert_array_equal(remote.varget(variable), local.varget(variable))

    def test_s3_fetchall(self):
        self.assertEqual(cdfread.s3_fetchall({"Body": io.BytesIO(b"abc")}).read(), b"abc")

    def test_s3_object_reads_ranges(self):
        obj = S3object(FakeS3Handle(b"0123456789"))
        self.assertEqual(obj.read(4), b"0123")
        self.assertEqual(obj.tell(), 4)
        obj.seek(-2, 2)
        self.assertEqual(obj.read(2), b"89")
        obj.seek(1)
        obj.seek(2, 1)
        self.assertEqual(obj.tell(), 3)
        obj.seek(0)
        self.assertEqual(obj.read(-1), b"0123456789")
        self.assertEqual(obj.fetchS3entire().read(), b"0123456789")


if __name__ == "__main__":
    unittest.main()
