# Compression benchmark

Compares the CDF standard's gzip with the experimental zstd and blosc2 codecs on real CDF files.

```bash
meson setup build -Dwith_experimental_zstd=true -Dwith_experimental_blosc2=true
ninja -C build
pip install numpy pyyaml requests blosc2   # Python >= 3.14 (stdlib compression.zstd)
cd benchmarks/compression
PYTHONPATH=../../build python corpus.py                 # download the corpus (~1.1 GiB, cached)
PYTHONPATH=../../build python sweep.py sweep.csv        # per-variable codec/filter/shape-hint grid
PYTHONPATH=../../build python end_to_end.py e2e.csv     # CDFpp save + load, values checked
python summarize.py sweep.csv e2e.csv > summary.json
```

- `corpus.py`: 23 current CDAWeb datasets (one day each) plus 16 historical NASA CDF test files.
  Set `CDFPP_CORPUS_DIR` to change the cache location.
- `sweep.py`: compresses the raw bytes of every non-string variable, exactly what CDFpp hands to the
  codec, with gzip-6, zstd and a grid of blosc2 settings. python-blosc2 bundles the same C-blosc2
  version as CDFpp's wrap.
- `end_to_end.py`: sets every variable to one codec, saves with CDFpp, reloads eagerly and checks
  all values, recording file size and save/load time.

Timings are single-threaded wall clock, best of 3 for files under 50 MiB.

## Projection on the whole SPDF archive

```bash
cd benchmarks/compression
PYTHONPATH=../../build python archive_census.py census.json                       # 1.2 GB file list, cached
PYTHONPATH=../../build python archive_projection.py measure   census.json samples.csv
PYTHONPATH=../../build python archive_projection.py variables census.json samples.csv
PYTHONPATH=../../build python archive_projection.py project   census.json samples.csv > projection.json
```

- `archive_census.py`: reads SPDF's `pub/catalogs/filelist.gz` and sums the bytes of every `.cdf` under
  `pub/data` by mission, instrument, dataset directory and family (the same dataset on sibling
  spacecraft, e.g. MMS1-4).
- `archive_projection.py measure`: two median-sized files from each of the 40 largest families,
  downloaded from SPDF and run through `end_to_end.py`, plus a transposed-layout estimate.
- `archive_projection.py variables`: per-variable GZIP, Zstd and Blosc2 filter sizes, so `project`
  can model a writer choosing the best codec per variable.
- `archive_projection.py project`: scales each family's archive bytes by its samples' size ratios.

`results/archive/` holds the 2026-09-23 file list's samples and projection (the census itself is
regenerated from the list, which SPDF updates daily).
