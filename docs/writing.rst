=============
Writing files
=============

This guide shows how to build a CDF file from scratch, and how to edit an existing one.
It is written for people who produce data: instrument teams, ground segments, archives.

If your mission provides a master CDF, start from it instead: see :doc:`masters`.

If you have not read it yet, :doc:`concepts` explains records, attributes and types.
Once you know how to write a file, :doc:`istp` shows how to make it follow the ISTP
conventions, so every tool can read it.

The recipe
==========

Building a file always follows the same steps:

1. Create an empty :class:`pycdfpp.CDF`.
2. Add the global attributes.
3. Add the time variable.
4. Add the data variables, with their attributes.
5. Add the support variables: labels, energy tables, and so on.
6. Save.

Here is a complete example. The rest of the page explains each step.

.. code-block:: python

    import numpy as np
    import pycdfpp
    from pycdfpp import DataType

    cdf = pycdfpp.CDF()

    cdf.add_attribute("Project", ["My mission"])
    cdf.add_attribute("PI_name", ["Ada Lovelace"])

    time = np.arange("2024-01-01", "2024-01-02", np.timedelta64(1, "m"),
                     dtype="datetime64[ns]")
    cdf.add_variable("Epoch", values=time)

    field = np.random.default_rng(0).normal(size=(len(time), 3)).astype(np.float32)
    cdf.add_variable("B", values=field, attributes={
        "UNITS": "nT",
        "DEPEND_0": "Epoch",
        "LABL_PTR_1": "B_labels",
        "FILLVAL": np.float32(-1e31),
    })

    cdf.add_variable("B_labels", values=np.array([["Bx", "By", "Bz"]]),
                     data_type=DataType.CDF_CHAR, is_nrv=True)

    pycdfpp.save(cdf, "my_mission.cdf")

Creating a file
===============

``pycdfpp.CDF()`` creates an empty file in memory. Nothing is written to disk until you
call :func:`pycdfpp.save`, so you can build the whole file in any order.

The sections below keep adding to the ``cdf`` of the example above.

Global attributes
=================

Add a global attribute with :meth:`pycdfpp.CDF.add_attribute`. Give it a name and a
**list of entries**. Most attributes have a single entry:

.. code-block:: python

    cdf.add_attribute("Mission_group", ["My mission"])

Long texts are usually split in several entries, one per line:

.. code-block:: python

    cdf.add_attribute("TEXT", [
        "Magnetic field measured by the MAG instrument.",
        "Calibrated in the spacecraft frame.",
    ])

Each entry can hold a string, numbers, or dates, and each gets its own type:

.. code-block:: python

    from datetime import datetime

    cdf.add_attribute("Calibration", [
        "version 3",                                      # CDF_CHAR
        np.array([1.0, 0.98, 1.02], dtype=np.float32),    # CDF_FLOAT
        [datetime(2024, 1, 1)],                           # CDF_TIME_TT2000
    ])

To force a type, pass one type per entry in ``entries_types``:

.. code-block:: python

    cdf.add_attribute("Orbit", [[1234]], entries_types=[DataType.CDF_INT4])

To change an attribute later, use ``set_values``. It replaces all the entries:

.. code-block:: python

    cdf.attributes["PI_name"].set_values(["Grace Hopper"])

Variables
=========

Add a variable with :meth:`pycdfpp.CDF.add_variable`. It returns the new variable.

.. code-block:: python

    temperature = cdf.add_variable("Temperature", values=np.full(len(time), 21.5))

The shape of ``values`` is ``(number of records, ...)``: one row per record. A scalar
per record gives a 1-D array. A 3-component vector per record gives ``(n, 3)``.

Adding a variable that already exists raises a ``ValueError``.

Choosing the type
-----------------

The type of the variable comes from the numpy dtype of ``values``:

.. list-table::
   :header-rows: 1

   * - numpy dtype
     - CDF type
   * - ``float64`` / ``float32``
     - ``CDF_DOUBLE`` / ``CDF_FLOAT``
   * - ``int8``, ``int16``, ``int32``, ``int64``
     - ``CDF_INT1``, ``CDF_INT2``, ``CDF_INT4``, ``CDF_INT8``
   * - ``uint8``, ``uint16``, ``uint32``
     - ``CDF_UINT1``, ``CDF_UINT2``, ``CDF_UINT4``
   * - ``datetime64``
     - ``CDF_TIME_TT2000``
   * - strings (``str`` or ``bytes``)
     - ``CDF_UCHAR``

To get another type, pass ``data_type``:

.. code-block:: python

    cdf.add_variable("Epoch_ms", values=time, data_type=DataType.CDF_EPOCH)

.. warning::

   **Use numpy arrays, not Python lists.** With a list of integers, ``pycdfpp`` picks
   the smallest type that can hold the values. ``[1, 2, 3]`` becomes ``CDF_UINT1``,
   which is probably not what you want. A numpy array keeps its exact dtype.

Time
----

Store time as numpy ``datetime64``. It becomes ``CDF_TIME_TT2000``, the recommended
time type:

.. code-block:: python

    cdf["Epoch"].type        # DataType.CDF_TIME_TT2000

See :doc:`time` for the other time types.

Strings
-------

Strings are stored with a fixed width: the longest string sets the width. ISTP tools
expect the ``CDF_CHAR`` type, so ask for it explicitly:

.. code-block:: python

    cdf.add_variable("Instrument_mode", values=np.array(["normal", "burst", "normal"]),
                     data_type=DataType.CDF_CHAR)

Variables that don't change with time
-------------------------------------

Labels, energy tables and other constants are stored once, as **non-record-varying**
(NRV) variables. Pass ``is_nrv=True``. The values you give are the variable's single
record:

.. code-block:: python

    cdf.add_variable("Energy", values=np.array([10.0, 100.0, 1000.0], dtype=np.float32),
                     is_nrv=True)
    cdf["Energy"].shape      # (1, 3): one record of 3 values

    cdf.add_variable("B_labels2", values=["Bx", "By", "Bz"], data_type=DataType.CDF_CHAR,
                     is_nrv=True)
    cdf["B_labels2"].shape   # (1, 3, 2): one record of 3 labels of 2 characters

Variable attributes
===================

Variable attributes hold one value each. Pass them as a dictionary when you create the
variable, or add them later with ``add_attribute``:

.. code-block:: python

    temperature = cdf["Temperature"]
    temperature.add_attribute("UNITS", "degC")
    temperature.add_attribute("DEPEND_0", "Epoch")

Read and change them through ``attributes``:

.. code-block:: python

    cdf["Temperature"].attributes["UNITS"].set_value("K")

Getting the types right
-----------------------

Some attributes **must have the same type as their variable**: ``FILLVAL``,
``VALIDMIN``, ``VALIDMAX``, ``SCALEMIN``, ``SCALEMAX``. Plain Python numbers don't carry
a type, so ``pycdfpp`` has to guess, and it often guesses wrong:

.. code-block:: python

    cdf["B"].add_attribute("SCALEMIN", [-100.0])
    cdf["B"].attributes["SCALEMIN"].type()     # CDF_DOUBLE, but B is CDF_FLOAT!

Use numpy values with the right dtype instead, or pass the type explicitly:

.. code-block:: python

    cdf["B"].add_attribute("VALIDMIN", np.array([-1000, -1000, -1000], dtype=np.float32))
    cdf["B"].add_attribute("SCALEMAX", [100.0], DataType.CDF_FLOAT)

Fill values
-----------

:func:`pycdfpp.default_fill_value` returns the standard ISTP fill value for a type,
already with the right numpy dtype:

.. code-block:: python

    pycdfpp.default_fill_value(DataType.CDF_FLOAT)         # np.float32(-1e+31)
    pycdfpp.default_fill_value(DataType.CDF_INT2)          # np.int16(-32768)
    pycdfpp.default_fill_value(DataType.CDF_TIME_TT2000)   # 9999-12-31T23:59:59.999999999

    cdf["Epoch"].add_attribute("FILLVAL", pycdfpp.default_fill_value(DataType.CDF_TIME_TT2000))

:func:`pycdfpp.default_pad_value` does the same for pad values.

Changing a variable's values
============================

Use ``set_values`` with ``force=True``:

.. code-block:: python

    cdf["Temperature"].set_values(np.full(len(time), 22.0), force=True)

``force=True`` also lets you change the shape or the type of the variable. Without it,
``pycdfpp`` warns you: replacing values without ``force`` will become an error in a
future version.

Copying from another file
=========================

Pass an existing variable or attribute to ``add_variable`` or ``add_attribute``. It is
copied with all its values and attributes:

.. code-block:: python

    other = pycdfpp.CDF()
    other.add_variable(cdf["Epoch"])
    other.add_variable(cdf["B"])
    other.add_attribute(cdf.attributes["Project"])

Removing variables
==================

Use :meth:`pycdfpp.CDF.filter` with ``inplace=True``. It keeps what matches, and removes
everything else:

.. code-block:: python

    cdf.filter(variables=lambda v: v.name != "Instrument_mode", inplace=True)
    "Instrument_mode" in cdf      # False

Editing an existing file
========================

Load it, change it, save it:

.. code-block:: python

    cdf = pycdfpp.load("my_mission.cdf")
    cdf.attributes["PI_name"].set_values(["Grace Hopper"])
    cdf["B"].attributes["UNITS"].set_value("nanoTesla")
    pycdfpp.save(cdf, "my_mission.cdf")

Saving over the file you loaded is safe: ``pycdfpp`` reads every value before it
overwrites the file.

Saving
======

To a file
---------

.. code-block:: python

    pycdfpp.save(cdf, "my_mission.cdf")      # raises OSError if the file can't be written

To memory
---------

Leave out the file name to get the file content in memory. Use ``bytes()`` to turn it
into a ``bytes`` object, for example to upload it:

.. code-block:: python

    content = bytes(pycdfpp.save(cdf))
    content[:4]       # b'\xcd\xf3\x00\x01': the CDF magic number

Compressed
----------

Set ``compression`` on the file or on each variable before saving. :doc:`compression`
explains the options:

.. code-block:: python

    cdf.compression = pycdfpp.CompressionType.gzip_compression
    pycdfpp.save(cdf, "my_mission_compressed.cdf")

Checking the result
===================

Always read your file back once:

.. code-block:: python

    check = pycdfpp.load("my_mission.cdf")
    print(check)

Then check it against the ISTP conventions. The easiest way is to drop it in the
`CDFpp Explorer <https://sciqlop.github.io/CDFpp/>`_ and click **Validate**. See
:doc:`istp`.

One pitfall: stale references
=============================

``cdf["B"]`` gives you a lightweight reference into the file's internal storage. Adding
or removing variables can move that storage, and old references then point to freed
memory. Using them can crash Python.

.. code-block:: python

    b = cdf["B"]
    cdf.add_variable("New", values=np.zeros(3))
    b = cdf["B"]          # fetch it again after adding or removing variables
    print(b.shape)

The same holds for attributes: fetch ``var.attributes["NAME"]`` again after adding or
removing attributes on that variable. Values you already copied out, like
``cdf["B"].values`` or ``attrs["UNITS"].value``, are safe.
