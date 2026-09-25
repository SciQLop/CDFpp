===============================
Producing ISTP-compliant files
===============================

A CDF file can hold any attributes you like. But tools only understand a file if they
know where to find the time, the units, the fill value, and so on. The space physics
community agreed on conventions for this, called **ISTP**.

A file that follows ISTP works out of the box with
`CDAWeb <https://cdaweb.gsfc.nasa.gov/>`_,
`Speasy <https://speasy.readthedocs.io/>`_,
`SciQLop <https://sciqlop.github.io/>`_,
`the CDFpp Explorer <https://sciqlop.github.io/CDFpp/>`_ and many others. If you
distribute data, follow ISTP.

This page gives you a complete, working example, then a checklist. The reference is the
`ISTP metadata guide <https://istp-metadata.readthedocs.io/>`_.

The rules in five points
========================

1. **Global attributes** describe the dataset: mission, instrument, level, version,
   contacts. A fixed list of them is required.
2. **Every variable has a** ``VAR_TYPE``. It is ``data`` for the physical quantities,
   ``support_data`` for time and coordinates, and ``metadata`` for labels.
3. **Every data variable points to its time** with ``DEPEND_0``, and to its other
   coordinates or labels with ``DEPEND_1``, ``LABL_PTR_1``, and so on.
4. **Every data variable describes itself**: ``CATDESC`` (a sentence), ``FIELDNAM`` (a
   short name), ``UNITS``, ``FILLVAL``, ``VALIDMIN``, ``VALIDMAX``, ``DISPLAY_TYPE``,
   ``FORMAT``.
5. **Numbers that describe a variable have its type.** ``FILLVAL``, ``VALIDMIN``,
   ``VALIDMAX``, ``SCALEMIN`` and ``SCALEMAX`` must have the same CDF type as the
   variable.

A complete example
==================

This script writes one day of magnetic field data for a fictional satellite, "MySat".
It passes the ISTP checks of `AstraLint <https://github.com/SciQLop/AstraLint>`_. Copy
it and adapt the names and values to your mission.

The data
--------

.. code-block:: python

    import numpy as np
    import pycdfpp
    from pycdfpp import DataType

    day = np.datetime64("2024-01-01")
    time = np.arange(day, day + np.timedelta64(1, "D"),
                     np.timedelta64(1, "m")).astype("datetime64[ns]")
    field = np.random.default_rng(0).normal(0.0, 5.0, size=(len(time), 3)).astype(np.float32)

    cdf = pycdfpp.CDF()

Global attributes
-----------------

.. code-block:: python

    global_attributes = {
        # Who and what
        "Project": "ISTP>International Solar-Terrestrial Physics",
        "Mission_group": "MySat",
        "Source_name": "MYSAT>My Satellite",
        "Discipline": "Space Physics>Magnetospheric Science",
        "Instrument_type": "Magnetic Fields (space)",
        "Descriptor": "MAG>Magnetometer",
        "Data_type": "L2>Level 2 Data",
        "PI_name": "A. Lovelace",
        "PI_affiliation": "Analytical Engine Lab",
        # Identification and version
        "Logical_source": "mysat_l2_mag",
        "Logical_file_id": "mysat_l2_mag_20240101_v01",
        "Logical_source_description": "MySat Level 2 magnetic field, 1-minute resolution",
        "Data_version": "1",
        "File_naming_convention": "source_datatype_descriptor_yyyyMMdd",
        "MODS": "v01: first release",
        # Description and provenance
        "TEXT": "Magnetic field vector measured by the MAG fluxgate magnetometer.",
        "Time_resolution": "60 s",
        "Generated_by": "MySat ground segment",
        "Generation_date": "20240102",
        # Usage
        "Acknowledgement": "Please acknowledge the MySat MAG team.",
        "Rules_of_use": "Open data, see the MySat data policy.",
        "HTTP_LINK": "https://example.org/mysat",
        "LINK_TEXT": "MySat mission page",
        "LINK_TITLE": "MySat",
    }
    for name, value in global_attributes.items():
        cdf.add_attribute(name, [value])

A few of these follow a pattern:

- ``Source_name``, ``Descriptor`` and ``Data_type`` are written ``SHORT>Long name``.
- ``Logical_source`` is ``<source>_<data type>_<descriptor>``, in lower case.
- ``Logical_file_id`` is the file name without ``.cdf``: the logical source, the date,
  and the version.

The time variable
-----------------

.. code-block:: python

    cdf.add_variable("Epoch", values=time, attributes={
        "VAR_TYPE": "support_data",
        "CATDESC": "Time at the middle of each 1-minute interval",
        "FIELDNAM": "Time",
        "LABLAXIS": "Epoch",
        "UNITS": "ns",
        "FILLVAL": [pycdfpp.default_fill_value(DataType.CDF_TIME_TT2000)],
        "VALIDMIN": np.array(["2020-01-01"], dtype="datetime64[ns]"),
        "VALIDMAX": np.array(["2050-01-01"], dtype="datetime64[ns]"),
        "MONOTON": "INCREASE",
        "SCALETYP": "linear",
        "TIME_BASE": "J2000",
        "TIME_SCALE": "Terrestrial Time",
        "REFERENCE_POSITION": "Rotating Earth Geoid",
        "RESOLUTION": "60s",
        "DISPLAY_TYPE": "time_series",
        "FORMAT": "A30",
    })

The ``datetime64`` values are stored as ``CDF_TIME_TT2000``. So are the ``VALIDMIN`` and
``VALIDMAX`` dates, which keeps their type equal to the variable's.

The data variable
-----------------

.. code-block:: python

    cdf.add_variable("B_GSE", values=field, attributes={
        "VAR_TYPE": "data",
        "CATDESC": "Magnetic field vector in GSE coordinates, 1-minute averages",
        "FIELDNAM": "Magnetic field GSE",
        "UNITS": "nT",
        "DEPEND_0": "Epoch",
        "DEPEND_1": "B_GSE_component",
        "LABL_PTR_1": "B_GSE_label",
        "COORDINATE_SYSTEM": "GSE",
        "DISPLAY_TYPE": "time_series",
        "FILLVAL": [pycdfpp.default_fill_value(DataType.CDF_FLOAT)],
        "VALIDMIN": np.array([-5000.0], dtype=np.float32),
        "VALIDMAX": np.array([5000.0], dtype=np.float32),
        "SCALEMIN": np.array([-50.0], dtype=np.float32),
        "SCALEMAX": np.array([50.0], dtype=np.float32),
        "SCALETYP": "linear",
        "FORMAT": "F10.3",
    })

``field`` is ``float32``, so the variable is ``CDF_FLOAT``. Every numeric attribute is
built as ``float32`` too.

Labels and coordinates
----------------------

``DEPEND_1`` and ``LABL_PTR_1`` point to two small variables. They don't change with
time, so they are non-record-varying (``is_nrv=True``), with a single record:

.. code-block:: python

    cdf.add_variable("B_GSE_component", values=np.array([["x", "y", "z"]]),
                     data_type=DataType.CDF_CHAR, is_nrv=True, attributes={
        "VAR_TYPE": "metadata",
        "CATDESC": "Component names of B_GSE",
        "FIELDNAM": "Component",
        "FORMAT": "A1",
    })

    cdf.add_variable("B_GSE_label", values=np.array([["Bx GSE", "By GSE", "Bz GSE"]]),
                     data_type=DataType.CDF_CHAR, is_nrv=True, attributes={
        "VAR_TYPE": "metadata",
        "CATDESC": "Plot labels for the components of B_GSE",
        "FIELDNAM": "Labels",
        "FORMAT": "A6",
    })

Saving
------

Compress with gzip, and name the file after ``Logical_file_id``:

.. code-block:: python

    cdf.compression = pycdfpp.CompressionType.gzip_compression
    pycdfpp.save(cdf, "mysat_l2_mag_20240101_v01.cdf")

Checking your file
==================

In the browser
--------------

Drop the file in the `CDFpp Explorer <https://sciqlop.github.io/CDFpp/>`_ and click
**Validate**. It opens the file in AstraLint, which lists every problem with a link to
the matching ISTP rule. Nothing is uploaded: everything runs in your browser.

On the command line
-------------------

Install `AstraLint <https://github.com/SciQLop/AstraLint>`_ and run it on your file. It
fits well in a ground-segment pipeline or a CI job:

.. code-block:: console

    $ pip install astralint
    $ astralint lint mysat_l2_mag_20240101_v01.cdf

    mysat_l2_mag_20240101_v01.cdf
      WARNING ISTP-GA-002  missing recommended global attribute(s): DOI,
    spase_DatasetResourceID

    ✗ Found 1 problem (1 warning), plus 4 info findings

The one warning left is expected for this fictional mission. ``DOI`` and
``spase_DatasetResourceID`` are identifiers your dataset gets when you register it with
an archive. Add them once you have them.

``astralint fix`` can correct many issues automatically, and ``--strict`` turns warnings
into failures for your CI.

Checklist
=========

Checklist: global attributes
----------------------------

☐ ``Project``, ``Source_name``, ``Discipline``, ``Data_type``, ``Descriptor``

☐ ``Data_version``, ``Logical_source``, ``Logical_file_id``, ``Logical_source_description``

☐ ``Mission_group``, ``Instrument_type``, ``PI_name``, ``PI_affiliation``, ``TEXT``

☐ Recommended: ``Acknowledgement``, ``Rules_of_use``, ``Generated_by``,
``Generation_date``, ``MODS``, ``HTTP_LINK``, ``LINK_TEXT``, ``LINK_TITLE``, ``DOI``

Checklist: every variable
-------------------------

☐ ``VAR_TYPE`` is ``data``, ``support_data`` or ``metadata``

☐ ``CATDESC`` and ``FIELDNAM``

Checklist: data variables
-------------------------

☐ ``DEPEND_0`` names an existing time variable, with the same number of records

☐ ``UNITS``, ``DISPLAY_TYPE``, ``FORMAT``

☐ ``FILLVAL``, ``VALIDMIN``, ``VALIDMAX``, all with the variable's own type

☐ ``FILLVAL`` is outside the ``VALIDMIN``–``VALIDMAX`` range

☐ Multi-dimensional data: ``DEPEND_1``… or ``LABL_PTR_1``… for each extra dimension

Checklist: time variable
------------------------

☐ ``CDF_TIME_TT2000``, ``VAR_TYPE`` is ``support_data``, ``MONOTON`` is ``INCREASE``

Checklist: the file
-------------------

☐ Named ``<logical source>_<date>_v<version>.cdf``

☐ Compressed with gzip, not an experimental codec (see :doc:`compression`)

☐ Read back once, and checked with AstraLint
