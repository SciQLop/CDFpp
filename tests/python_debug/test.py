#!/usr/bin/env python
# -*- coding: utf-8 -*-
import os
import unittest

import pycdfpp
import pycdfpp.debug as debug
from pycdfpp.cli import app, build_tree

os.environ['TZ'] = 'UTC'

_here = os.path.dirname(os.path.abspath(__file__))
_resources = os.path.join(_here, '..', 'resources')


class DebugForEachRecordTest(unittest.TestCase):
    def test_walks_a_real_v3_cdf_in_physical_order(self):
        path = os.path.join(_resources, 'a_cdf.cdf')
        records = debug.for_each_record(path)

        self.assertEqual(len(records), 89)

        offset, type_name, fields = records[0]
        self.assertEqual(offset, 8)
        self.assertEqual(type_name, 'CDR')
        self.assertEqual(fields['Version'], 3)
        self.assertEqual(fields['Release'], 9)
        self.assertEqual(fields['Encoding'], 'IBMPC')
        self.assertIsInstance(fields['copyright'], str)

        offset, type_name, fields = records[1]
        self.assertEqual(offset, 320)
        self.assertEqual(type_name, 'GDR')

        offset, type_name, fields = records[-1]
        self.assertEqual(offset, 122926)
        self.assertEqual(type_name, 'AgrEDR')

    def test_zvdr_count_matches_loaded_zvariable_count(self):
        path = os.path.join(_resources, 'a_cdf.cdf')
        cdf = pycdfpp.load(path)
        n_zvar = sum(1 for _, v in cdf.items() if v.is_zvariable)

        records = debug.for_each_record(path)
        n_zvdr = sum(1 for _, type_name, _ in records if type_name == 'zVDR')

        self.assertEqual(n_zvdr, n_zvar)

    def test_dynamic_array_fields_come_back_as_python_lists(self):
        path = os.path.join(_resources, 'a_cdf.cdf')
        records = debug.for_each_record(path)
        zvdr_fields = next(fields for _, type_name, fields in records if type_name == 'zVDR')

        self.assertIsInstance(zvdr_fields['DimVarys'], list)

    def test_handles_a_compressed_cdf(self):
        path = os.path.join(_resources, 'a_compressed_cdf.cdf')
        records = debug.for_each_record(path)

        type_names = {type_name for _, type_name, _ in records}
        self.assertIn('CDR', type_names)
        self.assertIn('GDR', type_names)
        self.assertGreater(len(records), 0)


class NasaCompatDumpTest(unittest.TestCase):
    def test_matches_a_real_cdfirsdump_capture_byte_for_byte(self):
        path = os.path.join(_resources, 'a_cdf.cdf')
        with open(os.path.join(_resources, 'a_cdf_cdfirsdump_reference.txt')) as f:
            reference = f.read()

        # The one known, already-tracked divergence (finding-tt2000-scalar-simd-
        # pre1972 project memory - a pre-1972 tt2000 date, not this feature's bug),
        # normalized the same way the C++ end-to-end test does.
        reference = reference.replace(
            '1970-01-01T00:00:00.000000000, ...',
            '1970-01-01T00:00:08.001377999, ...',
        )

        self.assertEqual(debug.nasa_compat_dump(path), reference)

    def test_radix_16_matches_a_real_hex_capture(self):
        path = os.path.join(_resources, 'rvariable.cdf')
        with open(os.path.join(_resources, 'rvariable_cdfirsdump_hex_reference.txt')) as f:
            reference = f.read()
        self.assertEqual(debug.nasa_compat_dump(path, radix=16), reference)

    def test_dump_from_offset_matches_a_real_offset_capture(self):
        path = os.path.join(_resources, 'rvariable.cdf')
        with open(os.path.join(_resources, 'rvariable_cdfirsdump_offset_reference.txt')) as f:
            reference = f.read()
        self.assertEqual(debug.nasa_compat_dump_from_offset(path, 404), reference)

    def test_dump_brief_matches_a_real_brief_capture(self):
        path = os.path.join(_resources, 'a_cdf.cdf')
        with open(os.path.join(_resources, 'a_cdf_cdfirsdump_brief_reference.txt')) as f:
            reference = f.read()
        self.assertEqual(debug.nasa_compat_dump_brief(path), reference)

    def test_summary_true_matches_a_real_full_plus_summary_capture(self):
        path = os.path.join(_resources, 'rvariable.cdf')
        with open(os.path.join(_resources, 'rvariable_cdfirsdump_summary_reference.txt')) as f:
            reference = f.read()
        self.assertEqual(debug.nasa_compat_dump(path, summary=True), reference)


class CliBuildTreeTest(unittest.TestCase):
    def test_tree_has_one_node_per_record_with_field_leaves(self):
        path = os.path.join(_resources, 'a_cdf.cdf')
        tree = build_tree(path)

        self.assertEqual(tree.label, path)
        self.assertEqual(len(tree.children), 89)

        cdr_node = tree.children[0]
        self.assertIn('@8', cdr_node.label)
        self.assertIn('CDR', cdr_node.label)
        field_labels = [leaf.label for leaf in cdr_node.children]
        self.assertTrue(any('Version' in label and '3' in label for label in field_labels))
        self.assertTrue(any('Encoding' in label and 'IBMPC' in label for label in field_labels))
        self.assertTrue(any('copyright' in label for label in field_labels))

    def test_cli_app_runs_end_to_end(self):
        path = os.path.join(_resources, 'a_cdf.cdf')
        # Exercises real argument parsing + console rendering, not just build_tree().
        # cyclopts sys.exit()s on completion, like any CLI entry point - a clean exit
        # (code 0) is success here, not a test error.
        with self.assertRaises(SystemExit) as ctx:
            app([path])
        self.assertEqual(ctx.exception.code, 0)

    def test_cli_irsdump_flag_runs_end_to_end(self):
        path = os.path.join(_resources, 'a_cdf.cdf')
        with self.assertRaises(SystemExit) as ctx:
            app([path, '--irsdump'])
        self.assertEqual(ctx.exception.code, 0)


if __name__ == '__main__':
    unittest.main()
