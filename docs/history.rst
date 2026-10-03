=========
Changelog
=========

The full notes of each release are on
`GitHub <https://github.com/SciQLop/CDFpp/releases>`_.

0.17.0 (2026-10-03)
-------------------

* Saving a loaded file keeps what it declares. Each of these is read, written back and
  settable:

  * the GZIP compression level: ``CDF.compression_level``, ``Variable.compression_level``
    and ``add_variable(compression_level=...)``. CDFpp always wrote 9 while compressing at 6.
  * the sparse records mode: ``Variable.sparse_records`` (``pycdfpp.SparseRecords``).
  * the pad value: ``Variable.pad_value``. A resaved master got the default pad value of its
    type instead (#38).
  * the majority and byte order: ``CDF.majority`` and ``CDF.encoding``
    (``pycdfpp.Encoding``). Files are written column major or big endian when asked, a
    loaded file keeps its own. They all came back row major and in the host byte order.
  * the MD5 checksum: ``CDF.checksum`` (``pycdfpp.Checksum``). It is computed while the file
    is written. It isn't checked when loading.
  * the declared variable attributes, in their order: ``CDF.declared_variable_attributes``.
    Attributes no variable uses were dropped, and the others numbered in another order.

* Writing ``datetime64`` values gives the right dates:

  * NaT was stored as a real date, 2262-04-11 as TT2000 and 1677-09-21 as CDF_EPOCH. It is
    now stored as the fill value, which reads back as NaT.
  * Dates before 1970 read back as NaT as CDF_EPOCH16, and one millisecond late as
    CDF_EPOCH.
  * Dates before 1707, which TT2000 can't hold, overflowed. They become its illegal value,
    as with NASA's library.

* HP encoded files are read as big endian: their values had their bytes swapped.
* Column major string arrays of two dimensions or more, in several records, read with the
  strings of different records mixed.
* ``CDF.add_attribute("A", [])`` raised an error. Masters declare such attributes.
* A loop over variables or attributes sees the collection as it was when it started. Adding a
  variable during the loop could leave it reading freed memory.
* Faster saving, on the AMD Ryzen 7 5800X of :doc:`optimizations`:

  * over an existing file: 40 to 8 ms for 83 MB. btrfs and ext4 flushed the file to disk
    when it was closed.
  * column major files with multi-dimensional records: 75 to 32 ms for an MMS FPI burst
    distribution file, and loading them 81 to 58 ms.
  * files with a checksum: MD5 runs at 970 MB/s instead of 660, while the file is written.
  * big-endian files, and files with much metadata (THEMIS ESA: 18.7 to 10.8 ms).
  * ``datetime64`` time axes are converted to TT2000 with SIMD and threads: 3 to 6 times
    faster for dates before 2017.

* Loading a file eagerly, or adding variables, no longer copies the values of every variable
  already there each time the CDF grows: eager loads of MMS FGM, MMS FPI and Wind MFI files
  take 40 to 56% less time.
* Faster on Apple Silicon (MacBook Air M2), see :doc:`optimizations`:

  * time conversions use NEON on every ARM CPU: TT2000 both ways, CDF_EPOCH and EPOCH16.
  * files are written with ``write()`` on macOS, whose ``std::fstream`` copies big writes
    through a small buffer: the uncompressed MMS FGM file saves in 10.1 ms instead of 29.1,
    now faster than spacepy.
  * loops over attributes and variables no longer end by throwing a C++ exception, which is
    slow on macOS: opening the MMS FPI file and reading all its attributes takes 0.5 ms
    instead of 3.4.

* CDFpp Explorer: its WebAssembly module uses WebAssembly SIMD, which every browser has since
  2023 (Safari 16.4). Gzip saves are 10 to 17% faster, and big-endian files load up to a third
  faster. The Pyodide wheels don't use it yet.
* ``save(cdf)``'s result can be written, viewed or loaded again without ``bytes()``, which
  copies it, and has a length.
* New documentation page, :doc:`optimizations`: how reading, writing and time conversions
  are made fast, on x86, on Apple Silicon and in the browser, with the measurements behind it.

0.16.0 (2026-10-01)
-------------------

* Windows ARM64 wheels, for Python 3.11 and later.
* Python 3.15 wheels, free-threaded ones included, are tested like the others.
* Numbers given a text type (``CDF_CHAR``, ``CDF_UCHAR``) are refused: their raw bytes were
  written as characters, ``[1, 2]`` as two 8-character strings of control bytes. 0.8.7
  refused them for an existing text variable; 0.9.0 to 0.15.1 accepted them whenever the
  shapes matched, and always with an explicit ``data_type``.
* Empty attribute entries are refused. NASA's CDF library refuses them too, and reads a file
  holding one as corrupted; they were stored as ``CDF_TIME_TT2000`` whatever the type asked.
  Files that already hold one still load.
* ``save`` refuses a CDF where an attribute name is both global and variable, before opening
  the file. NASA's library refuses such a name too (``ATTR_EXISTS``), and doesn't see the
  variable attribute in a file holding both; pycdfpp wrote such files without a word.
* ``default_pad_value(CDF_EPOCH16)`` raised ``TypeError``.
* A variable created with a ``data_type`` and no values has no record, shape ``(0,)``: its
  shape was ``()``, and reading its values read past its empty buffer.
* ``[np.uint64(1)]`` as an attribute entry is stored as ``CDF_INT8``, and ``["abc"]`` as the
  string ``"abc"``: both raised errors. Several strings in one entry get a clear error.
* CDFpp Explorer: zooming redraws plots from the visible range, so every sample shows again;
  lines break at data gaps and spectrogram columns no longer stretch across them. Zoom and pan
  work as in speasy-proxy's plots (wheel, drag, pinch, Y axis on its own). Example files to
  start from, ``?url=…&var=…`` links that open a variable, and the CDFpp version in the header.
* Tests pin every behaviour of the Python API that mutation testing found unchecked, and the
  coverage report includes the Python layer.

0.15.1 (2026-09-28)
-------------------

* Values are stored exactly, or ``pycdfpp`` raises a ``ValueError``; several paths gave
  different values without a word:

  * Arrays that aren't C-contiguous or in native byte order (slices, transposed or
    Fortran-ordered arrays, big-endian data) were written from their raw memory, so with
    wrong values, for variables and for big-endian attributes.
  * With a ``data_type``, values were reinterpreted: ``int32`` values given as
    ``CDF_FLOAT`` were stored as their bits, ``-1`` as ``CDF_UINT1`` as 255, and attribute
    lists were truncated (``[1.5]`` as ``CDF_INT4`` gave 1). They are now converted when no
    value changes, and refused otherwise.
  * ``int32`` values set on an existing ``CDF_FLOAT`` variable were stored as their bits:
    they are refused, as other types already were.
  * ``uint64`` values above 2^63 turned negative in ``CDF_INT8``: they are refused.
  * ``copy=False`` borrowed arrays of another kind of the same size.

* Booleans are stored as ``CDF_UINT1``; boolean arrays and attributes raised errors.
  Lists of numpy scalars (``np.float32``, ``np.int16``, ``np.datetime64``, bytes) are
  accepted for variables and keep their type. Tuples are accepted as attribute values.
* ``pycdfpp.load(data).attributes[...]`` read freed memory when the loaded file wasn't kept
  in a variable: ``KeyError`` on existing attributes, or worse.
* Reading the values of a variable that doesn't match its shape, in a corrupt file, raises
  an exception instead of ending the Python process.
* Loading from a buffer that isn't contiguous raises instead of reading the wrong bytes.
* A 0-d array given as variable values says what is wrong: the error was empty.
* Filling an empty variable, like those of a master CDF, from another file's variable works
  again: ``set_values(other["Epoch"])`` failed since 0.9.0 with "Incompatible variable
  shapes: destination [0], source [4269]". Records may be added; the shape of each record
  must still match.
* Numpy strings, like the values of a ``CDF_CHAR`` variable, are accepted as attribute
  entries again: ``np.bytes_`` gave "Unsupported CDF type CDF_CHAR for buffer attribute"
  since 0.13.0. ``np.str_`` and one-element string arrays work too; they never did.

0.15.0 (2026-09-28)
-------------------

* ``add_variable`` and ``set_values`` take ``copy=False`` to borrow a numpy array instead of
  copying it: saving writes straight from the array. For big arrays this saves the copy's time
  and memory (a 29 MB uncompressed write goes from 16 ms to 12 ms, and 25 MB less peak memory).
* Big compressed variables load up to 3× faster on Linux: 100 MB of gzip data now reads at
  3.5 GB/s instead of 1.2 GB/s. Several decompression threads made the kernel zero the same
  huge page once for each of them.

0.14.0 (2026-09-27)
-------------------

* ``to_datetime64`` is exact for ``CDF_EPOCH``: it used to round to 256 ns (up to 128 ns off).
* ``to_datetime64`` gives ``NaT`` for fill and pad values, NaN, and dates outside
  ``datetime64[ns]``, for all three time types, and for NASA's third special TT2000 value,
  ``ILLEGAL_TT2000_VALUE`` (INT64_MIN + 3). TT2000 fill values gave 1707 dates, EPOCH16 pad
  values 1753. Lists and single values now convert like arrays: they gave clamped dates.
* TT2000 dates between 1707 and 1739 print as NASA's library prints them: they printed as 1739.
* ``to_datetime``, ``to_time_string`` and printing follow NASA's library for special values:
  fill, illegal, NaN and infinities are 9999-12-31, pad values year 0 (``datetime(1, 1, 1)``
  for ``datetime``, which has no year 0). They gave clamped dates, NaN gave 1970. Dates outside
  1677-2262 (EPOCH, EPOCH16, TT2000 up to 2292) convert and print exactly: they were clamped.
* ``to_tt2000`` turns EPOCH and EPOCH16 fill, pad and NaN values into the TT2000 fill, pad
  and illegal values, as NASA's library does: fill and pad gave 2262 dates, NaN gave 1970.
* ``to_datetime64``, ``to_datetime`` and ``to_time_string`` accept lists of numpy records,
  such as ``list(variable.values)``.
* Variables compressed in many blocks, as in most mission archives, are decompressed on
  several threads. Saving writes compressed variables as 256 KB blocks, compressed on
  several threads, which other readers can also decompress in parallel or in part.
* CDF_EPOCH and EPOCH16 conversions use SIMD on AVX2 processors too.
* ``dir(pycdfpp)`` lists the public API only.
* CDFpp Explorer shows ``NaT`` times as such, and leaves them out of plots.

0.13.1 (2026-09-26)
-------------------

* Records a file doesn't store (sparse records, gaps) are read as NASA's library reads
  them: the previous record for "previous" sparse records, else ``FILLVAL``, else the
  file's pad value, else the default pad value. They were left as uninitialized memory,
  and records after a gap were read at the wrong index.
* TT2000 values before 1972 convert like NASA's library, with the 1960-1972 drift of
  TAI-UTC. They were off by 0 to 10 s, differently on the scalar and SIMD paths.
* ``pycdfpp.default_pad_value`` returns a space for strings, as the CDF User's Guide says.
* Threads reading the same lazily loaded variable at once no longer crash: the first read
  loads the values, the others wait for it. Before, they could get freed memory.
* More operations let other Python threads run: copying values in ``set_values``, comparing
  variables or CDFs (which may decompress them), and copying a CDF.

0.13.0 (2026-09-25)
-------------------

* Saving a lazily loaded CDF over its own file no longer destroys it: ``save`` reads every
  value before opening the file. ``io::save`` returns ``false`` and ``pycdfpp.save`` raises
  ``OSError`` when the file can't be written.
* ``to_datetime`` no longer crashes on multi-dimensional time arrays, like ``(N, 1)``
  ``Epoch`` variables. Time conversions of strided arrays (slices, transposes) are correct.
* ``to_datetime64`` on EPOCH16 is exact (it lost up to 128 ns).
* Naive ``datetime`` values are UTC in every conversion, whatever the local timezone;
  timezone-aware ones are converted to UTC. ``to_epoch(list)`` is ~400x faster.
* Time conversions and ``to_time_string`` work in Pyodide and other WebAssembly builds
  without threads.
* ``load`` raises ``FileNotFoundError`` or ``ValueError`` instead of returning ``None``,
  and accepts ``pathlib.Path``. **Behavior change**: code checking ``is None`` must catch
  the exceptions instead.
* Single values (``5``, ``1.5``, a ``datetime``, a numpy scalar) are valid attribute values.
* ``CDF.filter``: a criterion left out keeps everything. **Behavior change**: before,
  ``filter(variables=[...])`` dropped every global attribute.
* Values given to a non-record-varying variable are its single record.
* Filling the empty variables of a master CDF no longer warns about overriding values, and
  scalars declared with records of shape ``(1,)`` accept plain 1-D arrays.
* C++: headers can be included from several source files; CDFpp works as a Meson
  subproject; ``meson install`` installs the whole header tree, ``libcdfpp`` and a
  ``cdfpp.pc`` file; ``io::load("file.cdf", true, false)`` loads the file.
* ``cdfdump``/``cdfirsdump --help`` show every option's full description.
* Documentation: new pages for master CDFs and for the JavaScript (WebAssembly) module.

0.12.0 (2026-09-24)
-------------------

* Experimental Blosc2 compression (``CompressionType.blosc2_compression``). Whole records
  are shuffled when they fit Blosc2's typesize limit.
* zstd and blosc2 ship in the Python wheels. Saving with them emits an
  ``ExperimentalCompressionWarning``: only CDFpp can read such files.
* CDFpp Explorer: convert a file to every codec in the browser; a 64-bit WebAssembly
  build for files over 4 GiB; GitHub-style line diff in the compare view; renamed
  variables detected.
* Fix corrupted record offsets in saved files over 2 GiB.
* Fix the zstd save path.
* Explorer: clear errors and a memory check for large files.
* Windows/MSVC build fixes.

0.11.1 (2026-09-17)
-------------------

* Fix ``str()``/``repr()`` of pre-1970 time values on Windows.
* Fix undefined behavior in ``cdf::to_time_point()`` for out-of-range time values.

0.11.0 (2026-06-23)
-------------------

* Add ``Variable.is_zvariable``.
* Add ``Variable.is_contiguous()``.

0.10.0 (2026-06-19)
-------------------

* Pyodide / ``wasm32`` wheels on PyPI: ``pycdfpp`` installs with ``micropip`` in Pyodide
  and JupyterLite.
* WebAssembly wrapper (``wacdfpp``) and the `CDFpp Explorer <https://sciqlop.github.io/CDFpp/>`_.
* Fix ``wasm32`` save corruption and several read/write correctness issues.

0.9.3 (2026-04-10)
------------------

* Fix ``force=True`` being ignored in ``Variable.set_values()`` (#76).

0.9.2 (2026-04-04)
------------------

* Fix Windows heap corruption and mmap locking; add Windows CI.

0.9.1 (2026-04-03)
------------------

* Add ``to_time_string()`` for vectorized CDF time to string conversion with user-defined format (closes #70)
* Fix PyCapsule_New null pointer crash on empty variables (Windows)

0.9.0 (2026-04-01)
------------------

* SIMD-optimized time conversions (AVX512/AVX2/SSE2 runtime dispatch for CDF_EPOCH, EPOCH16, TT2000)
* **Breaking**, not announced at the time: the raw values of time variables name their field
  after its unit. ``values['value']`` became ``values['nseconds']`` for TT2000 and
  ``values['mseconds']`` for CDF_EPOCH. :func:`pycdfpp.to_datetime64` is unchanged.
* Improved Python exception messages with actionable context
* Reject unexpected keyword arguments in Python API functions
* Reject Variables and Attributes with empty names
* Fix epoch16 to_epoch16 catastrophic cancellation losing sub-second precision
* Fix leap_second() off-by-one error and pre-1972 TT2000 handling
* Fix RLE corruption for sequences of >256 zeros
* Fix nomap container: swap corruption, const_iterator UB, asymmetric equality
* Fix mmap MAP_FAILED not detected as invalid mapping, fd leak on failure
* Fix null-check for libdeflate compressor
* Fix record_count * record_size uint32_t overflow
* Fix missing std::move in Variable constructors and rvalue assignment operators
* Fix blk_iterator postfix operator++ calling step_forward(0)
* Fix CDF_UCHAR shape handling
* Fix cdf_map __getitem__ to raise KeyError instead of silently inserting
* Fix missing CPR record for empty compressed variables (#52)
* Fix ZSTD error checking and meson ZSTD build
* C++20 modernization: concepts, structured bindings, std::ranges, if constexpr, std::visit with overload pattern
* Apple Clang 15 compatibility fix

0.8.6 (2026-01-14)
------------------

* Switch to bump-my-version
* Auto-convert Latin-1 strings to UTF-8 on load
* Bump actions/download-artifact from 6 to 7
* Bump actions/upload-artifact from 5 to 6

0.8.5 (2025-12-09)
------------------

* Add ccache support for Linux builds
* Migrate to macos-15 for Intel runner, raise minimum macOS version to 13.0
* Ensure wraps are always built for MACOSX_DEPLOYMENT_TARGET
* Ignore existing packages on PyPI upload

0.8.4 (2025-12-05)
------------------

* Do not push Wasm wheels to PyPI yet

0.8.3 (2025-11-28)
------------------

* Experimental Pyodide/Wasm builds
* Configure Dependabot for GitHub Actions updates
* Bump actions/checkout v4→v6, codecov-action v4→v5, upload-artifact v4→v5, download-artifact v4→v6

0.8.2 (2025-10-08)
------------------

* CI: use the latest pyenv on macOS

0.8.1 (2025-10-07)
------------------

* Update CI and bump dependencies

0.8.0 (2025-07-28)
------------------

* Add support for filtering CDF content using static list, regex, or callable
* Add support for setting Attributes and Variables values from other Attributes and Variables
* Add support for cloning variables or attributes
* Add Python 3.14(t) build and tests (free-threaded Python)
* Update PyBind11
* Improve support for CDF special values
* Honor numpy dtype for attributes values that are List[np.intX or np.uintX]

0.7.7 (2025-05-08)
------------------

* Bump CI build wheel and use native Linux ARM runners

0.7.6 (2025-01-22)
------------------

* Drop ppc wheels (too slow to build on GitHub Actions)

0.7.5 (2025-01-21)
------------------

* Switch to MIT license
* Add new package architectures, CI cleanup
* Build Linux wheels in parallel
* Bump cibuildwheel, update wraps for Python 3.13

0.7.4 (2024-09-18)
------------------

* Fix wrong majority swap with string labels

0.7.3 (2024-06-22)
------------------

* Add numpy 2.0 support and Python 3.13
* Ensure majority swap is correct with strings

0.7.2 (2024-06-20)
------------------

* Fix majority wrong swap with data cubes
* Avoid numpy 2.0 until next release

0.7.1 (2024-06-05)
------------------

* Add experimental ZSTD compression algorithm support
* Ensure C++ side always gets C-contiguous arrays and avoids views
* Fixes #30 and adds ImHex rudimentary patterns

0.7.0 (2024-05-17)
------------------

* Add preliminary support for CDF file format versions prior to 2.5
* Add minimum supported CDF file format version
* Benchmark with CDAWeb masters
* Sanitizer fixes and specific handling across 2.x versions

0.6.4 (2024-04-17)
------------------

* CI fixes and build architecture configuration

0.6.3 (2024-03-11)
------------------

* macOS compatibility fixes

0.6.2 (2024-03-08)
------------------

* Release GIL as much as possible
* Switch to cibuildwheel
* Allow building without Python wrapper
* Apple Clang fixes
* Basic WASM proof of concept

0.6.1 (2023-12-05)
------------------

* Fix writing empty attributes strings (ensure numElements cdf fields > 0)
* set_values assume values=[] if only data_type is provided
* Attributes values reset, user can now change attributes values once set

0.6.0 (2023-10-18)
------------------

* Fixes + Unfinished skeletons export
* Adds Python 3.12 support.
* Builds with O3 optimizations instead of O2.
* Always expose record count as first dimension (even with NRV variables)

0.5.0 (2023-09-06)
------------------

* Add support for writing CDF files.
* Add support for lazy loading variables (default behavior now).
* Read performances improvements.
* Exposes string variables values as numpy array of unencoded strings by default (.values).
* Add support for encoding string variables values (.values_encoded).
* Exposes CDF version, compression, majority,...

0.4.6 (2023-06-22)
------------------

* Fixes Windows 'access violation' error.


0.4.5 (2023-06-21)
------------------

* Mostly CI refactoring.


0.4.4 (2023-06-19)
------------------

* Packaging fix.
