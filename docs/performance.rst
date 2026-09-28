===========
Performance
===========

How does ``pycdfpp`` compare with the two other Python CDF readers,
`spacepy.pycdf <https://spacepy.github.io/pycdf.html>`_ and
`cdflib <https://cdflib.readthedocs.io/>`_? This page measures everyday tasks on real
files, then explains where the differences come from.

The short answer
================

``pycdfpp`` is faster on every task we measured but one. When you read whole files of
compressed data, it is 2.9× to 10× faster. When you open a file, or convert time
variables, it is 12× to about 3800× faster. Writing compressed files is 3× to 12×
faster. Writing without compression, ``spacepy`` is 20% faster.

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
     - **0.8 ms**
     - 251 ms (305×)
     - 9.9 ms (12×)
   * - Read B and its time axis as ``datetime64``
     - MMS FGM survey, 1.2 M points, gzip, TT2000
     - **18.0 ms**
     - 3.77 s (210×)
     - 307 ms (17×)
   * - Read B and its time axis as ``datetime64``
     - Wind MFI, 0.9 M points, CDF_EPOCH
     - **3.6 ms**
     - 41.3 ms (12×)
     - 13.4 s (3773×)
   * - Read every variable of a file
     - MMS FPI electron distribution, 178 MB, gzip
     - **218 ms**
     - 942 ms (4.3×)
     - 628 ms (2.9×)
   * - Read every variable of a folder
     - 23 CDAWeb files, 11 missions, 528 MB
     - **553 ms**
     - 2.28 s (4.1×)
     - 2.85 s (5.2×)
   * - Same folder, 8 threads
     - 23 CDAWeb files, 11 missions, 528 MB
     - **237 ms**
     - not thread-safe
     - 2.39 s (10×)
   * - Write B and its time axis, gzip
     - MMS FGM survey, 1.2 M points, 29 MB
     - **53.3 ms**
     - 454 ms (8.5×)
     - 171 ms (3.2×)
   * - Write B and its time axis, uncompressed
     - MMS FGM survey, 1.2 M points, 29 MB
     - 15.4 ms
     - **12.6 ms (0.8×)**
     - 17.0 ms (1.1×)
   * - Write a particle distribution file, gzip
     - MMS FPI electron distribution, 210 MB
     - **322 ms**
     - 3.87 s (12×)
     - 1.89 s (5.9×)

(N×) means N times longer than ``pycdfpp``. Each time is the median of 5 runs, after
one warm-up run, so the files are in the page cache.

Measured on an AMD Ryzen 7 5800X (8 cores, AVX2, no AVX-512), Linux, Python 3.13, with the packages
from PyPI: pycdfpp 0.14.0, spacepy 0.7.0 (which bundles NASA's CDF library 3.9.0),
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

Writing, each library writes the same variables, with gzip level 6 when compressed:
``pycdfpp.save`` of a ``CDF`` built with ``add_variable``; ``spacepy``'s ``cdf.new`` and
``raw_var`` with TT2000 integers; ``cdflib``'s ``CDFWriter``. Every written file is read
back and checked.

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
   is about 250 ms, before you have read anything.
4. ``cdflib`` parses the headers in Python, which takes about 10 ms.

``pycdfpp`` does not check checksums. If you need that check, use NASA's tools.

Converting time
---------------

1. ``pycdfpp`` converts CDF time values to ``datetime64[ns]`` in C++, using SIMD
   instructions, and exactly. On the test machine (AVX2), it converts about one billion
   TT2000 values and 2.6 billion CDF_EPOCH values per second.
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

Decompressing takes most of the time for big compressed files, so these rows gain the
most from it: 2.8× to 5× here, up to 10× with threads.

Using threads
-------------

1. ``pycdfpp`` releases Python's GIL while it reads and decompresses. So several
   threads really run at the same time.
2. With 8 threads, the folder loads in 0.23 s instead of 0.55 s. The biggest file alone
   takes 0.22 s, even with its own blocks decompressed in parallel.
3. ``cdflib`` runs mostly Python code, which holds the GIL. Threads barely help it.
4. NASA's library keeps global state, so ``spacepy`` can't be used from several threads.

See :doc:`reading` for how to load many files with a thread pool.

Writing
-------

1. ``pycdfpp`` cuts compressed variables into 256 KB blocks and compresses them on all
   cores, with libdeflate.
2. NASA's library, used by ``spacepy``, and ``cdflib`` both compress with zlib, on one
   thread.
3. Without compression, writing is mostly copying memory to the file. ``pycdfpp`` first
   copies the array into the variable; ``spacepy`` writes straight from the array. That
   copy is why ``spacepy`` is 20% faster there.

Scaling
=======

The script
`benchmarks/python_libs/scaling.py <https://github.com/SciQLop/CDFpp/blob/main/benchmarks/python_libs/scaling.py>`_
measures how each library scales with threads and with file size. Same machine and
versions as above.

Reading the 23-file folder with a thread pool (``spacepy`` can't use threads):

.. list-table::
   :header-rows: 1

   * - Threads
     - pycdfpp
     - cdflib
   * - 1
     - 0.53 s
     - 2.77 s
   * - 2
     - 0.34 s (1.6×)
     - 2.31 s (1.2×)
   * - 4
     - 0.26 s (2.0×)
     - 2.26 s (1.2×)
   * - 8
     - 0.24 s (2.2×)
     - 2.39 s (1.2×)
   * - 16
     - 0.24 s (2.2×)
     - 2.44 s (1.1×)

(N×) is the speed-up over one thread. ``pycdfpp`` stops at about 2.2× because one file
of the folder takes 0.22 s on its own.

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
     - 113
     - 43
     - 112
     - 502
     - 283
     - 463
   * - 10 MB
     - 466
     - 43
     - 115
     - 4027
     - 284
     - 468
   * - 100 MB
     - 644
     - 43
     - 105
     - 1206
     - 271
     - 324
   * - 1000 MB
     - 684
     - 43
     - 106
     - 1249
     - 267
     - 312

1. At 1 MB, a variable is too small to be worth several threads, so ``pycdfpp``
   compresses on one thread, like the others.
2. From 10 MB up, it compresses and decompresses on all cores.
3. Reading 10 MB is faster than reading 100 MB because of memory, not decompression.
   Values of up to 32 MB reuse memory the previous run freed. Bigger ones get fresh
   memory from the system, and the first write to each fresh page costs a page fault.
   With glibc's ``mmap_threshold`` raised so that memory is reused, 100 MB reads at
   4.4 GB/s too. A program that reads a big file once pays those page faults, so
   1.2 GB/s is what to expect.
