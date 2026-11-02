#!/usr/bin/env python
# -*- coding: utf-8 -*-
import os
import pathlib
import subprocess
import tempfile
import sys
import unittest
from glob import glob

import pycdfpp

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from file_reader import FileReader  # noqa: E402

RESOURCES = f'{HERE}/../resources'
FGM = f'{RESOURCES}/fgm_blocks.cdf'


def resource_files():
    return [p for p in sorted(glob(f'{RESOURCES}/*.cdf')) if not p.endswith('not_a_cdf.cdf')]


def run_python(code, **env):
    return subprocess.run([sys.executable, '-c', code], capture_output=True, text=True,
                          timeout=60, cwd=HERE,
                          env={**os.environ, 'PYTHONPATH': os.environ.get('PYTHONPATH', ''),
                               **env})


class RemoteLoadingTest(unittest.TestCase):
    def test_equals_path_load(self):
        for path in resource_files():
            local = pycdfpp.load(path)
            comparable = local == pycdfpp.load(path)  # False for files holding NaN values
            for lazy in (False, True):
                with self.subTest(path=path, lazy=lazy):
                    remote = pycdfpp.load(FileReader(path), lazy_load=lazy,
                                          read_ahead=4096, tail_prefetch=0)
                    self.assertEqual(list(remote), list(local))
                    self.assertEqual([v.shape for _, v in remote.items()],
                                     [v.shape for _, v in local.items()])
                    if comparable:
                        self.assertEqual(remote, local)

    def test_reader_raising_propagates(self):
        reader = FileReader(FGM, fail_on_call=2)
        cdf = pycdfpp.load(reader, read_ahead=4096, tail_prefetch=0)
        with self.assertRaises(OSError):
            cdf['b_gse'].values
        reader.healed = True
        self.assertEqual(cdf['b_gse'].values.shape, pycdfpp.load(FGM)['b_gse'].values.shape)

    def test_short_read_raises(self):
        with self.assertRaises(RuntimeError):
            pycdfpp.load(FileReader(FGM, short=True))

    def test_missing_chunk_raises(self):
        with self.assertRaises(RuntimeError):
            pycdfpp.load(FileReader(FGM, missing_chunk=True), tail_prefetch=0, read_ahead=4096)

    def test_strided_chunk_raises(self):
        with self.assertRaises((RuntimeError, BufferError)):
            pycdfpp.load(FileReader(FGM, strided=True))

    def test_every_knob_reaches_the_cache(self):
        size = os.path.getsize(FGM)
        reader = FileReader(FGM)
        pycdfpp.load(reader, download_whole_below=size + 1, tail_prefetch=0, read_ahead=4096)
        self.assertEqual(sum(s for _, s in reader.calls[0]), size)
        reader = FileReader(FGM)
        pycdfpp.load(reader, tail_prefetch=8192, read_ahead=4096)
        tail_start = (size - 8192) // 4096 * 4096  # the tail starts on a page boundary
        self.assertEqual(sum(s for _, s in reader.calls[0]), 4096 + size - tail_start)
        with tempfile.TemporaryDirectory(dir=os.path.dirname(os.environ.get(
                'PYTHONPATH', HERE).split(os.pathsep)[0]) or HERE) as directory:
            cdf = pycdfpp.load(FileReader(FGM), backing='file', temp_dir=pathlib.Path(directory),
                               lazy_load=False)
            self.assertEqual(cdf, pycdfpp.load(FGM))
        with self.assertRaises(ValueError):
            pycdfpp.load(FileReader(FGM), backing='lsm')

    def test_knobs_need_a_reader(self):
        with self.assertRaises(TypeError):
            pycdfpp.load(FGM, read_ahead=4096)

    def test_env_overrides(self):
        code = (
            'import pycdfpp\n'
            'from file_reader import FileReader\n'
            f'r = FileReader({FGM!r})\n'
            'pycdfpp.load(r)\n'
            'print(sum(size for _, size in r.calls[0]))\n'
            f'r = FileReader({FGM!r})\n'
            'pycdfpp.load(r, read_ahead=8192)\n'
            'print(sum(size for _, size in r.calls[0]))\n'
        )
        result = run_python(code, CDFPP_REMOTE_READ_AHEAD='4096', CDFPP_REMOTE_TAIL_PREFETCH='0')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout.split(), ['4096', '8192'])

    def test_no_deadlock_same_variable(self):
        # The reader sleeps so the loading thread waits in Python without the GIL, like a real
        # network read: that is when the polling thread can block on the variable lock.
        code = (
            'import threading, pycdfpp\n'
            'from file_reader import FileReader\n'
            f'cdf = pycdfpp.load(FileReader({FGM!r}, delay=0.05), read_ahead=4096, '
            'tail_prefetch=0)\n'
            'var = cdf["b_gse"]\n'
            'done = threading.Event()\n'
            'def load():\n'
            '    var.values\n'
            '    done.set()\n'
            'def poll():\n'
            '    while not done.is_set():\n'
            '        var.type\n'
            'threads = [threading.Thread(target=load), threading.Thread(target=poll)]\n'
            '[t.start() for t in threads]\n'
            '[t.join() for t in threads]\n'
            'print("ok")\n'
        )
        result = run_python(code)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout.strip(), 'ok')

    def test_exit_with_live_lazy_cdf(self):
        code = (
            'import pycdfpp\n'
            'from file_reader import FileReader\n'
            f'CDF = pycdfpp.load(FileReader({FGM!r}))\n'
            'VAR = CDF["b_gse"]\n'
        )
        result = run_python(code)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stderr, '')


class UrlLoadingTest(unittest.TestCase):
    """URLs go through fsspec; memory:// stands in for S3."""

    @classmethod
    def setUpClass(cls):
        cls.fs = __import__('fsspec').filesystem('memory')
        with open(FGM, 'rb') as f:
            cls.fs.pipe('/fgm_blocks.cdf', f.read())

    def test_loads_a_url_like_the_local_file(self):
        self.assertEqual(pycdfpp.load('memory://fgm_blocks.cdf', lazy_load=False),
                         pycdfpp.load(FGM))

    def test_lazy_url_load(self):
        cdf = pycdfpp.load('memory://fgm_blocks.cdf', read_ahead=4096)
        self.assertEqual(cdf, pycdfpp.load(FGM))

    def test_missing_url_raises_file_not_found(self):
        with self.assertRaises(FileNotFoundError):
            pycdfpp.load('memory://missing.cdf')

    def test_storage_options_reach_fsspec(self):
        from fsspec.implementations.memory import MemoryFileSystem

        class RecordingFileSystem(MemoryFileSystem):
            protocol = ('recording',)
            options = None

            def __init__(self, **options):
                RecordingFileSystem.options = options
                super().__init__(**options)

            @classmethod
            def _strip_protocol(cls, path):
                return MemoryFileSystem._strip_protocol(path.replace('recording://', 'memory://'))

        __import__('fsspec').register_implementation('recording', RecordingFileSystem,
                                                     clobber=True)
        pycdfpp.load('recording://fgm_blocks.cdf', storage_options={'anon': True})
        self.assertEqual(RecordingFileSystem.options, {'anon': True})

    def test_storage_options_need_a_url(self):
        with self.assertRaises(TypeError):
            pycdfpp.load(FGM, storage_options={'anon': True})

    def s3_options_seen(self, storage_options='None', **aws_env):
        no_aws = {k: v for k, v in os.environ.items() if not k.startswith('AWS_')}
        code = ("import fsspec, pycdfpp\n"
                "from fsspec.implementations.memory import MemoryFileSystem\n"
                "class Recording(MemoryFileSystem):\n"
                "    protocol = ('s3',)\n"
                "    def __init__(self, **options):\n"
                "        print('options', options)\n"
                "        super().__init__()\n"
                "    @classmethod\n"
                "    def _strip_protocol(cls, path):\n"
                "        return MemoryFileSystem._strip_protocol(path.replace('s3://', 'memory://'))\n"
                "fsspec.register_implementation('s3', Recording, clobber=True)\n"
                f"fsspec.filesystem('memory').pipe('/f.cdf', open({FGM!r}, 'rb').read())\n"
                f"pycdfpp.load('s3://f.cdf', storage_options={storage_options})\n")
        result = subprocess.run([sys.executable, '-c', code], capture_output=True, text=True,
                                timeout=60, cwd=HERE,
                                env={**no_aws, 'AWS_SHARED_CREDENTIALS_FILE': os.devnull,
                                     'AWS_CONFIG_FILE': os.devnull,
                                     'AWS_EC2_METADATA_DISABLED': 'true', **aws_env})
        self.assertEqual(result.returncode, 0, result.stderr)
        return result.stdout.strip()

    def test_s3_without_credentials_reads_anonymously(self):
        self.assertEqual(self.s3_options_seen(), "options {'anon': True}")

    def test_s3_with_credentials_uses_them(self):
        self.assertEqual(self.s3_options_seen(AWS_ACCESS_KEY_ID='key',
                                              AWS_SECRET_ACCESS_KEY='secret'), "options {}")

    def test_s3_storage_options_win_over_the_default(self):
        self.assertEqual(self.s3_options_seen("{'profile': 'x'}"), "options {'profile': 'x'}")

    def test_builds_without_remote_reads_say_so(self):
        result = run_python("import pycdfpp\n"
                            "del pycdfpp._pycdfpp.load_reader  # as in WASM builds\n"
                            "from file_reader import FileReader\n"
                            "try:\n"
                            f"    pycdfpp.load(FileReader({FGM!r}))\n"
                            "except NotImplementedError as e:\n"
                            "    print(e)\n")
        self.assertIn('not available in this build', result.stdout, result.stderr)

    def test_without_fsspec_the_error_says_what_to_install(self):
        result = run_python("import sys; sys.modules['fsspec'] = None\n"
                            "import pycdfpp\n"
                            "try:\n"
                            "    pycdfpp.load('s3://bucket/file.cdf')\n"
                            "except ImportError as e:\n"
                            "    print(e)\n")
        self.assertIn('pip install fsspec', result.stdout, result.stderr)


if __name__ == '__main__':
    unittest.main()
