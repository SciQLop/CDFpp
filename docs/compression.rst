===========
Compression
===========

Reading compressed files needs nothing from you: ``pycdfpp`` decompresses on the fly.
This page is about **writing**: which compression to choose, and where to set it.

The short answer
================

For files you distribute, compress each variable with gzip:

.. code-block:: python

    import numpy as np
    import pycdfpp

    cdf = pycdfpp.CDF()
    cdf.add_variable("B", values=np.zeros((10_000, 3), dtype=np.float32),
                     compression=pycdfpp.CompressionType.gzip_compression)
    pycdfpp.save(cdf, "compressed.cdf")

Gzip is part of the CDF standard, so every CDF reader can open the file.

The available codecs
====================

They are listed in :class:`pycdfpp.CompressionType`.

.. list-table::
   :header-rows: 1
   :widths: 25 15 60

   * - ``CompressionType``
     - Standard?
     - When to use it
   * - ``no_compression``
     - yes
     - The default. Fastest to read and write, biggest files.
   * - ``gzip_compression``
     - yes
     - **The safe choice.** Good compression, readable everywhere.
   * - ``rle_compression``
     - yes
     - Only compresses runs of zeros. Rarely useful.
   * - ``zstd_compression``
     - **no**
     - Experimental. Only CDFpp can read it.
   * - ``blosc2_compression``
     - **no**
     - Experimental. Only CDFpp can read it.

``huff_compression`` and ``ahuff_compression`` also exist in the standard, but CDFpp
doesn't support them. Saving with them raises an error.

Per variable, or the whole file?
================================

You can compress each variable, the whole file, or both.

**Per variable** (``compression=`` in ``add_variable``, or ``var.compression = ...``):
each variable is compressed on its own. A reader only decompresses the variables it
uses, so lazy loading still pays off. **Prefer this for large files.**

**Whole file** (``cdf.compression = ...``): everything after the file header is
compressed as one block. Small files get a bit smaller this way, but a reader must
decompress the whole file to read anything.

.. code-block:: python

    cdf["B"].compression = pycdfpp.CompressionType.gzip_compression   # one variable
    cdf.compression = pycdfpp.CompressionType.gzip_compression        # the whole file

Here is what each option gives on the ACE magnetometer file from the :doc:`quickstart`,
which holds 398 kB of data:

.. list-table::
   :header-rows: 1

   * - Compression
     - Size
   * - none
     - 398 kB
   * - gzip, per variable
     - 208 kB
   * - gzip, whole file
     - 172 kB
   * - RLE, per variable
     - 395 kB
   * - blosc2, per variable
     - 177 kB
   * - zstd, per variable
     - 258 kB

Results depend a lot on the data. Measure on your own files before you choose.

Converting an existing file
===========================

Load the file, set the compression, and save it under a new name:

.. code-block:: python

    import urllib.request

    url = ("https://spdf.gsfc.nasa.gov/pub/data/ace/mag/level_2_cdaweb/"
           "mfi_h0/2020/ac_h0_mfi_20200101_v07.cdf")
    urllib.request.urlretrieve(url, "ac_h0_mfi_20200101_v07.cdf")

    cdf = pycdfpp.load("ac_h0_mfi_20200101_v07.cdf")
    for name in cdf:
        cdf[name].compression = pycdfpp.CompressionType.gzip_compression
    cdf.compression = pycdfpp.CompressionType.no_compression
    pycdfpp.save(cdf, "ac_h0_mfi_20200101_v07_gzip.cdf")

To compare the codecs on a file without writing code, open it in the
`CDFpp Explorer <https://sciqlop.github.io/CDFpp/>`_ and click **Convert codec**. It
converts the file with every codec, in your browser, and shows the sizes.

Experimental codecs: zstd and blosc2
====================================

These two codecs are **not** part of the CDF standard. NASA's library, and every other
CDF reader, will fail to open a file that uses them. ``pycdfpp`` warns you when you save
such a file:

.. code-block:: text

    ExperimentalCompressionWarning: saving with blosc2_compression: this is not standard
    CDF, and only CDFpp can read the file. Use gzip_compression for files meant to be shared.

So why are they here? Blosc2 is much better than gzip on typical space physics data. On
23 CDAWeb datasets, blosc2 files were 27% smaller than gzip, and loaded 3.7 times
faster. We are sharing these results with the CDF maintainers, in the hope that a
better codec joins the standard. The benchmark lives in ``benchmarks/compression/``.

Use them for files that stay inside your own pipeline: caches, intermediate products,
local archives. Don't distribute them.

If you do, silence the warning explicitly, so the choice is visible in your code:

.. code-block:: python

    import warnings

    cdf["BGSEc"].compression = pycdfpp.CompressionType.blosc2_compression
    with warnings.catch_warnings():
        warnings.simplefilter("ignore", pycdfpp.ExperimentalCompressionWarning)
        pycdfpp.save(cdf, "internal_cache.cdf")
