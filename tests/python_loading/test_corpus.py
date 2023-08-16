#!/usr/bin/env python3
# -*- coding: utf-8 -*-
import os
import sys
import unittest
from ddt import ddt, data
import pycdfpp
import tempfile
from urllib.request import urlopen

has_pycdf = True
try:
    from spacepy import pycdf
except ImportError:
    has_pycdf = False


def dl_data(url:str):
    with tempfile.NamedTemporaryFile(delete=False) as f:
        with urlopen(url) as remote_file:
            f.write(remote_file.read())
        f.close()
    return f


@ddt
class SimpleRequest(unittest.TestCase):
    def setUp(self):
        pass

    def tearDown(self):
        pass
#lynx -dump -listonly https://hephaistos.lpp.polytechnique.fr/data/mirrors/CDF/test_files/ | grep http | grep '\.cdf' | awk '{print $2}'
    @data(
            'https://hephaistos.lpp.polytechnique.fr/data/mirrors/CDF/test_files/a1_k0_mpa_20050804_v02.cdf',
            'https://hephaistos.lpp.polytechnique.fr/data/mirrors/CDF/test_files/ac_h2_sis_20101105_v06.cdf',
            'https://hephaistos.lpp.polytechnique.fr/data/mirrors/CDF/test_files/ac_or_ssc_20031101_v01.cdf',
            'https://hephaistos.lpp.polytechnique.fr/data/mirrors/CDF/test_files/ac_or_ssc_20040809_v01.cdf',
            'https://hephaistos.lpp.polytechnique.fr/data/mirrors/CDF/test_files/bigcdf_compressed.cdf',
            'https://hephaistos.lpp.polytechnique.fr/data/mirrors/CDF/test_files/c1_waveform_wbd_200202080940_v01.cdf',
            'https://hephaistos.lpp.polytechnique.fr/data/mirrors/CDF/test_files/c1_waveform_wbd_200202080940_v01_subset.cdf',
            'https://hephaistos.lpp.polytechnique.fr/data/mirrors/CDF/test_files/cl_jp_pgp_20031001_v52.cdf',
            'https://hephaistos.lpp.polytechnique.fr/data/mirrors/CDF/test_files/cl_sp_edi_00000000_v01.cdf',
            'https://hephaistos.lpp.polytechnique.fr/data/mirrors/CDF/test_files/cluster-2_cp3drl_2002052000000_v1.cdf',
            'https://hephaistos.lpp.polytechnique.fr/data/mirrors/CDF/test_files/de_uv_sai_19910218_v01.cdf',
            'https://hephaistos.lpp.polytechnique.fr/data/mirrors/CDF/test_files/ge_k0_cpi_19921231_v02.cdf',
            'https://hephaistos.lpp.polytechnique.fr/data/mirrors/CDF/test_files/i1_av_ott_1983351130734_v01.cdf',
            'https://hephaistos.lpp.polytechnique.fr/data/mirrors/CDF/test_files/im_k0_euv_20011231_v01.cdf',
            'https://hephaistos.lpp.polytechnique.fr/data/mirrors/CDF/test_files/im_k0_rpi_20051218_v01.cdf',
            'https://hephaistos.lpp.polytechnique.fr/data/mirrors/CDF/test_files/im_k1_rpi_20051217_v01.cdf',
            'https://hephaistos.lpp.polytechnique.fr/data/mirrors/CDF/test_files/mms1_fpi_fast_sitl_20150801132440_v0.0.0.cdf',
            'https://hephaistos.lpp.polytechnique.fr/data/mirrors/CDF/test_files/po_h4_pwi_19970901_v01.cdf',
            'https://hephaistos.lpp.polytechnique.fr/data/mirrors/CDF/test_files/po_h9_pwi_1997010103_v01.cdf',
            'https://hephaistos.lpp.polytechnique.fr/data/mirrors/CDF/test_files/po_k0_uvi_20051230_v02.cdf',
            'https://hephaistos.lpp.polytechnique.fr/data/mirrors/CDF/test_files/tha_l2_fgm_20070729_v01.cdf',
            'https://hephaistos.lpp.polytechnique.fr/data/mirrors/CDF/test_files/tha_l2_fgm_20101202_v01.cdf',
            'https://hephaistos.lpp.polytechnique.fr/data/mirrors/CDF/test_files/tha_l2_scm_20160831_v01.cdf',
            'https://hephaistos.lpp.polytechnique.fr/data/mirrors/CDF/test_files/timed_L1Cdisk_guvi_20060601005849_v01.cdf',
            'https://hephaistos.lpp.polytechnique.fr/data/mirrors/CDF/test_files/twins1_l1_imager_2010102701_v01.cdf',
            'https://hephaistos.lpp.polytechnique.fr/data/mirrors/CDF/test_files/ulysses.cdf',
            #manually added files 
            "https://hephaistos.lpp.polytechnique.fr/data/mirrors/CDF/test_files/mms1_fpi_brst_l2_des-moms_20180101005543_v3.3.0.cdf",
            "https://hephaistos.lpp.polytechnique.fr/data/mirrors/CDF/test_files/mms1_fpi_brst_l2_dis-qmoms_20170703052703_v3.3.0.cdf",
            "https://hephaistos.lpp.polytechnique.fr/data/mirrors/CDF/test_files/mms1_scm_srvy_l2_scsrvy_20190301_v2.2.0.cdf",
            "https://hephaistos.lpp.polytechnique.fr/data/mirrors/CDF/test_files/c1_cp_fgm_spin_20080101_v01.cdf",
            "https://hephaistos.lpp.polytechnique.fr/data/mirrors/CDF/test_files/c1_jp_pmp_20081001_v32.cdf",
            "https://hephaistos.lpp.polytechnique.fr/data/mirrors/CDF/test_files/c1_jp_pse_20090101_v28.cdf",
            "https://hephaistos.lpp.polytechnique.fr/data/mirrors/CDF/test_files/c1_pp_cis_20080101_v01.cdf",
            "https://hephaistos.lpp.polytechnique.fr/data/mirrors/CDF/test_files/c1_pp_dwp_20100111_v01.cdf",
            )
    def test_open(self, url):
        print(f"downloading {url}")
        file = dl_data(url)
        if file is not None:
            try:
                print(f"loading {file.name}")
                cdf = pycdfpp.load(file.name)
                self.assertIsNotNone(cdf)
                if cdf is not None and has_pycdf:
                    ref_cdf = pycdf.CDF(file.name)
                    self.assertEqual(len(cdf), len(ref_cdf))
                    self.assertEqual(len(cdf.attributes), len(ref_cdf.attrs))
                    for name, attr in cdf.attributes.items():
                        self.assertIn(name, ref_cdf.attrs)
                        ref_attr = ref_cdf.attrs[name]
                        for val, ref_val in zip(attr, ref_attr):
                            self.assertEqual(val, ref_val)

            except:
                self.fail(sys.exc_info()[0])
            os.unlink(file.name)


if __name__ == '__main__':
    unittest.main()
