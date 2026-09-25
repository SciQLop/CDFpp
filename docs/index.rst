=====
CDFpp
=====

**Read and write NASA CDF files, fast, from Python, C++ or your browser.**

`CDF <https://cdf.gsfc.nasa.gov/>`_ (Common Data Format) is the file format most
space physics missions use to distribute their data. CDFpp is a modern implementation
of it, written from scratch in C++20. Its Python package is called ``pycdfpp``.

.. code-block:: python

    import urllib.request
    import pycdfpp

    url = ("https://spdf.gsfc.nasa.gov/pub/data/ace/mag/level_2_cdaweb/"
           "mfi_h0/2020/ac_h0_mfi_20200101_v07.cdf")
    cdf = pycdfpp.load(urllib.request.urlopen(url).read())

    field = cdf["BGSEc"].values                  # a numpy array, shape (5401, 3)
    time = pycdfpp.to_datetime64(cdf["Epoch"])   # numpy datetime64

.. code-block:: console

    $ pip install pycdfpp

What do you want to do?
=======================

.. grid:: 1 1 2 2
    :gutter: 3

    .. grid-item-card:: 🔭 Read CDF files
        :link: reading
        :link-type: doc

        You are a scientist. You have CDF files from a mission archive and want the
        data in numpy, xarray or pandas.

        Start with the :doc:`quickstart`, then :doc:`reading`.

    .. grid-item-card:: 🛰️ Produce CDF files
        :link: writing
        :link-type: doc

        You work on a ground segment or instrument team. You need to write clean,
        ISTP-compliant CDF files that other people and tools can read.

        Read :doc:`concepts`, then :doc:`writing` and :doc:`istp`.

    .. grid-item-card:: ⚙️ Use CDFpp from C++
        :link: cpp
        :link-type: doc

        CDFpp is a C++20 library, made mostly of headers. No global state, so it is
        safe to use from many threads.

        Go to the :doc:`cpp`.

    .. grid-item-card:: 🌐 No install: use your browser
        :link: explorer
        :link-type: doc

        Open, plot, validate and compare CDF files in the
        `CDFpp Explorer <https://sciqlop.github.io/CDFpp/>`_. Your files never leave
        your machine.

        See :doc:`explorer`.

Why CDFpp?
==========

- **Fast.** Files open instantly: data is only read when you ask for it. Reading runs at
  up to ~4 GB/s, and time conversions use SIMD instructions.
- **Complete.** Reads and writes CDF versions 2.2 to 3.x, row and column major files,
  gzip and RLE compressed files and variables, and all CDF data types, including the
  three time types.
- **Thread-safe.** NASA's C library keeps global state, so it cannot safely be used
  from several threads. CDFpp can.
- **Easy to install.** ``pip install pycdfpp`` ships ready-made packages for Linux,
  Windows and macOS (Intel and ARM). No compiler, no NASA library needed.
- **Permissive license.** MIT, so it fits in any project and any Linux distribution.

.. toctree::
   :hidden:
   :caption: Getting started

   installation
   quickstart
   concepts

.. toctree::
   :hidden:
   :caption: Reading data

   reading
   time
   cookbook
   examples/index

.. toctree::
   :hidden:
   :caption: Producing files

   writing
   istp
   compression

.. toctree::
   :hidden:
   :caption: Tools

   cli
   explorer
   cpp

.. toctree::
   :hidden:
   :caption: Reference

   faq
   api
   history
   contributing
   authors
