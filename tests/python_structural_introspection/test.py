#!/usr/bin/env python
# -*- coding: utf-8 -*-
import os
import unittest
import pycdfpp

RESOURCES = f'{os.path.dirname(os.path.abspath(__file__))}/../resources'


def load(name, lazy=True):
    return pycdfpp.load(f'{RESOURCES}/{name}', lazy)


class StructuralIntrospection(unittest.TestCase):
    def test_zvariable_is_reported_for_modern_variables(self):
        cdf = load('contiguous.cdf')
        self.assertTrue(cdf['whole_zvar'].is_zvariable)

    def test_rvariable_is_reported_as_not_zvariable(self):
        cdf = load('rvariable.cdf')
        self.assertFalse(cdf['legacy_rvar'].is_zvariable)

    def test_single_block_variable_is_contiguous(self):
        cdf = load('contiguous.cdf')
        self.assertTrue(cdf['whole_zvar'].is_contiguous())

    def test_variable_split_across_two_vvr_blocks_is_not_contiguous(self):
        cdf = load('fragmented.cdf')
        self.assertFalse(cdf['split_zvar'].is_contiguous())
        self.assertTrue(cdf['filler'].is_contiguous())

    def test_contiguity_is_consistent_between_lazy_and_eager_loads(self):
        for lazy in (True, False):
            cdf = load('fragmented.cdf', lazy)
            self.assertFalse(cdf['split_zvar'].is_contiguous())

    def test_rvariable_and_zvariable_of_same_number_keep_their_own_attributes(self):
        for lazy in (True, False):
            cdf = load('ac_k0_mfi_00000000_v01.cdf', lazy)
            self.assertFalse(cdf['Epoch'].is_zvariable)
            self.assertTrue(cdf['label_BGSE'].is_zvariable)
            self.assertEqual(cdf['Epoch'].attributes['FIELDNAM'].value, 'Time')
            self.assertEqual(cdf['label_BGSE'].attributes['FIELDNAM'].value,
                             'Label for B in Cartesian GSE')
            # No zVariable has this number, yet the attribute also has a zEntry.
            self.assertEqual(cdf['Time_PB5'].attributes['FIELDNAM'].value, 'Time PB5')

    def test_variable_numbered_past_the_variable_count_loads_without_attributes(self):
        with open(f'{RESOURCES}/ac_k0_mfi_00000000_v01.cdf', 'rb') as f:
            data = bytearray(f.read())
        set_vdr_number(data, b'label_BGSE', 1_000_000)
        for lazy in (True, False):
            cdf = pycdfpp.load(bytes(data), lazy)
            self.assertEqual(len(cdf['label_BGSE'].attributes), 0)
            self.assertEqual(cdf['Epoch'].attributes['FIELDNAM'].value, 'Time')


def set_vdr_number(data, name, number):
    """Overwrite the Num field of the v3 VDR named `name` (spec: VDR layout, Num at 68, Name at 84)."""
    name_field = name + b'\0'
    position = data.find(name_field)
    while position != -1:
        record = position - 84
        if int.from_bytes(data[record + 8:record + 12], 'big') in (3, 8):
            data[record + 68:record + 72] = number.to_bytes(4, 'big')
            return
        position = data.find(name_field, position + 1)
    raise ValueError(f'no VDR named {name!r}')


if __name__ == '__main__':
    unittest.main()
