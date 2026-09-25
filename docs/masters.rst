===========================
Starting from a master CDF
===========================

Most missions don't build their data files from scratch. They start from a **master
CDF**: a file with every variable and every attribute already defined, but no data.
Filling a master is the easiest way to produce files that always have the same, correct
structure.

With ``pycdfpp``, a master is an ordinary CDF file. The recipe is always the same:

1. Load the master.
2. Fill its variables with your data.
3. Update the global attributes that change from file to file.
4. Save under the data file's name.

What is in a master
===================

NASA publishes the masters of every dataset of its archive in the
`SPDF 0MASTERS folder <https://spdf.gsfc.nasa.gov/pub/software/cdawlib/0MASTERS/>`_.
Instrument teams usually keep their own. Here is the master of the ACE magnetometer
data used throughout this documentation:

.. code-block:: python

    import urllib.request
    import numpy as np
    import pycdfpp

    url = "https://spdf.gsfc.nasa.gov/pub/software/cdawlib/0MASTERS/ac_h0_mfi_00000000_v01.cdf"
    urllib.request.urlretrieve(url, "ac_h0_mfi_00000000_v01.cdf")

    master = pycdfpp.load("ac_h0_mfi_00000000_v01.cdf")
    for name, var in master.items():
        print(f"{name:15} {str(var.shape):12} {var.type.name:12} nrv={var.is_nrv}")

.. code-block:: text

    Epoch           (0, 1)       CDF_EPOCH    nrv=False
    Magnitude       (0, 1)       CDF_REAL4    nrv=False
    BGSEc           (0, 3)       CDF_REAL4    nrv=False
    label_BGSE      (1, 3, 6)    CDF_CHAR     nrv=True
    ...

Two kinds of variables:

- **Variables that change with time** have 0 records. They wait for your data.
- **Non-record-varying variables** (``nrv=True``) are already filled: labels, energy
  tables, and other constants. Leave them as they are.

Every variable already has its attributes: units, fill value, valid range, the
``DEPEND_0`` link to the time variable, and so on. You don't have to set any of them.

Filling the variables
=====================

Use ``set_values`` on each variable to fill. Here, one minute of made-up data, one sample
every 16 seconds:

.. code-block:: python

    time = np.arange("2024-01-01", "2024-01-01T00:01", np.timedelta64(16, "s"),
                     dtype="datetime64[ns]")
    field = np.random.default_rng(0).normal(0.0, 5.0, size=(len(time), 3))

    cdf = pycdfpp.load("ac_h0_mfi_00000000_v01.cdf")
    cdf["Epoch"].set_values(time)
    cdf["BGSEc"].set_values(field.astype(cdf["BGSEc"].values.dtype))
    cdf["Magnitude"].set_values(np.linalg.norm(field, axis=1).astype(np.float32))

A few rules:

- **Time**: pass ``datetime64`` values. They are converted to the master's time type,
  ``CDF_EPOCH`` here.
- **Numbers must have the master's type.** ``BGSEc`` is ``CDF_REAL4`` (``float32``), so
  ``float64`` values are refused. ``var.values.dtype`` gives the right numpy type, even
  for an empty variable: ``data.astype(var.values.dtype)``.
- **Shapes must match the records.** ``BGSEc`` records hold 3 values, so give it an
  ``(N, 3)`` array. Scalars like ``Magnitude`` are declared with records of shape
  ``(1,)`` in this master; a plain 1-D array of N values is fine.
- **Every variable with a** ``DEPEND_0`` needs as many records as its time variable.

.. note::

   You don't need ``force=True`` to fill a master: its variables are empty, so nothing is
   overwritten. Keep ``force=True`` for when you really mean to change a variable's type
   or shape.

Variables you don't fill
------------------------

A master often defines more variables than one file needs. Variables you leave empty are
saved empty, which ISTP tools accept. To leave them out of the file instead, remove them
with :meth:`pycdfpp.CDF.filter`:

.. code-block:: python

    cdf.filter(variables=lambda v: v.is_nrv or len(v) > 0, inplace=True)

Updating the global attributes
==============================

Some global attributes change with each file: its identifier, its version, when it was
made. Set them with ``set_values``:

.. code-block:: python

    from datetime import date

    cdf.attributes["Logical_file_id"].set_values(["ac_h0_mfi_20240101_v01"])
    cdf.attributes["Data_version"].set_values(["1"])
    cdf.attributes["Generation_date"].set_values([date.today().strftime("%Y%m%d")])

Attributes that don't exist in the master can be added with ``add_attribute``, as in
:doc:`writing`.

Saving
======

Save under the data file's name. Never save over the master: you will need it for the
next file.

.. code-block:: python

    pycdfpp.save(cdf, "ac_h0_mfi_20240101_v01.cdf")

    check = pycdfpp.load("ac_h0_mfi_20240101_v01.cdf")
    print(check["BGSEc"].shape, check["BGSEc"].attributes["UNITS"].value)   # (4, 3) nT

The compression of each variable comes from the master too. Here, ``BGSEc`` is saved
with gzip, as the master asks.

Producing many files
====================

Load the master again for each file. A fresh load is fast (only the structure is read)
and guarantees that nothing from the previous file leaks into the next one:

.. code-block:: python

    def produce(day, time, field):
        cdf = pycdfpp.load("ac_h0_mfi_00000000_v01.cdf")
        cdf["Epoch"].set_values(time)
        cdf["BGSEc"].set_values(field.astype(np.float32))
        cdf["Magnitude"].set_values(np.linalg.norm(field, axis=1).astype(np.float32))
        cdf.attributes["Logical_file_id"].set_values([f"ac_h0_mfi_{day}_v01"])
        pycdfpp.save(cdf, f"ac_h0_mfi_{day}_v01.cdf")

    produce("20240101", time, field)

Finally, check the result against the ISTP conventions, as explained in :doc:`istp`.
