===========
Performance
===========

How does ``pycdfpp`` compare with the two other Python CDF readers,
`spacepy.pycdf <https://spacepy.github.io/pycdf.html>`_ and
`cdflib <https://cdflib.readthedocs.io/>`_? This page measures everyday tasks on real
files, then explains where the differences come from.

The short answer
================

On an AMD Ryzen 7 5800X under Linux, ``pycdfpp`` is faster on every task we measured but
one. When you read whole files of compressed data, it is 5.6× to 15× faster. When you open a
file, or convert time variables, it is 11× to about 3700× faster. Writing compressed files
is 3.7× to 14× faster. Writing without compression, ``spacepy`` is 13% faster.

On an Apple M2 (MacBook Air), ``pycdfpp`` is faster on every task, that one included: 5.3× to
12× when reading whole compressed files, 11× to about 4400× when opening a file or converting
time, 9× to 10× when writing compressed files, and 1.2× without compression.

Results
=======

AMD Ryzen 7 5800X, Linux
------------------------

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
     - 264 ms (407×)
     - 9.9 ms (15×)
   * - Read B and its time axis as ``datetime64``
     - MMS FGM survey, 1.2 M points, gzip, TT2000
     - **7.8 ms**
     - 3.77 s (483×)
     - 306 ms (39×)
   * - Read B and its time axis as ``datetime64``
     - Wind MFI, 0.9 M points, CDF_EPOCH
     - **3.6 ms**
     - 40.2 ms (11×)
     - 13.3 s (3718×)
   * - Read every variable of a file
     - MMS FPI electron distribution, 178 MB, gzip
     - **110 ms**
     - 960 ms (8.7×)
     - 620 ms (5.6×)
   * - Read every variable of a folder
     - 23 CDAWeb files, 11 missions, 528 MB
     - **391 ms**
     - 2.37 s (6.1×)
     - 2.81 s (7.2×)
   * - Same folder, 8 threads
     - 23 CDAWeb files, 11 missions, 528 MB
     - **161 ms**
     - not thread-safe
     - 2.41 s (15×)
   * - Write B and its time axis, gzip
     - MMS FGM survey, 1.2 M points, 29 MB
     - **46.0 ms**
     - 458 ms (10×)
     - 172 ms (3.7×)
   * - Write B and its time axis, uncompressed
     - MMS FGM survey, 1.2 M points, 29 MB
     - 14.1 ms
     - **12.3 ms (0.9×)**
     - 17.4 ms (1.2×)
   * - Write a particle distribution file, gzip
     - MMS FPI electron distribution, 210 MB
     - **275 ms**
     - 3.93 s (14×)
     - 1.87 s (6.8×)

(N×) means N times longer than ``pycdfpp``. Each time is the median of 5 runs, after
one warm-up run, so the files are in the page cache.

Measured on an AMD Ryzen 7 5800X (8 cores, AVX2, no AVX-512), Linux, Python 3.13, with the packages
from PyPI: pycdfpp 0.15.0, spacepy 0.7.0 (which bundles NASA's CDF library 3.9.0),
cdflib 1.3.14 and numpy 2.5.3.

Apple M2, macOS
---------------

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
     - **0.3 ms**
     - 369 ms (1062×)
     - 8.3 ms (24×)
   * - Read B and its time axis as ``datetime64``
     - MMS FGM survey, 1.2 M points, gzip, TT2000
     - **8.3 ms**
     - 4.36 s (523×)
     - 239 ms (29×)
   * - Read B and its time axis as ``datetime64``
     - Wind MFI, 0.9 M points, CDF_EPOCH
     - **2.5 ms**
     - 28.5 ms (11×)
     - 10.9 s (4388×)
   * - Read every variable of a file
     - MMS FPI electron distribution, 178 MB, gzip
     - **93.7 ms**
     - 1.13 s (12×)
     - 499 ms (5.3×)
   * - Read every variable of a folder
     - 23 CDAWeb files, 11 missions, 528 MB
     - **295 ms**
     - 2.62 s (8.9×)
     - 2.74 s (9.3×)
   * - Same folder, 8 threads
     - 23 CDAWeb files, 11 missions, 528 MB
     - **186 ms**
     - not thread-safe
     - 2.28 s (12×)
   * - Write B and its time axis, gzip
     - MMS FGM survey, 1.2 M points, 29 MB
     - **55.7 ms**
     - 572 ms (10×)
     - 507 ms (9.1×)
   * - Write B and its time axis, uncompressed
     - MMS FGM survey, 1.2 M points, 29 MB
     - **10.6 ms**
     - 12.8 ms (1.2×)
     - 12.3 ms (1.2×)
   * - Write a particle distribution file, gzip
     - MMS FPI electron distribution, 210 MB
     - **425 ms**
     - 4.39 s (10×)
     - 4.23 s (10×)

Same files and script. Measured on a MacBook Air with an Apple M2 (4 performance and 4
efficiency cores, NEON, 16 GB), macOS 26.6, on AC power, Python 3.14. pycdfpp 0.16.0 was
built from source like the wheels (``-Db_ndebug=if-release``), with the Apple Silicon work of
:doc:`optimizations`. spacepy 0.7.0 bundles NASA's CDF library 3.9.1 there; cdflib 1.3.14,
numpy 2.5.3.

Compared with the Ryzen, the M2 reads files within 6% or faster on one thread, and opens them
faster. With threads, and when writing gzip, the Ryzen's 16 hardware threads win: compression
keeps every core busy, and 4 of the M2's 8 cores are efficiency cores.

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

Writing, each library writes the same variables, with gzip level 6 when compressed:
``pycdfpp.save`` of a ``CDF`` built with ``add_variable``, borrowing the data arrays
with ``copy=False``; ``spacepy``'s ``cdf.new`` and
``raw_var`` with TT2000 integers; ``cdflib``'s ``CDFWriter``. Every written file is read
back and checked.

We checked that the three libraries return the same values for every variable of the
23 files.

Why is pycdfpp faster?
======================

Each row of the table has its own reason. :doc:`optimizations` explains how each one works,
in detail, with the measurements behind it.

Opening a file
--------------

1. ``pycdfpp`` maps the file in memory and parses only its headers, in C++.
   Variable data is read later, when you ask for it.
2. NASA's library, used by ``spacepy``, checks the file's MD5 checksum every time it
   opens a file that has one. MMS files all have one.
3. Checking the checksum means reading and hashing the whole file. For 178 MB, that
   is about 250 ms, before you have read anything.
4. ``cdflib`` parses the headers in Python, which takes about 10 ms.

``pycdfpp`` does not check checksums. If you need that check, use NASA's tools.

Converting time
---------------

1. ``pycdfpp`` converts CDF time values to ``datetime64[ns]`` in C++, using SIMD
   instructions, and exactly. On the Ryzen (AVX2), it converts about one billion
   TT2000 values and 2.6 billion CDF_EPOCH values per second. On the M2 (NEON), 4 to 6
   billion sorted TT2000 values and 2.4 billion CDF_EPOCH values per second.
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

Most mission files compress their variables with gzip, in many blocks: an MMS FPI
distribution variable has 640 of them. Two things make ``pycdfpp`` faster there:

1. It decompresses the blocks of a variable on all cores at once. The two other libraries
   decompress them one after another.
2. It uses `libdeflate <https://github.com/ebiggers/libdeflate>`_, the two others zlib.
   On these files, libdeflate alone decompresses 1.5 to 1.7 times faster.
3. On Linux, big buffers use 2 MB huge pages: filling them on one thread is 2 to 3
   times faster. Before the threads start, one thread touches every page. Otherwise
   several threads fault the same fresh huge page at once, and the kernel zeroes one
   for each of them.

Decompressing takes most of the time for big compressed files, so these rows gain the
most from it: 5.6× to 8.7× here, up to 15× with threads.

Using threads
-------------

1. ``pycdfpp`` releases Python's GIL while it reads and decompresses. So several
   threads really run at the same time.
2. With 8 threads, the folder loads in 0.16 s instead of 0.39 s. The biggest file alone
   takes 0.11 s, even with its own blocks decompressed in parallel.
3. ``cdflib`` runs mostly Python code, which holds the GIL. Threads barely help it.
4. NASA's library keeps global state, so ``spacepy`` can't be used from several threads.

See :doc:`reading` for how to load many files with a thread pool.

Writing
-------

1. ``pycdfpp`` cuts compressed variables into 256 KB blocks and compresses them on all
   cores, with libdeflate.
2. NASA's library, used by ``spacepy``, and ``cdflib`` both compress with zlib, on one
   thread.
3. Without compression, writing is mostly copying memory to the file. With
   ``copy=False`` (see :doc:`writing`), ``pycdfpp`` writes straight from the arrays, like
   ``spacepy``. The rest of the gap is the time axis: ``pycdfpp`` converts it from
   ``datetime64``, which takes 1.3 ms on the Ryzen and 0.2 ms on the M2, while ``spacepy``
   is given TT2000 integers.
4. On macOS, ``pycdfpp`` writes files with plain ``write()`` calls: the C++ library of macOS
   copies big writes through a small buffer, which made this row 3 times slower.

Scaling
=======

The script
`benchmarks/python_libs/scaling.py <https://github.com/SciQLop/CDFpp/blob/main/benchmarks/python_libs/scaling.py>`_
measures how each library scales with threads and with file size. Same machines and
versions as above: the Ryzen first, then the M2.

Reading the 23-file folder with a thread pool (``spacepy`` can't use threads):

.. list-table::
   :header-rows: 1

   * - Threads
     - pycdfpp
     - cdflib
   * - 1
     - 0.35 s
     - 2.79 s
   * - 2
     - 0.21 s (1.6×)
     - 2.31 s (1.2×)
   * - 4
     - 0.16 s (2.2×)
     - 2.27 s (1.2×)
   * - 8
     - 0.16 s (2.3×)
     - 2.53 s (1.1×)
   * - 16
     - 0.16 s (2.2×)
     - 2.47 s (1.1×)

(N×) is the speed-up over one thread. ``pycdfpp`` stops at about 2.3× because one file
of the folder takes 0.11 s on its own.

Writing and reading one ``float32`` variable of 3 components, gzip compressed, in MB/s
of values (higher is better). Every library reads the file written by NASA's library:

.. list-table::
   :header-rows: 1

   * - Size
     - Write: pycdfpp
     - spacepy
     - cdflib
     - Read: pycdfpp
     - spacepy
     - cdflib
   * - 1 MB
     - 111
     - 41
     - 112
     - 498
     - 277
     - 453
   * - 10 MB
     - 606
     - 42
     - 113
     - 4085
     - 273
     - 438
   * - 100 MB
     - 697
     - 42
     - 104
     - 3450
     - 270
     - 317
   * - 1000 MB
     - 712
     - 42
     - 103
     - 3550
     - 264
     - 326

1. At 1 MB, a variable is too small to be worth several threads, so ``pycdfpp``
   compresses and decompresses on one thread, like the others.
2. From 10 MB up, it compresses and decompresses on all cores, and keeps that speed up
   to 1 GB.

On the Apple M2, reading the same folder with a thread pool:

.. list-table::
   :header-rows: 1

   * - Threads
     - pycdfpp
     - cdflib
   * - 1
     - 0.31 s
     - 2.71 s
   * - 2
     - 0.22 s (1.4×)
     - 2.15 s (1.3×)
   * - 4
     - 0.19 s (1.6×)
     - 2.08 s (1.3×)
   * - 8
     - 0.19 s (1.6×)
     - 2.20 s (1.2×)
   * - 16
     - 0.19 s (1.6×)
     - 2.28 s (1.2×)

One thread already decompresses each file's blocks on all cores, and the biggest file takes
0.09 s on its own: threads gain less than on the Ryzen.

And one ``float32`` variable of 3 components, gzip compressed, in MB/s of values:

.. list-table::
   :header-rows: 1

   * - Size
     - Write: pycdfpp
     - spacepy
     - cdflib
     - Read: pycdfpp
     - spacepy
     - cdflib
   * - 1 MB
     - 93
     - 36
     - 37
     - 475
     - 257
     - 400
   * - 10 MB
     - 449
     - 36
     - 38
     - 2494
     - 257
     - 400
   * - 100 MB
     - 484
     - 36
     - 38
     - 2734
     - 256
     - 398
   * - 1000 MB
     - 441
     - 37
     - 37
     - 2757
     - 251
     - 393

Writing with gzip reaches 12 times the speed of spacepy and cdflib, reading 7 to 11 times.
