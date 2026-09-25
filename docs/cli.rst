==================
Command-line tools
==================

``pycdfpp`` comes with two command-line tools. They show how a CDF file is laid out
**on disk**, record by record. That's useful to debug a file writer, to understand why
a file is big, or to compare with what NASA's tools report.

To just look at a file's variables and attributes, use ``print(pycdfpp.load(path))`` or
the :doc:`explorer` instead.

Install them with the ``cli`` extra:

.. code-block:: console

    $ python -m pip install "pycdfpp[cli]"

cdfdump
=======

``cdfdump`` prints every internal record of the file, in the order they are stored,
with all their fields:

.. code-block:: console

    $ cdfdump ac_h0_mfi_20200101_v07.cdf
    ac_h0_mfi_20200101_v07.cdf
    ├── @8 CDR
    │   ├── GDRoffset: 320
    │   ├── Version: 3
    │   ├── Release: 7
    │   ├── Encoding: "network"
    │   ...
    ├── @320 GDR
    │   ├── rVDRhead: 33674
    ...

Each node is one record: ``@8`` is its byte offset in the file, and ``CDR`` its type.
The CDF specification describes every record type.

Add ``--irsdump`` to get the text format of NASA's ``cdfirsdump`` tool instead.

cdfirsdump
==========

``cdfirsdump`` reproduces the output of NASA's ``cdfirsdump`` tool, byte for byte. So
you can diff its output against the real tool's, or feed it to scripts that expect that
format.

By default, it prints a summary of what takes space in the file:

.. code-block:: console

    $ cdfirsdump ac_h0_mfi_20200101_v07.cdf
    ...
    Summary...

      Total bytes: 253980 (+16 if with checksum)
       Used bytes: 253980, 100.000%
     Unused bytes:      0,   0.000%

         IR count:    349
        ...
        VVR count:      9,  43550 bytes, 17.147%
       CVVR count:      7, 168326 bytes, 66.275%

Here, compressed variable records (``CVVR``) take two thirds of the file.

The main options:

.. list-table::
   :header-rows: 1

   * - Option
     - Effect
   * - ``--level full``
     - Dump every record, not only the summary.
   * - ``--data``
     - With ``--level full``, also hex-dump the variable data.
   * - ``--offset N``
     - Start at byte ``N``.
   * - ``--radix 16``
     - Print offsets in hexadecimal.
   * - ``--output FILE``
     - Write to a file instead of the terminal.
   * - ``--no-summary``
     - Leave out the summary table.

Unlike NASA's tool, options use two dashes: ``--level full``, not ``-full``. Run
``cdfirsdump --help`` for the complete list.

From Python
===========

Both tools are built on the :mod:`pycdfpp.debug` module. Use it directly to inspect
records from a script. Here, we list the compression records of the ACE file from the
:doc:`quickstart`:

.. code-block:: python

    import urllib.request
    import pycdfpp

    url = ("https://spdf.gsfc.nasa.gov/pub/data/ace/mag/level_2_cdaweb/"
           "mfi_h0/2020/ac_h0_mfi_20200101_v07.cdf")
    urllib.request.urlretrieve(url, "ac_h0_mfi_20200101_v07.cdf")

    from pycdfpp.debug import for_each_record

    for offset, record_type, fields in for_each_record("ac_h0_mfi_20200101_v07.cdf"):
        if record_type == "CPR":
            print(offset, fields)
