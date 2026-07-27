#!/usr/bin/env python
# -*- coding: utf-8 -*-
import os
import re
import tempfile
import unittest

from cyclopts import App

from pycdfpp import __version__
from pycdfpp.cli_irsdump import app

os.environ['TZ'] = 'UTC'

_here = os.path.dirname(os.path.abspath(__file__))
_resources = os.path.join(_here, '..', 'resources')


def _run(*args):
    # cyclopts' App.__call__ defaults to sys.exit(0) on a *successful* run too (not just
    # on error) - result_action='return_value' returns main()'s return value instead,
    # verified against the installed cyclopts 4.22.1 this session (a bare app(tokens)
    # call raises SystemExit(0) here even when nothing went wrong).
    return app(list(args), result_action='return_value')


class CdfirsdumpCliTest(unittest.TestCase):
    def test_is_a_cyclopts_app_named_cdfirsdump(self):
        self.assertIsInstance(app, App)
        self.assertEqual(app.name[0], 'cdfirsdump')

    def test_default_level_is_brief_and_matches_a_real_capture(self):
        path = os.path.join(_resources, 'a_cdf.cdf')
        with open(os.path.join(_resources, 'a_cdf_cdfirsdump_brief_reference.txt')) as f:
            reference = f.read()
        # A bare path (no --output handle held open by the test itself) avoids
        # NamedTemporaryFile's Windows-only "second open() on an already-open handle"
        # lock, which this project's tests-windows.yml CI would otherwise hit.
        with tempfile.TemporaryDirectory() as tmpdir:
            out_path = os.path.join(tmpdir, 'out.txt')
            _run(path, '--output', out_path)
            with open(out_path) as f:
                self.assertEqual(f.read(), reference)

    def test_level_full_with_radix_16_matches_a_real_capture(self):
        path = os.path.join(_resources, 'rvariable.cdf')
        with open(os.path.join(_resources, 'rvariable_cdfirsdump_hex_reference.txt')) as f:
            reference = f.read()
        with tempfile.TemporaryDirectory() as tmpdir:
            out_path = os.path.join(tmpdir, 'out.txt')
            _run(path, '--level', 'full', '--no-summary', '--radix', '16', '--output', out_path)
            with open(out_path) as f:
                self.assertEqual(f.read(), reference)

    def test_offset_matches_a_real_capture(self):
        path = os.path.join(_resources, 'rvariable.cdf')
        with open(os.path.join(_resources, 'rvariable_cdfirsdump_offset_reference.txt')) as f:
            reference = f.read()
        with tempfile.TemporaryDirectory() as tmpdir:
            out_path = os.path.join(tmpdir, 'out.txt')
            _run(path, '--level', 'full', '--no-summary', '--offset', '404', '--output', out_path)
            with open(out_path) as f:
                self.assertEqual(f.read(), reference)

    def test_most_aliases_to_full(self):
        path = os.path.join(_resources, 'rvariable.cdf')
        with tempfile.TemporaryDirectory() as tmpdir:
            most_path = os.path.join(tmpdir, 'most.txt')
            full_path = os.path.join(tmpdir, 'full.txt')
            _run(path, '--level', 'most', '--no-summary', '--output', most_path)
            _run(path, '--level', 'full', '--no-summary', '--output', full_path)
            with open(most_path) as f_most, open(full_path) as f_full:
                self.assertEqual(f_most.read(), f_full.read())

    def test_about_prints_the_pycdfpp_version(self):
        import io
        import contextlib
        buf = io.StringIO()
        with contextlib.redirect_stdout(buf):
            _run(os.path.join(_resources, 'a_cdf.cdf'), '--about')
        self.assertIn(__version__, buf.getvalue())

    def test_about_with_no_path_prints_the_pycdfpp_version(self):
        # Regression test: `path` used to be a required positional argument, so
        # cyclopts rejected `cdfirsdump --about` (no path at all) before main()'s own
        # `if about: ...; return` branch ever ran - the real, most common way `--about`
        # is actually invoked (e.g. `cdfirsdump --about` with nothing else). `path`
        # must default to None and the `about` branch must still run first.
        import io
        import contextlib
        buf = io.StringIO()
        with contextlib.redirect_stdout(buf):
            _run('--about')
        self.assertIn(__version__, buf.getvalue())

    def test_app_object_is_the_correct_pyproject_toml_entry_point(self):
        # Regression test: pyproject.toml's [project.scripts] entries MUST reference
        # `app` (a cyclopts.App, callable with zero args -> parses sys.argv), not
        # `main` directly (a plain function requiring `path` positionally - a real
        # installed console script always calls its entry point with zero
        # arguments, so pointing pyproject.toml at `main` crashes immediately with
        # "missing 1 required positional argument: 'path'"). This was a real,
        # independently-reproduced bug found in review for cdfirsdump, and the
        # identical bug was separately found (same review, same root cause) in the
        # pre-existing cdfdump entry - both are checked here so the class of bug
        # can't silently reappear on either script.
        #
        # Parsed with a regex rather than tomllib/tomli: tomllib is stdlib only
        # from Python 3.11 (this project's requires-python is >=3.9), and adding
        # the tomli backport as a dependency just for this one line isn't worth it.
        pyproject_path = os.path.join(_here, '..', '..', 'pyproject.toml')
        with open(pyproject_path) as f:
            content = f.read()
        for script_name, expected_module in (
            ('cdfdump', 'pycdfpp.cli'),
            ('cdfirsdump', 'pycdfpp.cli_irsdump'),
        ):
            match = re.search(rf'^{script_name}\s*=\s*"([^"]+)"', content, re.MULTILINE)
            self.assertIsNotNone(match, f"{script_name} entry point not found in pyproject.toml")
            module_name, _, attr_name = match.group(1).partition(':')
            self.assertEqual(module_name, expected_module)
            self.assertEqual(attr_name, 'app',
                f"{script_name} entry point must reference 'app' (cyclopts.App, parses "
                f"sys.argv), not '{attr_name}' - a real console script always calls its "
                f"entry point with zero pre-supplied arguments, which crashes against "
                f"'main' directly")


if __name__ == '__main__':
    unittest.main()
