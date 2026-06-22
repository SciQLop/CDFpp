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


if __name__ == '__main__':
    unittest.main()
