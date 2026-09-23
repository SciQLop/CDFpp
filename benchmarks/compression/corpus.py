"""Fetches a corpus of real CDF files: current CDAWeb datasets plus the historical NASA test files
already used by tests/full_corpus. Files are cached, so re-runs are offline."""
import os
from pathlib import Path

import requests

CACHE_DIR = Path(os.environ.get("CDFPP_CORPUS_DIR", Path.home() / ".cache" / "cdfpp-compression-corpus"))
CDAWEB_API = "https://cdaweb.gsfc.nasa.gov/WS/cdasr/1/dataviews/sp_phys/datasets"
LPP_MIRROR = "https://129.104.27.7/data/mirrors/CDF/test_files"

# (CDAWeb dataset id, day) -- one day per dataset, chosen to cover missions, instrument kinds
# (magnetometers, moments, distributions, waves, ephemeris) and data types.
CDAWEB_DATASETS = (
    ("MMS1_FGM_SRVY_L2", "2020-01-01"),
    ("MMS1_FPI_FAST_L2_DIS-MOMS", "2020-01-01"),
    ("MMS1_FPI_FAST_L2_DES-DIST", "2020-01-01"),
    ("MMS1_EDP_FAST_L2_DCE", "2020-01-01"),
    ("MMS1_SCM_SRVY_L2_SCSRVY", "2020-01-01"),
    ("MMS1_MEC_SRVY_L2_EPHT89D", "2020-01-01"),
    ("THA_L2_FGM", "2020-01-01"),
    ("THA_L2_ESA", "2020-01-01"),
    ("PSP_FLD_L2_MAG_RTN_1MIN", "2020-01-29"),
    ("PSP_SWP_SPI_SF00_L3_MOM", "2020-01-29"),
    ("SOLO_L2_MAG-RTN-NORMAL", "2021-01-01"),
    ("SOLO_L2_SWA-PAS-GRND-MOM", "2021-06-01"),
    ("WI_H2_MFI", "2020-01-01"),
    ("WI_PM_3DP", "2020-01-01"),
    ("WI_ELPD_3DP", "2020-01-01"),
    ("AC_H0_MFI", "2020-01-01"),
    ("AC_H0_SWE", "2020-01-01"),
    ("OMNI_HRO_1MIN", "2020-01-01"),
    ("RBSP-A_MAGNETOMETER_1SEC-GSM_EMFISIS-L3", "2018-01-01"),
    ("RBSPA_REL04_ECT-HOPE-SCI-L2SA", "2018-01-01"),
    ("MVN_MAG_L2-SUNSTATE-1SEC", "2020-01-01"),
    ("C1_CP_FGM_SPIN", "2010-01-01"),
    ("GE_EDB3SEC_MGF", "2005-01-01"),
)

# Same list as tests/full_corpus/test.py (minus pathological ones irrelevant to compression).
LPP_FILES = (
    "a1_k0_mpa_20050804_v02.cdf", "ac_h2_sis_20101105_v06.cdf", "ac_or_ssc_20031101_v01.cdf",
    "c1_cp_fgm_spin_20080101_v01.cdf", "c1_jp_pmp_20081001_v32.cdf", "c1_pp_cis_20080101_v01.cdf",
    "c1_waveform_wbd_200202080940_v01.cdf", "cl_jp_pgp_20031001_v52.cdf",
    "cluster-2_cp3drl_2002052000000_v1.cdf", "de_uv_sai_19910218_v01.cdf",
    "ge_k0_cpi_19921231_v02.cdf", "i1_av_ott_1983351130734_v01.cdf", "im_k0_euv_20011231_v01.cdf",
    "im_k0_rpi_20051218_v01.cdf", "mms1_fpi_brst_l2_des-moms_20180101005543_v3.3.0.cdf",
    "mms1_fpi_brst_l2_dis-qmoms_20170703052703_v3.3.0.cdf",
)


def _download(url: str, dest: Path, verify: bool = True) -> Path:
    if not dest.exists():
        response = requests.get(url, verify=verify, timeout=600)
        response.raise_for_status()
        dest.parent.mkdir(parents=True, exist_ok=True)
        dest.write_bytes(response.content)
    return dest


def _cdaweb_file_url(dataset: str, day: str) -> str:
    compact = day.replace("-", "")
    response = requests.get(f"{CDAWEB_API}/{dataset}/orig_data/{compact}T000000Z,{compact}T235959Z",
                            headers={"Accept": "application/json"}, timeout=120)
    response.raise_for_status()
    urls = [f["Name"] for f in response.json().get("FileDescription", []) if f["Name"].endswith(".cdf")]
    if not urls:
        raise LookupError(f"{dataset}: no CDF file on {day}")
    return urls[0]


def cdaweb_file(dataset: str, day: str) -> Path:
    cached = list((CACHE_DIR / "cdaweb" / dataset).glob("*.cdf"))
    if cached:
        return cached[0]
    url = _cdaweb_file_url(dataset, day)
    return _download(url, CACHE_DIR / "cdaweb" / dataset / url.rsplit("/", 1)[-1])


def lpp_file(name: str) -> Path:
    # The LPP mirror uses a self-signed certificate.
    return _download(f"{LPP_MIRROR}/{name}", CACHE_DIR / "lpp" / name, verify=False)


def corpus() -> list[tuple[str, Path]]:
    """[(source, path)] for every file that could be fetched; failures are reported, not fatal."""
    files = []
    fetchers = [("cdaweb", lambda d=d, day=day: cdaweb_file(d, day)) for d, day in CDAWEB_DATASETS]
    fetchers += [("lpp", lambda n=n: lpp_file(n)) for n in LPP_FILES]
    for source, fetch in fetchers:
        try:
            files.append((source, fetch()))
        except Exception as error:  # a missing dataset/day must not abort the whole benchmark
            print(f"skipped ({source}): {error}")
    return files


if __name__ == "__main__":
    import urllib3
    urllib3.disable_warnings()
    for source, path in corpus():
        print(f"{source:7s} {path.stat().st_size / 2**20:8.1f} MiB  {path.name}")
