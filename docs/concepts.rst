=====================
CDF in plain words
=====================

This page explains how a CDF file is organized. You don't need to know it to read a
file. You do need it to produce good files, and it makes everything else in this
documentation easier to follow.

We use the ACE magnetometer file from the :doc:`quickstart` as the example. To follow
along, load it first:

.. code-block:: python

    import urllib.request
    import pycdfpp

    url = ("https://spdf.gsfc.nasa.gov/pub/data/ace/mag/level_2_cdaweb/"
           "mfi_h0/2020/ac_h0_mfi_20200101_v07.cdf")
    urllib.request.urlretrieve(url, "ac_h0_mfi_20200101_v07.cdf")
    cdf = pycdfpp.load("ac_h0_mfi_20200101_v07.cdf")

The big picture
===============

A CDF file holds two things:

1. **Global attributes.** Information about the whole file: mission, instrument,
   version, who made it, and so on.
2. **Variables.** The data. Each variable has a name, a type, a shape, its values, and
   its own attributes.

.. code-block:: text

    ac_h0_mfi_20200101_v07.cdf
    ├── global attributes
    │   ├── TITLE        "ACE> Magnetometer Parameters"
    │   ├── Project      "ACE>Advanced Composition Explorer", "ISTP>International ..."
    │   └── ...
    └── variables
        ├── Epoch        time of each sample
        ├── BGSEc        magnetic field, 3 components  ── attributes: UNITS="nT", DEPEND_0="Epoch", ...
        ├── label_BGSE   "Bx GSE", "By GSE", "Bz GSE"
        └── ...

In ``pycdfpp``, the file is a :class:`pycdfpp.CDF` object. It works like a dictionary
of variables, and its ``attributes`` property is a dictionary of global attributes.

Records
=======

Most variables are **time series**. Each measurement is stored as one **record**.

``BGSEc`` has 5401 records, one every 16 seconds. Each record holds 3 numbers: the X, Y
and Z components of the field. So its shape is ``(5401, 3)``:

.. code-block:: python

    cdf["BGSEc"].shape     # (5401, 3)
    len(cdf["BGSEc"])      # 5401 records

The first dimension is always the record number. The other dimensions describe one
record. A spectrogram could have shape ``(n_records, 32)`` for 32 energy channels. A
particle distribution could have ``(n_records, 16, 32, 8)``.

Some files store scalar values with an extra dimension of size 1. That's why ACE's
``Epoch`` has shape ``(5401, 1)``. Use ``.ravel()`` or ``[:, 0]`` if you want a flat
array.

Variables that don't change: NRV
================================

Some variables don't depend on time. Examples: the labels of a vector's components, or
the table of energy channels of a particle instrument. Storing them in every record
would waste space.

These are **non-record-varying** variables, or **NRV**. They hold a single record:

.. code-block:: python

    cdf["label_BGSE"].is_nrv    # True
    cdf["label_BGSE"].shape     # (1, 3, 6): 1 record of 3 strings of 6 characters

Variables that change with time are **record-varying** (``is_nrv`` is ``False``).

.. note::

   ``pycdfpp`` always shows the record count as the first dimension, even for NRV
   variables. An NRV variable has shape ``(1, ...)``, or ``(0, ...)`` if it is empty.

Attributes
==========

There are two kinds of attributes, and they behave differently.

**Global attributes** describe the file. Each one holds a list of **entries**, and each
entry can have its own type. Long texts are often split into several entries, one per
line:

.. code-block:: python

    project = cdf.attributes["Project"]
    len(project)     # 2 entries
    project[0]       # 'ACE>Advanced Composition Explorer'
    project[1]       # 'ISTP>International Solar-Terrestrial Physics'

**Variable attributes** describe one variable. Each one holds a single value. That
value can be a string, a number, or an array of numbers:

.. code-block:: python

    attrs = cdf["BGSEc"].attributes
    attrs["UNITS"].value      # 'nT'
    attrs["VALIDMIN"].value   # [-65534.0, -65534.0, -65534.0]

Some variable attributes point to other variables by name. ``DEPEND_0`` names the time
variable. ``LABL_PTR_1`` names the variable holding the component labels. This is how
a CDF file ties its variables together.

Data types
==========

Every variable and every attribute value has a CDF type. Here is how they map to numpy:

.. list-table::
   :header-rows: 1
   :widths: 30 20 50

   * - CDF type
     - numpy dtype
     - Notes
   * - ``CDF_INT1`` / ``CDF_BYTE``
     - ``int8``
     -
   * - ``CDF_INT2``, ``CDF_INT4``, ``CDF_INT8``
     - ``int16``, ``int32``, ``int64``
     -
   * - ``CDF_UINT1``, ``CDF_UINT2``, ``CDF_UINT4``
     - ``uint8``, ``uint16``, ``uint32``
     - There is no unsigned 64-bit type in CDF.
   * - ``CDF_FLOAT`` / ``CDF_REAL4``
     - ``float32``
     - The two names mean the same thing.
   * - ``CDF_DOUBLE`` / ``CDF_REAL8``
     - ``float64``
     - The two names mean the same thing.
   * - ``CDF_CHAR`` / ``CDF_UCHAR``
     - ``bytes`` (``S`` dtype)
     - Fixed-width strings. See below.
   * - ``CDF_TIME_TT2000``
     - ``int64``
     - Time. See below.
   * - ``CDF_EPOCH``
     - ``float64``
     - Time. See below.
   * - ``CDF_EPOCH16``
     - two ``float64``
     - Time. See below.

In ``pycdfpp``, these types are listed in :class:`pycdfpp.DataType`.

Strings
-------

CDF strings have a fixed width. All strings in a variable use the same number of
characters. ``.values`` gives them as bytes. ``.values_encoded`` decodes them to Python
strings:

.. code-block:: python

    cdf["label_BGSE"].values           # [[b'Bx GSE' b'By GSE' b'Bz GSE']]
    cdf["label_BGSE"].values_encoded   # [['Bx GSE' 'By GSE' 'Bz GSE']]

Time
====

CDF has three time types. Which one a file uses depends on who made it, and when.

.. list-table::
   :header-rows: 1
   :widths: 22 48 30

   * - Type
     - What is stored
     - Precision
   * - ``CDF_TIME_TT2000``
     - Nanoseconds since 2000-01-01 12:00 (J2000), **counting leap seconds**.
     - 1 nanosecond
   * - ``CDF_EPOCH``
     - Milliseconds since year 0, as a float. No leap seconds.
     - 1 millisecond
   * - ``CDF_EPOCH16``
     - Two numbers: seconds since year 0, and picoseconds.
     - 1 picosecond

**Use TT2000 for new files.** It is the modern standard, it handles leap seconds, and
it is what ``pycdfpp`` uses by default when you store ``datetime64`` values.

You rarely need to handle these types yourself. ``pycdfpp`` converts all three to
numpy ``datetime64`` in one call. See :doc:`time`.

Fill values and valid ranges
============================

Sometimes an instrument has no valid measurement: it was off, saturated, or the data was
corrupted. The record is still there, so every variable keeps the same length. It holds
a special **fill value** instead of a measurement.

A file can also leave records out entirely, to save space ("sparse records"). ``pycdfpp``
fills those records the way NASA's CDF library does: with the previous record when the
variable asks for it, otherwise with ``FILLVAL``, otherwise with the variable's **pad
value**. So a missing record reads like a fill value, and you can mask both at once.

The ``FILLVAL`` attribute tells you which value that is. ``VALIDMIN`` and ``VALIDMAX``
give the range of physically meaningful values. Always mask values outside it before
you compute anything. The :doc:`cookbook` shows how.

.. code-block:: python

    attrs["FILLVAL"].value    # [-1e+31]

Majority
========

Multi-dimensional arrays can be stored in **row-major** order (C, Python) or
**column-major** order (Fortran, IDL). A file uses one or the other.

You don't need to care. ``pycdfpp`` always gives you normal row-major numpy arrays,
whatever the file uses.

Compression
===========

A CDF file can be compressed as a whole, or variable by variable. ``pycdfpp``
decompresses on the fly. It reads and writes the two codecs used in practice, gzip and
RLE. The Huffman codecs of the standard are not supported, but they are very rarely
used. When writing, you choose. See :doc:`compression`.

Conventions: ISTP
=================

The CDF format itself doesn't say which attributes a file must have, or what they mean.
The space physics community agreed on conventions for that, called **ISTP**. They say,
for example, that each data variable must have ``UNITS``, ``FILLVAL`` and a
``DEPEND_0`` pointing to its time variable.

Files that follow ISTP work out of the box with tools like
`CDAWeb <https://cdaweb.gsfc.nasa.gov/>`_,
`Speasy <https://speasy.readthedocs.io/>`_ and
`SciQLop <https://sciqlop.github.io/>`_. If you produce files, read :doc:`istp`.
