===========
Performance
===========

How does ``pycdfpp`` compare with the two other Python CDF readers,
`spacepy.pycdf <https://spacepy.github.io/pycdf.html>`_ and
`cdflib <https://cdflib.readthedocs.io/>`_? This page measures everyday tasks on real
files, then explains where the differences come from.

The short answer
================

``pycdfpp`` is faster on every task we measured. When you read a whole file of
compressed data, it is 1.4× to 2× faster. When you open a file, or convert time
variables, it is 7× to about 3000× faster.

Results
=======

.. list-table::
   :header-rows: 1
   :widths: 30 26 12 16 16

   * - Task
     - Data
     - pycdfpp
     - spacepy.pycdf
     - cdflib
   * - Open a file, list variables, read all attributes
     - MMS FPI electron distribution, 178 MB
     - **0.6 ms**
     - 266 ms (418×)
     - 10.2 ms (16×)
   * - Read B and its time axis as ``datetime64``
     - MMS FGM survey, 1.2 M points, gzip, TT2000
     - **42 ms**
     - 3.81 s (91×)
     - 317 ms (7.5×)
   * - Read B and its time axis as ``datetime64``
     - Wind MFI, 0.9 M points, CDF_EPOCH
     - **4.7 ms**
     - 43.5 ms (9.2×)
     - 14.1 s (2965×)
   * - Read every variable of a file
     - MMS FPI electron distribution, 178 MB, gzip
     - **473 ms**
     - 987 ms (2.1×)
     - 645 ms (1.4×)
   * - Read every variable of a folder
     - 23 CDAWeb files, 11 missions, 528 MB
     - **1.13 s**
     - 2.31 s (2.0×)
     - 2.84 s (2.5×)
   * - Same folder, 8 threads
     - 23 CDAWeb files, 11 missions, 528 MB
     - **482 ms**
     - not thread-safe
     - 2.49 s (5.2×)

(N×) means N times longer than ``pycdfpp``. Each time is the median of 5 runs, after
one warm-up run, so the files are in the page cache.

Measured on an AMD Ryzen 7 5800X (8 cores, AVX2, no AVX-512), Linux, Python 3.13, with the packages
from PyPI: pycdfpp 0.13.1, spacepy 0.7.0 (which bundles NASA's CDF library 3.9.0),
cdflib 1.3.14 and numpy 2.5.3.

How it was measured
===================

The script is
`benchmarks/python_libs/compare.py <https://github.com/SciQLop/CDFpp/blob/main/benchmarks/python_libs/compare.py>`_.
It downloads the files from CDAWeb once, then caches them. Run it yourself:

.. code-block:: console

    $ pip install pycdfpp spacepy cdflib requests
    $ python benchmarks/python_libs/compare.py

Each library is used the way its documentation shows, with its fastest documented
way to get ``datetime64``:

- ``pycdfpp``: ``pycdfpp.load(path)``, ``cdf[name].values`` and
  ``pycdfpp.to_datetime64(cdf[name])``.
- ``spacepy``: ``pycdf.CDF(path)`` and ``cdf.raw_var(name)[...]`` for values. For
  CDF_EPOCH times, ``spacepy.time.Ticktock(raw, "CDF").UNX``, which is vectorized.
  Ticktock doesn't handle TT2000, so TT2000 times go through ``cdf[name][...]``.
- ``cdflib``: ``cdflib.CDF(path)``, ``cdf.varget(name)`` and
  ``cdflib.cdfepoch.to_datetime(...)``.

We checked that the three libraries return the same values for every variable of the
23 files.

Why is pycdfpp faster?
======================

Each row of the table has its own reason.

Opening a file
--------------

1. ``pycdfpp`` maps the file in memory and parses only its headers, in C++.
   Variable data is read later, when you ask for it.
2. NASA's library, used by ``spacepy``, checks the file's MD5 checksum every time it
   opens a file that has one. MMS files all have one.
3. Checking the checksum means reading and hashing the whole file. For 178 MB, that
   is about 265 ms, before you have read anything.
4. ``cdflib`` parses the headers in Python, which takes about 10 ms.

``pycdfpp`` does not check checksums. If you need that check, use NASA's tools.

Converting time
---------------

1. ``pycdfpp`` converts CDF time values to ``datetime64[ns]`` in C++, using SIMD
   instructions. On the test machine (AVX2), it converts one to two billion values per
   second.
2. For TT2000, ``spacepy`` creates one Python ``datetime`` object per value. That takes
   seconds for a million points. ``datetime`` also stops at microseconds, so
   nanoseconds are lost. For CDF_EPOCH, ``spacepy.time.Ticktock`` is vectorized, so the
   gap is much smaller.
3. ``cdflib`` converts TT2000 with numpy, which is reasonably fast. But it converts
   CDF_EPOCH in a Python loop, one value at a time. That is why the Wind file takes
   14 s.

CDF_EPOCH is a plain count of milliseconds, with no leap seconds. So you can also
convert it yourself with one line of numpy, whatever the library. TT2000 needs a
leap-second table, which is why a library function matters more there.

``pycdfpp`` also keeps the fractions of milliseconds that CDF_EPOCH values can hold.
``cdflib`` rounds them down to the millisecond.

Decompressing
-------------

Most mission files compress their variables with gzip. ``pycdfpp`` decompresses them
with `libdeflate <https://github.com/ebiggers/libdeflate>`_. The two other libraries
use zlib. On these files, libdeflate decompresses 1.5 to 1.7 times faster.

Decompressing takes most of the time for big compressed files. That is why the gap
is only 1.4× to 2× on those rows.

Using threads
-------------

1. ``pycdfpp`` releases Python's GIL while it reads and decompresses. So several
   threads really run at the same time.
2. With 8 threads, the folder loads in 0.48 s instead of 1.13 s. It can't go lower:
   the biggest file alone takes 0.47 s.
3. ``cdflib`` runs mostly Python code, which holds the GIL. Threads barely help it.
4. NASA's library keeps global state, so ``spacepy`` can't be used from several threads.

See :doc:`reading` for how to load many files with a thread pool.
