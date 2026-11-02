#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""Runs the python_loading tests again, with every existing file path read through a byte-range
reader, page by page, so each parser path goes through the remote cache."""
import importlib.util
import os
import sys
import unittest

import pycdfpp

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from file_reader import FileReader  # noqa: E402

_load_from_path = pycdfpp.load
_loads_through_reader = []


def _load_through_reader(file_or_buffer, *args, **kwargs):
    if isinstance(file_or_buffer, (str, os.PathLike)) and os.path.isfile(file_or_buffer):
        _loads_through_reader.append(file_or_buffer)
        return _load_from_path(FileReader(os.fspath(file_or_buffer)), *args, read_ahead=4096,
                               tail_prefetch=0, **kwargs)
    return _load_from_path(file_or_buffer, *args, **kwargs)


pycdfpp.load = _load_through_reader

spec = importlib.util.spec_from_file_location(
    'python_loading', os.path.join(HERE, '..', 'python_loading', 'test.py'))
python_loading = importlib.util.module_from_spec(spec)
spec.loader.exec_module(python_loading)


def tearDownModule():
    assert len(_loads_through_reader) > 10, \
        f"only {len(_loads_through_reader)} loads went through the reader"


python_loading.tearDownModule = tearDownModule

if __name__ == '__main__':
    unittest.main(module=python_loading)
