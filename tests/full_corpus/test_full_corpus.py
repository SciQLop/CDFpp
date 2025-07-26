#!/usr/bin/env python
# -*- coding: utf-8 -*-
import os
from datetime import datetime, timedelta
import numpy as np
import math
import unittest
import ddt
from glob import glob
import pycdfpp
import requests
import threading

os.environ['TZ'] = 'UTC'

files = (
    ( "a1_k0_mpa_20050804_v02.cdf", 39, 18 ),
    ( "ac_h2_sis_20101105_v06.cdf", 61, 26 ),
    ( "ac_or_ssc_20031101_v01.cdf", 7, 18 ),
    ( "ac_or_ssc_20040809_v01.cdf", 61, 26 ),
    ( "bigcdf_compressed.cdf", 1, 0 ),
    ( "c1_cp_fgm_spin_20080101_v01.cdf", 11, 45 ),
    ( "c1_jp_pmp_20081001_v32.cdf", 7, 30 ),
    ( "c1_jp_pse_20090101_v28.cdf", 11, 30 ),
    ( "c1_pp_cis_20080101_v01.cdf", 18, 31 ),
    ( "c1_pp_dwp_20100111_v01.cdf", 20, 31 ),
    ( "c1_waveform_wbd_200202080940_v01.cdf", 13, 27 ),
    ( "c1_waveform_wbd_200202080940_v01_subset.cdf", 13, 27 ),
    ( "cl_jp_pgp_20031001_v52.cdf", 37, 32 ),
    ( "cl_sp_edi_00000000_v01.cdf", 7, 39 ),
    ( "cluster-2_cp3drl_2002052000000_v1.cdf", 40, 32 ),
    ( "de_uv_sai_19910218_v01.cdf", 39, 12 ),
    ( "ge_k0_cpi_19921231_v02.cdf", 25, 18 ),
    ( "i1_av_ott_1983351130734_v01.cdf", 47, 18 ),
    ( "im_k0_euv_20011231_v01.cdf", 12, 13 ),
    ( "im_k0_rpi_20051218_v01.cdf", 25, 14 ),
    ( "im_k1_rpi_20051217_v01.cdf", 14, 14 ),
    ( "mms1_fpi_brst_l2_des-moms_20180101005543_v3.3.0.cdf", 55, 46 ),
    ( "mms1_fpi_brst_l2_dis-qmoms_20170703052703_v3.3.0.cdf", 50, 43 ),
    ( "mms1_fpi_fast_sitl_20150801132440_v0.0.0.cdf", 100, 25 ),
    ( "mms1_scm_srvy_l2_scsrvy_20190301_v2.2.0.cdf", 6, 28 ),
    ( "po_h4_pwi_19970901_v01.cdf", 17, 26 ),
    ( "po_h9_pwi_1997010103_v01.cdf", 16, 26 ),
    ( "po_k0_uvi_20051230_v02.cdf", 31, 20 ),
    ( "tha_l2_fgm_20070729_v01.cdf", 37, 34 ),
    ( "tha_l2_fgm_20101202_v01.cdf", 41, 34 ),
    ( "tha_l2_scm_20160831_v01.cdf", 29, 34 ),
    ( "timed_L1Cdisk_guvi_20060601005849_v01.cdf", 67, 53 ),
    ( "twins1_l1_imager_2010102701_v01.cdf", 79, 18 ),
    ( "ulysses.cdf", 15, 10 )
)

@ddt.ddt
class PycdfCorpus(unittest.TestCase):
    def setUp(self):
        pass

    def tearDown(self):
        pass

    @ddt.data(
        *files
    )
    @ddt.unpack
    def test_opens_in_memory_remote_files(self, fname, variables, attrs):
        print(fname)
        cdf = pycdfpp.load(requests.get(f"https://129.104.27.7/data/mirrors/CDF/test_files/{fname}", verify=False).content)
        self.assertIsNotNone(cdf)
        self.assertEqual(len(cdf), variables)
        self.assertEqual(len(cdf.attributes), attrs)
        
    def test_multithreaded_load(self):
        def load_file(fname, variables, attrs):
            cdf = pycdfpp.load(requests.get(f"https://129.104.27.7/data/mirrors/CDF/test_files/{fname}", verify=False).content)
            self.assertIsNotNone(cdf)
            self.assertEqual(len(cdf), variables)
            self.assertEqual(len(cdf.attributes), attrs)

        threads = []
        for fname, variables, attrs in files:
            thread = threading.Thread(target=load_file, args=(fname, variables, attrs))
            thread.start()
            threads.append(thread)

        for thread in threads:
            thread.join()

if __name__ == '__main__':
    unittest.main()
