===========================
FAQ and troubleshooting
===========================

Find your problem by its symptom. If it's not here, please
`open an issue <https://github.com/SciQLop/CDFpp/issues>`_.

Some answers use the ACE file from the :doc:`quickstart`:

.. code-block:: python

    import urllib.request
    import pycdfpp

    url = ("https://spdf.gsfc.nasa.gov/pub/data/ace/mag/level_2_cdaweb/"
           "mfi_h0/2020/ac_h0_mfi_20200101_v07.cdf")
    urllib.request.urlretrieve(url, "ac_h0_mfi_20200101_v07.cdf")

Reading
=======

``ValueError: '...' is not a valid CDF file``
---------------------------------------------

The file is not a CDF file, or it is damaged: a partial download, for example. Check
that it starts with the CDF magic bytes:

.. code-block:: python

    with open("ac_h0_mfi_20200101_v07.cdf", "rb") as f:
        print(f.read(4).hex())    # 'cdf30001' for CDF 3.x files

Why does my scalar variable have shape ``(N, 1)``?
--------------------------------------------------

The file declares the variable with one dimension of size 1. ``pycdfpp`` shows the shape
exactly as stored. Use ``.values.ravel()`` or ``.values[:, 0]`` to get a flat array.

Why is a non-record-varying variable ``(1, ...)``?
--------------------------------------------------

``pycdfpp`` always shows the number of records as the first dimension. An NRV variable
has one record, so its shape starts with 1. It is ``(0, ...)`` if the variable is empty.
Use ``var.values[0]`` to get the single record.

Why are strings ``bytes``?
--------------------------

CDF strings are fixed-width byte arrays. ``.values`` gives them as they are stored, which
is fast for large arrays. Use ``.values_encoded`` for Python ``str``.

My averages are huge negative numbers
-------------------------------------

The data contains fill values, usually ``-1e31``. Replace them with ``NaN`` first. See
:ref:`cookbook:Replace fill values with NaN`.

My times are shifted by a few hours
-----------------------------------

``pycdfpp`` takes a :class:`datetime.datetime` without a timezone as UTC. If your
datetimes hold local times, give them a timezone (``dt.astimezone()``), and ``pycdfpp``
converts them to UTC. See :doc:`time`.

Writing
=======

Python crashes, or my file is empty, after saving over it
----------------------------------------------------------

This happened in pycdfpp 0.12.0 and earlier, when a file loaded lazily (the default) was
saved over itself. Update ``pycdfpp``. With an old version, load with
``lazy_load=False`` before saving over the same file.

Python crashes after adding or removing variables
-------------------------------------------------

``cdf["name"]`` and ``var.attributes["name"]`` return references into the file's
internal storage. Adding or removing variables (or attributes) can move that storage.
Old references then point to freed memory. Fetch them again after any change:

.. code-block:: python

    import numpy as np
    import pycdfpp

    cdf = pycdfpp.CDF()
    cdf.add_variable("var1", values=np.ones(10))
    var1 = cdf["var1"]

    cdf.add_variable("var2", values=np.zeros(5))   # may move var1 in memory
    var1 = cdf["var1"]                             # fetch it again: safe
    print(var1.values)

My attribute has the wrong type
-------------------------------

Plain Python numbers carry no type, so ``pycdfpp`` has to guess. A list of floats
becomes ``CDF_DOUBLE``, and a list of small integers becomes the smallest integer type
that fits. Use numpy values with an explicit dtype, or pass the type:

.. code-block:: python

    cdf["var1"].add_attribute("VALIDMIN", np.array([0], dtype=np.int16))
    cdf["var1"].add_attribute("VALIDMAX", [1000], pycdfpp.DataType.CDF_INT2)

To pick an integer type for a global attribute:

.. code-block:: python

    cdf.add_attribute("int8 attribute", np.array([[1, 2, 3]], dtype=np.int8))
    cdf.add_attribute("int32 attribute", [[np.int32(1)]])

``DeprecationWarning: Overriding existing variable values without force=True``
-------------------------------------------------------------------------------

Replacing the values of a variable needs ``force=True``:
``var.set_values(new_values, force=True)``. Without it, this will become an error in a
future version.

``ValueError: Variable '...' already exists``
---------------------------------------------

Variable names are unique. To replace a variable's values, use ``set_values`` on the
existing one. To start over, remove it with :meth:`pycdfpp.CDF.filter` first.

Other tools can't open my file
------------------------------

You probably saved with ``zstd_compression`` or ``blosc2_compression``. They are not
part of the CDF standard, and only CDFpp can read them. ``pycdfpp`` warns you with an
``ExperimentalCompressionWarning`` when you do. Use ``gzip_compression``. See
:doc:`compression`.

``Unsupported compression algorithm``
-------------------------------------

CDFpp doesn't support the Huffman codecs (``huff_compression``, ``ahuff_compression``).
Use ``gzip_compression``.

How do I make special values (fill, pad)?
-----------------------------------------

:func:`pycdfpp.default_fill_value` and :func:`pycdfpp.default_pad_value` return the
standard value for any CDF type, with the right numpy dtype:

.. code-block:: python

    pycdfpp.default_fill_value(pycdfpp.DataType.CDF_INT1)          # np.int8(-128)
    pycdfpp.default_fill_value(pycdfpp.DataType.CDF_TIME_TT2000)   # 9999-12-31T23:59:59.999999999

Other questions
===============

Is it safe to use from several threads?
---------------------------------------

Yes. CDFpp has no global state, and ``pycdfpp`` releases the GIL while reading and
decompressing. Don't modify the same :class:`pycdfpp.CDF` object from several threads
at once, though.

How is it different from NASA's library, cdflib or spacepy?
-----------------------------------------------------------

NASA's C library is the reference. CDFpp is a separate implementation, written from
scratch. It is thread-safe, installs with ``pip`` without any compiled dependency, and
is MIT-licensed. ``cdflib`` is written in pure Python; CDFpp is usually much faster.
``spacepy.pycdf`` wraps NASA's library, which you must install separately.

Where do I report a bug?
------------------------

On `GitHub <https://github.com/SciQLop/CDFpp/issues>`_. Please attach the file, or a
link to it, and the smallest code that shows the problem.
