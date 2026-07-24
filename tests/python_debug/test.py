#!/usr/bin/env python
# -*- coding: utf-8 -*-
import os
import unittest

import pycdfpp
import pycdfpp.debug as debug
from pycdfpp.cli import dump

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


class CliDumpTest(unittest.TestCase):
    def test_dump_formats_every_record_and_field(self):
        path = os.path.join(_resources, 'a_cdf.cdf')
        text = dump(path)

        self.assertIn('@8 CDR', text)
        self.assertIn('Version: 3', text)
        self.assertIn('Encoding: "IBMPC"', text)
        self.assertIn('copyright: "', text)


if __name__ == '__main__':
    unittest.main()
