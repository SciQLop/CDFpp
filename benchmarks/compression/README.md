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
