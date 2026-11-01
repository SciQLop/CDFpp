====================
cdflib compatibility
====================

``pycdfpp.cdflib`` offers the API of `cdflib <https://github.com/MAVENSDC/cdflib>`_ 1.3.16
on top of ``pycdfpp``. Code written for cdflib runs on it unchanged, and reads and writes
faster. It is pure Python: pycdfpp does the work.

Install
=======

cdflib itself is needed: ``pycdfpp.cdflib`` reuses its leap second table and its data classes.

.. code-block:: console

    $ pip install pycdfpp[cdflib]

The extra pulls ``cdflib>=1.3.16,<2``. For ``cdf_to_xarray`` and ``xarray_to_cdf``, install
``xarray`` too.

Switch your code
================

Change the import, nothing else:

.. code-block:: python

    # import cdflib
    import pycdfpp.cdflib as cdflib

    cdf = cdflib.CDF("file.cdf")
    values = cdf.varget("Epoch")
    times = cdflib.cdfepoch.to_datetime(values)

The submodules work the same way: ``pycdfpp.cdflib.cdfwrite``, ``pycdfpp.cdflib.epochs``,
``pycdfpp.cdflib.xarray``, ``pycdfpp.cdflib.s3``, ``pycdfpp.cdflib.utils`` and
``pycdfpp.cdflib.logging``. ``pycdfpp.cdflib.dataclasses`` and
``pycdfpp.cdflib.epochs_astropy`` are cdflib's own modules, so the objects you get are
cdflib's.

Another library that imports cdflib (SunPy, for instance) keeps using the real one. To switch it
too, call ``install()`` before that library is imported:

.. code-block:: python

    import pycdfpp.cdflib
    pycdfpp.cdflib.install()

    import cdflib   # now pycdfpp.cdflib
    import sunpy.io._cdf

``install()`` puts ``pycdfpp.cdflib`` and its submodules in ``sys.modules`` under the ``cdflib``
names. ``cdflib.dataclasses`` and ``cdflib.epochs_astropy`` stay cdflib's. Calling it twice does
nothing. A module that imported cdflib before ``install()`` keeps the real one.

Speed
=====

Time for cdflib and for ``pycdfpp.cdflib`` to do the same work, through the same code. Best of 5
interleaved runs, files in the page cache.

.. list-table::
   :header-rows: 1

   * - Task
     - Data
     - cdflib 1.3.16
     - pycdfpp.cdflib
     - Speedup
   * - Open, read one variable
     - MMS FGM survey, 1.2 M points, gzip
     - 67.8 ms
     - 8.6 ms
     - 7.9×
   * - Open, read every variable
     - MMS FPI electron distribution, 178 MB, gzip
     - 858 ms
     - 161 ms
     - 5.3×
   * - ``CDFepoch.to_datetime``
     - 1,000,000 ``CDF_TIME_TT2000`` values
     - 234 ms
     - 2.4 ms
     - 98×
   * - ``CDFepoch.to_datetime``
     - 1,000,000 ``CDF_EPOCH`` values
     - 14.2 s
     - 2.7 ms
     - 5173×
   * - ``cdfwrite``: write a file
     - time and B, 1,191,296 records, gzip
     - 511 ms
     - 45.7 ms
     - 11.2×
   * - ``cdf_to_xarray``
     - MMS FGM survey, 1.2 M points, gzip
     - 759 ms
     - 80.2 ms
     - 9.5×

AMD Ryzen 7 5800X (8 cores, x86-64 with AVX2, no AVX-512), Python 3.14, 2026-11-01. The machine
was shared with other builds: the 1 minute load average was 5.9 when the run started and 5.7
when it ended. pycdfpp uses several threads for compressed variables (decompression on read, compression
on write) while cdflib uses one, so part of the gap on those rows is multithreading. The
minimum of the runs is kept. cdflib was never
faster. The speedup on ``CDF_EPOCH`` is so large because cdflib converts each value in a Python
loop.

Run it yourself with ``python benchmarks/python_libs/cdflib_compat.py``.

Differences from cdflib
=======================

Where cdflib and NASA's CDF library disagree, ``pycdfpp.cdflib`` follows NASA. Where cdflib
fails, ``pycdfpp.cdflib`` returns the right value.

Time conversions (``cdfepoch``)
-------------------------------

- ``to_datetime`` of ``CDF_EPOCH`` values keeps the fraction of a millisecond the value holds.
  cdflib cuts it off.
- ``to_datetime`` of ``CDF_EPOCH`` fill values (``-1e31``) gives ``NaT``, as it does for the
  other time types. cdflib raises ``OverflowError`` for the whole array. Real dates
  ``datetime64[ns]`` can't hold (before 1678 or after 2262) still raise ``OverflowError``, as in
  cdflib.
- The first leap second, 1972-06-30T23:59:60, breaks down as second 60, like every other leap
  second. cdflib shows it as 23:59:59, the same as the second before it. ``encode``,
  ``to_datetime`` and ``unixtime`` follow.
- ``compute_epoch`` of the fill date (9999-12-31 23:59:59.999) gives the fill value ``-1e31``,
  as NASA's ``computeEPOCH`` does. For a single value cdflib returns the date's millisecond count,
  ``315569519999999.0``, not the fill value. For a list it adds an extra ``-1e31`` element.
  Out of range components (hour 25, ...) give one value each;
  cdflib adds extra elements.
- ``encode_tt2000`` of a list holding a fill or pad value encodes every value. cdflib returns
  only the fill or pad string for the whole list.
- ``timestamp_to_tt2000`` of a time from the fill date on (``unixtime`` of the TT2000 fill value is
  ``253402300800.0``) gives the TT2000 fill value. cdflib raises ``AttributeError``.

Reading files (``CDF``)
-----------------------

- Records a sparse variable doesn't store read as NASA's library reads them. For pad sparse
  records, a missing record holds ``FILLVAL`` when the variable has one, else the pad value, in
  every value of the record. For previous sparse records, a missing record repeats the last stored
  record before it; only the records before the first stored one get ``FILLVAL``, else the pad
  value. cdflib uses the pad value even when there is a ``FILLVAL``, and puts zeros between pad
  values when a record holds several values.
- File offsets have no meaning here: ``next_vdr_location``, ``head_vxr``, ``last_vxr`` (``VDR``),
  ``next_adr_loc``, ``first_gr_entry`` and ``first_z_entry`` (``ADRInfo``) are ``0``.
- ``cdf_info().CDF`` and ``file`` give the file you opened, compressed or not. For a compressed
  file, cdflib gives the path of a temporary uncompressed copy.
- ``attinq(number)`` accepts every attribute number, ``0`` to the number of attributes minus one,
  and raises ``KeyError`` beyond. cdflib checks the number against the count of zVariables:
  it refuses valid numbers in files with fewer zVariables than attributes, and reads garbage past
  the last attribute.
- Attribute entries and pad values of ``ARM_BIG`` encoded files are read big-endian. cdflib reads
  them little-endian. Files in the non IEEE 754 encodings (VAX, Alpha and IA64 VMS D and G floats)
  raise ``OSError``, as cdflib does for the first three.
- ``varget`` with a ``startrec`` past the last record raises ``ValueError``; cdflib raises or reads
  garbage depending on the file. With ``epoch=`` naming a variable that isn't a time variable,
  ``varget`` raises ``ValueError``; cdflib raises ``UnboundLocalError``.
- An rVariable whose first dimension doesn't vary (``rDim_sizes=[2, 3]``, ``Dim_Vary=[False,
  True]``) reads with its varying dimensions, ``(records, 3)``. cdflib reads ``(records,)``, the
  first value of each record, also from the files its own writer makes.

Writing files (``cdfwrite.CDF``)
--------------------------------

- The file is built in memory and written by ``close()``, or at the end of a ``with`` block. A
  ``with`` block left by an exception writes nothing, and a writer never closed writes nothing.
  cdflib writes as it goes and leaves a partial file. With ``delete=True``, the old file is
  replaced when the new one is written; cdflib deletes it right away.
- rVariables are written as zVariables holding their varying dimensions only: the values are the
  same, but ``varinq`` on the new file says ``zVariable`` and ``Dim_Sizes`` lists the varying
  dimensions. ``rvars`` and integer variable ids in ``write_variableattrs`` work as in cdflib.
- ``datetime64`` values for a ``CDF_TIME_TT2000``, ``CDF_EPOCH`` or ``CDF_EPOCH16`` variable or
  attribute store the right instants. cdflib stores nanoseconds since 1970 as TT2000, so dates
  come back 30 years late. A ``datetime64`` attribute value without a type is a ``CDF_TIME_TT2000``
  entry; cdflib skips it.
- Column major files hold the values you give. cdflib writes the bytes of the C ordered array, so
  records read back transposed.
- Numbers outside the range of their integer type (given as ``Data_Type`` or as
  ``[value, 'CDF_INT1']``) raise ``ValueError`` naming the variable or attribute, the type and the
  values; nothing is written. Floats count after truncation, as C casts them (``127.9`` as
  ``CDF_INT1`` is ``127``), and ``NaN`` or infinite floats never fit. cdflib wraps them silently:
  ``300`` or ``300.5`` as ``CDF_INT1`` is ``44``, ``2**40`` as ``CDF_INT4`` or ``CDF_UINT4`` is
  ``0``, ``-1`` or ``-1.0`` as ``CDF_UINT4`` is ``4294967295``, ``2**63`` as ``CDF_INT8`` is the
  fill value, ``NaN`` and ``1e10`` as ``CDF_INT1`` are ``0``. In ``xarray_to_cdf`` this covers a
  ``CDF_DATA_TYPE`` narrower than the values.
- A string longer than ``Num_Elements`` raises ``ValueError``; cdflib shifts every record after
  it. A string ``Pad`` keeps the whole string, cut to ``Num_Elements``; cdflib keeps its first
  character.
- Global attribute entries are numbered from 0, in the order of their entry numbers: with
  ``{0: a, 3: b}``, ``b`` is entry 1. cdflib keeps entry number 3.
- Encodings without IEEE 754 floats (VAX, Alpha and IA64 VMS D and G floats) and unknown encodings
  raise ``OSError``. cdflib writes IEEE floats under these labels, or encoding 0.
- ``Data_Type`` in ``var_spec`` can also be a name (``"CDF_REAL4"``). ``Block_Factor`` is ignored:
  pycdfpp chooses the blocks.
- A list of strings typed ``CDF_CHAR`` is one entry, strings joined with ``"\N "``, in every
  method. cdflib does so in ``write_var``, but raises in ``write_variableattrs`` and stops
  ``write_globalattrs``. An entry of unknown type is skipped with a warning; cdflib also skips the
  attributes after it in ``write_globalattrs``. Numbers written as text with a numeric type are
  parsed in every method.
- An attribute value without a type gets cdflib's guess, except unsigned integers of 1 or 2 bytes,
  which are ``CDF_UINT1`` and ``CDF_UINT2`` (cdflib: ``CDF_UINT4``). Values with no CDF type
  (``uint64``, Python integers from ``2**63``, ``long double``, ``complex64``, numpy booleans) are
  skipped with a warning, never written as a wrong number. cdflib skips the numpy ones too.
- Sparse data with fewer values than its records raises ``ValueError``. cdflib warns and writes no
  data for the variable.
- A writer dropped without ``close()`` warns with a ``ResourceWarning``.
- An attribute value typed ``CDF_CHAR`` is written as its text, numbers too. cdflib raises
  ``ValueError`` for a number.
- Writing a variable attribute twice for one variable replaces its value. cdflib adds a second
  entry to the file.
- No file exists before ``close()``: ``cdr_head``, ``gdr_head`` and ``offset`` are ``0``, the
  offsets in ``zvarsinfo``, ``rvarsinfo`` and ``attrsinfo`` too, and ``compressed_file`` is
  ``None``.

xarray (``cdf_to_xarray``)
--------------------------

- The differences of ``cdfepoch`` and ``CDF`` above carry over: ``CDF_EPOCH`` times keep their
  fraction of a millisecond, ``CDF_EPOCH`` fill values give ``NaT``, sparse records hold NASA's
  values.
- Without time conversion (``to_datetime=False``, ``to_unixtime=False``), every value of a
  ``CDF_EPOCH16`` variable or attribute becomes a ``CDF_EPOCH`` value, as cdflib intends. cdflib
  converts only single values: an array keeps its ``CDF_EPOCH16`` values and only its first 7
  records, in a dimension of its own, or raises ``ValueError`` on unusual dates. Their
  ``CDF_DATA_TYPE`` attribute says ``CDF_EPOCH``, so ``xarray_to_cdf`` writes them back as the
  same instants. cdflib keeps a ``CDF_DATA_TYPE`` attribute of the file, which says
  ``CDF_EPOCH16``.
- A ``LABL_PTR_n`` whose ``DEPEND_n`` names no variable is ignored. cdflib raises ``KeyError``.
  ``DEPEND_n``, ``LABL_PTR_n`` and ``DELTA_PLUS_VAR``/``DELTA_MINUS_VAR`` values that are not
  names are ignored too; cdflib raises ``TypeError`` for arrays.

xarray (``xarray_to_cdf``)
--------------------------

The ISTP checks are cdflib's: the same Datasets warn, and with ``terminate_on_warning=True``
raise the same exception type (``ISTPError`` or ``Exception``). Messages differ. ``ISTPError`` is
``pycdfpp.cdflib.xarray.xarray_to_cdf.ISTPError``, not cdflib's class.

- Nothing is written when the conversion fails. cdflib leaves a partial file, for instance when
  a ``DEPEND_n`` value is not a name and ``terminate_on_warning=True``.
- ``NaT`` values and attributes of a time variable (``CDF_EPOCH``, ``CDF_EPOCH16``,
  ``CDF_TIME_TT2000``) are written as the type's fill value, like the ``FILLVAL`` the ISTP check
  adds. cdflib writes ``0``: 0000-01-01 for ``CDF_EPOCH`` and ``CDF_EPOCH16``,
  2000-01-01T11:58:55.816 for ``CDF_TIME_TT2000``.
- With ``unix_time_to_cdf_time=True``, missing (``NaN``) unix times are written as the time
  type's fill value, whatever ``nan_to_fillval``. cdflib raises ``OverflowError``, or writes
  ``NaN``, or turns them into ``-1e31`` and converts that as a unix time.
- ``datetime64`` values of a ``CDF_EPOCH16`` variable keep their instants. cdflib cuts them to the
  second and writes each one a record late.
- ``bytes`` values are written as their text. cdflib writes ``"b'a'"``.
- A string variable's ``Num_Elements`` counts UTF-8 bytes, so non-ASCII strings fit. cdflib
  counts characters. ``cdf_to_xarray`` (ours and cdflib's) reads strings as latin-1, so they come
  back garbled: ``'é'`` reads back as ``'Ã©'``.
- A variable without values whose only empty dimension is the record one is written with zero
  records. Any
  other empty dimension raises ``ValueError``: CDF has no dimension of size 0. cdflib writes
  dimensions of size 0, files it can't read back.
- ``uint64`` values are ``CDF_INT8`` when every value is below ``2**63``; larger values raise
  ``ValueError``. cdflib warns and writes them as ``CDF_UINT4``, wrapping values from ``2**32``.
- Big-endian numbers keep their numeric type. cdflib warns and writes them as text (one character
  per value without ``NaN`` filling).
- ``VALIDMIN``, ``VALIDMAX`` and ``FILLVAL`` get the variable's type as ISTP requires, also with
  ``unix_time_to_cdf_time=True`` (``BIN_LOCATION`` stays ``CDF_REAL4``), and also for a
  ``CDF_EPOCH`` variable given ``float64`` values, like the ``-1e31`` ``FILLVAL`` the ISTP check
  adds. cdflib leaves them to its writer's guess: ``CDF_DOUBLE`` for those ``CDF_EPOCH`` values.
  Values of another numpy type than the time type's own (a ``float32`` ``FILLVAL`` of a
  ``CDF_EPOCH`` variable) keep the guess, as in cdflib.
- A ``FILLVAL`` outside the range of the variable's integer type (a ``float`` ``-1e31`` of a
  ``CDF_INT4`` variable) is written as the type's ISTP fill value. A ``VALIDMIN`` or ``VALIDMAX``
  outside it keeps its own type. cdflib wraps them into the variable's type.
- An object array of ``datetime`` objects is a time variable whatever its length. cdflib only
  sees a single ``datetime`` as a time.
- A ``DEPEND_n`` whose shape can't be checked is logged as a warning. cdflib prints it.

Remote files (``http(s)://``, ``s3://``)
----------------------------------------

- Every ``s3_read_method`` fetches the whole object into memory. ``file`` stays the URL and ``ftype``
  stays ``"url"`` or ``"s3"``, also for compressed files: ``temp_file`` and ``compressed_file`` stay ``None``.
