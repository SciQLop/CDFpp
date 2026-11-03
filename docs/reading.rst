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

From a remote store (S3...)
---------------------------

:func:`pycdfpp.load` can read a file where it lives, on S3 or any HTTP server, without
downloading it. Only the bytes it needs are fetched, each at most once. This matters on
S3, where you pay for every request and every byte.

Give it a URL. pycdfpp reads it through `fsspec <https://filesystem-spec.readthedocs.io>`_,
so any fsspec URL works: ``s3://``, ``gs://``, ``az://``, ``https://``... Install fsspec and
the package for your store: ``pip install pycdfpp[s3]`` for S3.

For ``s3://`` URLs, pycdfpp uses the AWS credentials found on the machine (environment
variables, ``~/.aws``, or the instance role), as any AWS tool does. When there are none, it
reads anonymously, so public buckets need no setup. ``storage_options`` goes to fsspec as
is, and replaces this default: ``storage_options={"profile": "work"}`` picks an AWS
profile, for example.

.. code-block:: python
    :class: no-check

    url = ("s3://gov-nasa-hdrl-data1/spdf/cdaweb/data/ace/mag/level_2_cdaweb/"
           "mfi_h0/2020/ac_h0_mfi_20200101_v07.cdf")
    cdf = pycdfpp.load(url)
    cdf["BGSEc"].values   # fetches only this variable

Other sources
~~~~~~~~~~~~~

For a source fsspec doesn't know, give :func:`pycdfpp.load` a *reader*: any object with
two methods.

- ``size()`` returns the size of the file in bytes.
- ``read_ranges(ranges)`` gets a list of ``(offset, size)`` pairs and returns one
  ``bytes`` object per pair, each exactly ``size`` long. Fetch them in parallel if you
  can: each step of a load asks for everything that step needs in one call. A load still
  makes several calls, one per step (opening, each index record, the data), and the
  ``read_ahead`` setting below lets one call cover several steps.

As a template, here is a reader for public HTTP(S) objects, written with the standard
library only. It reads the same ACE file:

.. code-block:: python

    from concurrent.futures import ThreadPoolExecutor


    class HttpReader:
        """Reads byte ranges of a public HTTP(S) object, such as a file in a public S3 bucket."""

        def __init__(self, url):
            self.url = url

        def size(self):
            request = urllib.request.Request(self.url, method="HEAD")
            with urllib.request.urlopen(request) as response:
                return int(response.headers["Content-Length"])

        def _get(self, byte_range):
            offset, size = byte_range
            headers = {"Range": f"bytes={offset}-{offset + size - 1}"}
            request = urllib.request.Request(self.url, headers=headers)
            with urllib.request.urlopen(request) as response:
                return response.read()

        def read_ranges(self, ranges):
            with ThreadPoolExecutor(max_workers=8) as pool:
                return list(pool.map(self._get, ranges))


    s3_url = ("https://gov-nasa-hdrl-data1.s3.amazonaws.com/spdf/cdaweb/data/"
              "ace/mag/level_2_cdaweb/mfi_h0/2020/ac_h0_mfi_20200101_v07.cdf")
    cdf = pycdfpp.load(HttpReader(s3_url))
    cdf["BGSEc"].values

With obstore, ``read_ranges`` is a call to ``obstore.get_ranges``.

The returned CDF keeps the reader alive, and uses it again each time you load a variable.
If the reader raises, the error reaches you from :func:`pycdfpp.load` or from ``.values``,
and nothing is half-loaded: the same call works again once the reader recovers.

How much is fetched is tunable. Every setting has a keyword and an environment variable;
the keyword wins over the variable, which wins over the default. Environment variables
let a platform such as a JupyterHub preset what suits its storage.

.. list-table::
   :header-rows: 1

   * - Keyword
     - Environment variable
     - Default
     - Meaning
   * - ``read_ahead``
     - ``CDFPP_REMOTE_READ_AHEAD``
     - 1 MiB
     - Each fetch is widened to the aligned windows of this size it touches, so a small
       read costs one window, or two if it straddles a boundary. Bigger means fewer
       requests but more bytes.
   * - ``tail_prefetch``
     - ``CDFPP_REMOTE_TAIL_PREFETCH``
     - 256 KiB
     - The first fetch also gets the end of the file, where CDF writers often put
       metadata.
   * - ``backing``
     - ``CDFPP_REMOTE_BACKING``
     - ``memory``
     - Where fetched bytes are kept: ``memory``, or ``file``, a deleted temporary file
       the system can move out of RAM.
   * - ``temp_dir``
     - ``CDFPP_REMOTE_TEMP_DIR``
     - your cache directory
     - Where the ``file`` backing puts its temporary file. Use a real disk: on a RAM
       filesystem such as ``/tmp`` on many systems, a file is just more RAM.
   * - ``download_whole_below``
     - ``CDFPP_REMOTE_DOWNLOAD_WHOLE_BELOW``
     - 0 (off)
     - Files smaller than this many bytes are fetched whole in one request.

Why 1 MiB for ``read_ahead``: a CDF file indexes each variable's data with a chain of small
records, and following the chain costs one round trip per link. A wide window brings in
several links at once. On NASA's HelioCloud, for a 53.6 MB MMS magnetometer file:

.. list-table::
   :header-rows: 1

   * - ``read_ahead``
     - Open
     - One variable
     - All variables
   * - 256 KiB
     - 1.2 s, 0.95 MB
     - 1.0 s, 7.2 MB
     - 17 to 20 s, 142 round trips
   * - 1 MiB
     - 0.7 s, 2.3 MB
     - 1.2 s, 8.6 MB
     - 10.5 s, 30 round trips

Downloading the whole file took 4.2 s. So reading a file where it lives pays off when you
need a few variables, not all of them. Lower ``read_ahead`` if you pay mostly for bytes and
read little from each file; raise it if you pay mostly for requests.

``download_whole_below`` is off by default on purpose. When you scan many small files
and read only a little from each, fetching them whole costs far more than reading
just the parts you need.

These numbers come from ``benchmarks/remote/heliocloud.py``, which you can run yourself on
HelioCloud or on any machine, against any URL.

The knobs are keywords of :func:`pycdfpp.load`. Here is the reader above on a small file: a
quarter of a megabyte is under the threshold, so this fetches it whole, and keeps the bytes
in a temporary file on disk instead of in memory.

.. code-block:: python

    tuned = pycdfpp.load(HttpReader(s3_url), read_ahead=4 << 20, backing="file",
                         download_whole_below=8 << 20)
    tuned["BGSEc"].values.shape   # (5401, 3): two requests, size() then the file

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
