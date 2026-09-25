=============
Reading files
=============

This guide covers everything about getting data out of a CDF file. The examples use
the ACE file from the :doc:`quickstart`. Download it first:

.. code-block:: python

    import urllib.request
    import pycdfpp

    url = ("https://spdf.gsfc.nasa.gov/pub/data/ace/mag/level_2_cdaweb/"
           "mfi_h0/2020/ac_h0_mfi_20200101_v07.cdf")
    urllib.request.urlretrieve(url, "ac_h0_mfi_20200101_v07.cdf")

Opening a file
==============

From disk
---------

.. code-block:: python

    cdf = pycdfpp.load("ac_h0_mfi_20200101_v07.cdf")

The path can be a string or a :class:`pathlib.Path`. If the file doesn't exist,
:func:`pycdfpp.load` raises :class:`FileNotFoundError`. If it is not a valid CDF file, it
raises :class:`ValueError`.

From memory
-----------

:func:`pycdfpp.load` also accepts bytes, or anything that exposes a memory buffer. This
is handy for files you download: no need to write them to disk.

.. code-block:: python

    with urllib.request.urlopen(url) as response:
        cdf = pycdfpp.load(response.read())

It works the same with ``requests``, S3 clients, zip archives, and so on.

Loading is lazy
---------------

Opening a file only reads its structure: variable names, shapes, types and attributes.
The values of a variable are read the first time you use them. So opening is fast, even
for a file of several gigabytes, and you only pay for the variables you use.

.. code-block:: python

    cdf = pycdfpp.load("ac_h0_mfi_20200101_v07.cdf")
    cdf["BGSEc"].values_loaded    # False: nothing read yet
    cdf["BGSEc"].values           # reads the data now
    cdf["BGSEc"].values_loaded    # True

To read everything at once, pass ``lazy_load=False``. It is useful when you will use
all the variables anyway, or when the file will disappear (a temporary file, for
example):

.. code-block:: python

    cdf = pycdfpp.load("ac_h0_mfi_20200101_v07.cdf", lazy_load=False)

Old files and special characters
--------------------------------

CDF files older than version 3.8 could not store UTF-8 text. Some contain Latin-1
characters instead, like ``°`` or ``µ``. By default, ``pycdfpp`` converts them to UTF-8
so they display correctly. Pass ``iso_8859_1_to_utf8=False`` to keep the raw bytes.

Exploring a file
================

Print a summary
---------------

``print(cdf)`` shows the global attributes, then every variable with its shape, type and
attributes. ``print(cdf["BGSEc"])`` shows just one variable:

.. code-block:: text

    BGSEc:
      shape: [ 5401, 3 ]
      type: CDF_REAL4
      record vary: True
      compression: GNU GZIP

      Attributes:
        FIELDNAM: "Mag Field vector, GSE coord"
        UNITS: "nT"
        DEPEND_0: "Epoch"
        ...

List the variables
------------------

A :class:`pycdfpp.CDF` object works like a read-only dictionary of variables:

.. code-block:: python

    len(cdf)                  # 17 variables
    list(cdf)                 # ['Epoch', 'Time_PB5', 'Magnitude', 'BGSEc', ...]
    "BGSEc" in cdf            # True
    var = cdf["BGSEc"]        # KeyError if the name doesn't exist

    for name, var in cdf.items():
        print(f"{name:15} {str(var.shape):15} {var.type}")

Describe a variable
-------------------

.. code-block:: python

    var = cdf["BGSEc"]
    var.name           # 'BGSEc'
    var.shape          # (5401, 3)
    var.type           # DataType.CDF_REAL4
    len(var)           # 5401: the number of records
    var.is_nrv         # False: it changes with time
    var.compression    # CompressionType.gzip_compression

Getting the values
==================

Numbers
-------

``.values`` returns a numpy array. The first dimension is the record number:

.. code-block:: python

    b = cdf["BGSEc"].values
    b.shape            # (5401, 3)
    b[0]               # the first record: [ 2.291 -1.483 -0.067]
    b[:, 2]            # the Z component, for all records

A variable also supports the Python buffer protocol, so you can pass it directly to
numpy and most scientific libraries:

.. code-block:: python

    import numpy as np

    b = np.asarray(cdf["BGSEc"])

Strings
-------

String variables come as fixed-width bytes. Use ``.values_encoded`` to get Python
strings:

.. code-block:: python

    cdf["label_BGSE"].values           # [[b'Bx GSE' b'By GSE' b'Bz GSE']]
    cdf["label_BGSE"].values_encoded   # [['Bx GSE' 'By GSE' 'Bz GSE']]

Time
----

Time variables hold CDF time values. Convert them to numpy ``datetime64`` in one call:

.. code-block:: python

    time = pycdfpp.to_datetime64(cdf["Epoch"])

The conversion works for all three CDF time types. :doc:`time` explains the options.

Reading attributes
==================

Variable attributes
-------------------

Each variable attribute holds one value. Read it with ``.value``:

.. code-block:: python

    attrs = cdf["BGSEc"].attributes

    attrs["UNITS"].value       # 'nT'
    attrs["FILLVAL"].value     # [-1e+31]
    attrs["VALIDMIN"].value    # [-65534.0, -65534.0, -65534.0]
    attrs["UNITS"].type()      # DataType.CDF_CHAR

    "UNITS" in attrs           # True
    list(attrs)                # ['FIELDNAM', 'VALIDMIN', 'VALIDMAX', ...]

Numeric values come as a list, even when there is a single number. So write
``attrs["FILLVAL"].value[0]`` to get the fill value itself.

Global attributes
-----------------

A global attribute holds a list of entries. Index it like a list:

.. code-block:: python

    text = cdf.attributes["TEXT"]
    len(text)        # 11 entries
    text[0]          # 'MAG - ACE Magnetic Field Experiment'
    text.type(0)     # DataType.CDF_CHAR: the type of entry 0

    for name, attr in cdf.attributes.items():
        print(name, [attr[i] for i in range(len(attr))])

A long text split over several entries reads best joined back together:

.. code-block:: python

    print("\n".join(text[i] for i in range(len(text))))

Following links between variables
=================================

Files that follow the ISTP conventions link their variables together through
attributes. The value of these attributes is the name of another variable:

- ``DEPEND_0``: the time variable of this variable.
- ``DEPEND_1``, ``DEPEND_2``, …: the coordinates of the other dimensions. For example,
  the energy of each channel of a spectrogram.
- ``LABL_PTR_1``: labels for the components, like ``"Bx GSE"``.

Here is how to gather everything you need to plot ``BGSEc``:

.. code-block:: python

    var = cdf["BGSEc"]
    attrs = var.attributes

    time = pycdfpp.to_datetime64(cdf[attrs["DEPEND_0"].value]).ravel()
    labels = cdf[attrs["LABL_PTR_1"].value].values_encoded[0]
    units = attrs["UNITS"].value

    print(labels, units)     # ['Bx GSE' 'By GSE' 'Bz GSE'] nT

Keeping only part of a file
===========================

:meth:`pycdfpp.CDF.filter` returns a copy of the file with only the variables and global
attributes you choose. You can give a list of names, a regular expression, or a
function:

.. code-block:: python

    small = cdf.filter(variables=["Epoch", "BGSEc"])
    small = cdf.filter(variables="B.*")
    small = cdf.filter(variables=lambda v: v.type == pycdfpp.DataType.CDF_REAL4)
    small = cdf.filter(attributes=["Project", "TITLE"])   # all variables, 2 global attributes

What you don't filter is kept: without ``attributes``, every global attribute stays, and
without ``variables``, every variable stays.

Add ``inplace=True`` to modify ``cdf`` itself instead of making a copy. Then save the
result with :func:`pycdfpp.save` to get a smaller file.

Exporting the structure
=======================

:func:`pycdfpp.to_dict_skeleton` turns a file, a variable or an attribute into a plain
dictionary. It contains the structure and the attributes, but not the variable values.
Use it to document a file format, compare files, or feed another tool:

.. code-block:: python

    import json

    skeleton = pycdfpp.to_dict_skeleton(cdf)
    print(json.dumps(skeleton["variables"]["BGSEc"], indent=2)[:300])

Reading many files
==================

``pycdfpp`` releases Python's global lock (the GIL) while it reads and decompresses.
So you can read many files in parallel with plain threads:

.. code-block:: python
    :class: no-playground

    from concurrent.futures import ThreadPoolExecutor

    def field(path):
        return pycdfpp.load(path, lazy_load=False)["BGSEc"].values

    with ThreadPoolExecutor() as pool:
        fields = list(pool.map(field, ["ac_h0_mfi_20200101_v07.cdf"] * 4))

To join several daily files into one time series, see :doc:`cookbook`.
