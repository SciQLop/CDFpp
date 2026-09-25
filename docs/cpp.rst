=========
C++ guide
=========

CDFpp is written in C++20. The Python package is a thin layer on top of it.
You can use the C++ library directly, without Python.

This page shows how to read and write CDF files from C++.
Every example is a complete program. Each one was compiled and run.

The reading examples use a real public file: one day of ACE magnetic field data.
Download it once, next to your program:

.. code-block:: console

    $ curl -O https://spdf.gsfc.nasa.gov/pub/data/ace/mag/level_2_cdaweb/mfi_h0/2020/ac_h0_mfi_20200101_v07.cdf


Adding CDFpp to your project
============================

CDFpp is a set of headers. There is no library to link, apart from a compression library.

You need:

- a C++20 compiler (a recent GCC, Clang or MSVC),
- the CDFpp ``include/`` folder,
- three small header-only dependencies: `cpp_utils <https://github.com/jeandet/cpp_utils>`_,
  `hedley <https://nemequ.github.io/hedley/>`_ and `fmt <https://fmt.dev>`_,
- zlib, or `libdeflate <https://github.com/ebiggers/libdeflate>`_ (faster), for gzip.

CDFpp also needs a small configuration header, ``cdfpp_config.h``.
Meson generates it when you build CDFpp.
You can also write it by hand. On a little-endian machine (x86_64, ARM64) it is:

.. code-block:: cpp

    #pragma once
    #define CDFPP_VERSION "0.12.0"
    #define CDFpp_ENCODING cdf_encoding::IBMPC
    #define CDFpp_LITTLE_ENDIAN
    #define CDFpp_USE_NOMAP
    // #define CDFpp_USE_LIBDEFLATE   // uncomment to use libdeflate instead of zlib

Put it in a folder on your include path. Then compile your program like this.
Here ``CDFpp`` is a clone of the repository, and its dependencies come from its ``subprojects/`` folder:

.. code-block:: console

    $ g++ -std=c++20 -DCDFPP_NO_SIMD -DFMT_HEADER_ONLY \
          -Imy_config_dir \
          -ICDFpp/include \
          -ICDFpp/subprojects/cpp_utils/include \
          -ICDFpp/subprojects/hedley-15 \
          -ICDFpp/subprojects/fmt-12.0.0/include \
          main.cpp -o main -lz

The ``subprojects/`` folders are filled the first time you run ``meson setup`` in the CDFpp repository.
You can also point at your own copies of cpp_utils, hedley and fmt.

``-DCDFPP_NO_SIMD`` turns off the SIMD time conversions.
They need two extra source files from ``src/arch/x86/``, compiled with xsimd.
Without SIMD, time conversions still work. They are just slower on very large arrays.

Optional codecs
---------------

gzip and RLE are always available. Two experimental codecs can be turned on:

.. list-table::
   :header-rows: 1

   * - Codec
     - Define
     - Link with
   * - zstd
     - ``-DCDFPP_USE_ZSTD``
     - ``-lzstd``
   * - Blosc2
     - ``-DCDFPP_USE_BLOSC2``
     - ``-lblosc2``

.. warning::

    zstd and Blosc2 are **not** part of the CDF standard.
    Only CDFpp can read files that use them.
    Use gzip for files you share with other people or tools.

With Meson
----------

CDFpp can be a Meson subproject. Add ``subprojects/cdfpp.wrap`` to your project:

.. code-block:: ini

    [wrap-git]
    url = https://github.com/SciQLop/CDFpp.git
    revision = main
    depth = 1

Then use it as a dependency. Meson fetches CDFpp's own dependencies for you:

.. code-block:: meson

    project('my_tool', 'cpp', 'c', default_options : ['cpp_std=c++20'])
    cdfpp_dep = dependency('cdfpp', fallback : ['cdfpp', 'cdfpp_dep'],
                           default_options : ['with_tests=false'])
    executable('my_tool', 'main.cpp', dependencies : cdfpp_dep)

Enable ``'c'`` as well as ``'cpp'``: the gzip library CDFpp uses by default,
libdeflate, is written in C.

Installing
----------

``meson install`` installs the headers, a small ``libcdfpp`` library (the SIMD time
conversions on x86), and a ``cdfpp.pc`` pkg-config file.

The headers also need `cpp_utils <https://github.com/jeandet/cpp_utils>`_, which installs
the same way (``meson install``, ``cpp_utils.pc``), plus ``hedley.h`` and the gzip library
(libdeflate or zlib) from your system. Then:

.. code-block:: console

    $ g++ -std=c++20 main.cpp $(pkg-config --cflags --libs cdfpp) -ldeflate


Loading a file
==============

``cdf::io::load`` reads a file and returns a ``std::optional<cdf::CDF>``.
The optional is empty when the file is missing or is not a CDF file.

.. code-block:: cpp

    #include <cdfpp/cdf.hpp>
    #include <iostream>

    int main()
    {
        auto cdf = cdf::io::load("ac_h0_mfi_20200101_v07.cdf");
        if (!cdf)
        {
            std::cerr << "Not a valid CDF file\n";
            return 1;
        }

        std::cout << "Majority: " << (cdf->majority == cdf::cdf_majority::row ? "row" : "column")
                  << '\n';

        for (const auto& [name, attribute] : cdf->attributes)
            std::cout << "Global attribute " << name << " has " << attribute.size() << " entries\n";

        for (const auto& [name, variable] : cdf->variables)
        {
            std::cout << name << ": " << cdf::cdf_type_str(variable.type()) << " shape (";
            for (auto dim : variable.shape())
                std::cout << dim << ' ';
            std::cout << ") records: " << variable.len() << (variable.is_nrv() ? " NRV" : "")
                      << (variable.compression_type() != cdf::cdf_compression_type::no_compression
                                 ? " compressed"
                                 : "")
                      << '\n';
        }
    }

The output starts like this:

.. code-block:: text

    Majority: column
    Global attribute TITLE has 1 entries
    Global attribute Project has 2 entries
    ...
    Epoch: CDF_EPOCH shape (5401 1 ) records: 5401
    BGSEc: CDF_REAL4 shape (5401 3 ) records: 5401 compressed
    label_BGSE: CDF_CHAR shape (1 3 6 ) records: 1 NRV
    ...

A few things to notice:

- ``cdf->variables`` and ``cdf->attributes`` keep the order of the file.
- The first number of a shape is always the number of records.
  ``BGSEc`` has 5401 records of 3 values each.
- A variable that does not change with time is "non-record-varying" (NRV).
  It has a single record.
- You can print a whole file with ``std::cout << *cdf;``.

Lazy loading
------------

By default, ``load`` reads only the structure of the file: names, types, shapes, attributes.
The values of a variable are read the first time you access them.
This makes opening a big file fast when you only need a few variables.

Pass ``false`` as the third argument to read everything up front.

.. code-block:: cpp

    #include <cdfpp/cdf.hpp>
    #include <fstream>
    #include <iostream>
    #include <iterator>
    #include <span>
    #include <vector>

    std::vector<char> read_file(const std::string& path)
    {
        std::ifstream file { path, std::ios::binary };
        return { std::istreambuf_iterator<char> { file }, std::istreambuf_iterator<char> {} };
    }

    int main()
    {
        // Lazy loading is the default for files: values are read on first access.
        auto lazy = cdf::io::load("ac_h0_mfi_20200101_v07.cdf");
        std::cout << "Loaded before access: " << lazy->variables.at("BGSEc").values_loaded() << '\n';
        std::cout << "Records: " << lazy->variables.at("BGSEc").get<float>().size() / 3 << '\n';
        std::cout << "Loaded after access: " << lazy->variables.at("BGSEc").values_loaded() << '\n';

        // Eager loading: the third argument is lazy_load.
        auto eager = cdf::io::load("ac_h0_mfi_20200101_v07.cdf", true, false);
        std::cout << "Eager, loaded: " << eager->variables.at("BGSEc").values_loaded() << '\n';

        // From memory: move the buffer in, so the CDF owns it.
        auto from_memory = cdf::io::load(read_file("ac_h0_mfi_20200101_v07.cdf"));
        std::cout << "From memory: " << from_memory->variables.size() << " variables\n";

        // Bulk time conversion: nanoseconds since 1970-01-01 (Unix time).
        const auto& epochs = from_memory->variables.at("Epoch").get<cdf::epoch>();
        std::vector<int64_t> unix_ns(std::size(epochs));
        cdf::to_ns_from_1970(std::span { epochs.data(), epochs.size() }, unix_ns.data());
        std::cout << "First sample, ns since 1970: " << unix_ns[0] << '\n';
    }

The second argument converts old Latin-1 text to UTF-8. Leave it to ``true``.

Loading from memory
-------------------

``load`` also accepts a buffer: a ``std::vector<char>``, or a ``const char*`` and a size.
This is useful for data that comes from the network or from an archive.

- **Move** the vector in (``load(std::move(buffer))``, or a temporary as above).
  The CDF then owns the bytes, and lazy loading is safe.
- If you pass a vector by reference, or a pointer and a size, CDFpp does not copy it.
  Keep the buffer alive as long as you use the CDF.
  These overloads load eagerly by default.


Reading values
==============

Each variable stores its values in one flat vector, in row-major order, records first.
Ask for it with ``get<T>()``, where ``T`` is the C++ type of the CDF type:

.. list-table::
   :header-rows: 1

   * - CDF type
     - C++ type
   * - ``CDF_INT1``, ``CDF_BYTE`` / ``CDF_INT2`` / ``CDF_INT4`` / ``CDF_INT8``
     - ``int8_t`` / ``int16_t`` / ``int32_t`` / ``int64_t``
   * - ``CDF_UINT1`` / ``CDF_UINT2`` / ``CDF_UINT4``
     - ``uint8_t`` / ``uint16_t`` / ``uint32_t``
   * - ``CDF_FLOAT``, ``CDF_REAL4``
     - ``float``
   * - ``CDF_DOUBLE``, ``CDF_REAL8``
     - ``double``
   * - ``CDF_CHAR`` / ``CDF_UCHAR``
     - ``char`` / ``unsigned char``
   * - ``CDF_EPOCH`` / ``CDF_EPOCH16`` / ``CDF_TIME_TT2000``
     - ``cdf::epoch`` / ``cdf::epoch16`` / ``cdf::tt2000_t``

You can also write the CDF type instead: ``get<cdf::CDF_Types::CDF_REAL4>()``.
Asking for the wrong type throws ``std::bad_variant_access``.

.. code-block:: cpp

    #include <cdfpp/cdf.hpp>
    #include <format>
    #include <iostream>
    #include <string>

    std::string as_string(const cdf::data_t& data)
    {
        const auto& chars = data.get<char>();
        return { std::cbegin(chars), std::cend(chars) };
    }

    int main()
    {
        auto cdf = cdf::io::load("ac_h0_mfi_20200101_v07.cdf");
        if (!cdf)
            return 1;

        // Numeric values: one flat vector, row-major, records first.
        const auto& b_gse = (*cdf)["BGSEc"];
        const auto& values = b_gse.get<float>();
        const auto components = b_gse.shape()[1];
        std::cout << "First record: " << values[0] << ", " << values[1] << ", " << values[2]
                  << '\n';
        std::cout << "Bz of record 100: " << values[100 * components + 2] << '\n';

        // A variable attribute holds exactly one value.
        std::cout << "UNITS: " << as_string(b_gse.attributes.at("UNITS").value()) << '\n';
        std::cout << "FILLVAL: " << b_gse.attributes.at("FILLVAL").get<float>()[0] << '\n';

        // A global attribute holds a list of entries, each with its own type.
        const auto& project = cdf->attributes.at("Project");
        for (const auto& entry : project)
            std::cout << "Project entry: " << as_string(entry) << '\n';

        // String variables: the last dimension is the string length.
        const auto& labels = (*cdf)["label_BGSE"];
        const auto& chars = labels.get<char>();
        const auto length = labels.shape().back();
        for (std::size_t i = 0; i < std::size(chars); i += length)
            std::cout << "Label: " << std::string(&chars[i], length) << '\n';

        // When you don't know the type in advance, visit the data.
        cdf::visit(b_gse.attributes.at("VALIDMIN").value(),
            [](const no_init_vector<char>& text)
            { std::cout << "text: " << std::string(std::cbegin(text), std::cend(text)) << '\n'; },
            [](const auto& numbers)
            {
                if constexpr (requires { numbers[0] + 0; })
                    std::cout << "VALIDMIN first value: " << numbers[0] << '\n';
            });

        // Time: convert a CDF time to std::chrono::system_clock::time_point.
        const auto& epochs = (*cdf)["Epoch"].get<cdf::epoch>();
        auto first = cdf::to_time_point(epochs[0]);
        auto last = cdf::to_time_point(epochs[std::size(epochs) - 1]);
        std::cout << std::format("From {:%F %T} to {:%F %T}\n", first, last);
    }

Output:

.. code-block:: text

    First record: 2.291, -1.483, -0.067
    Bz of record 100: 0.915
    UNITS: nT
    FILLVAL: -1e+31
    Project entry: ACE>Advanced Composition Explorer
    Project entry: ISTP>International Solar-Terrestrial Physics
    Label: Bx GSE
    Label: By GSE
    Label: Bz GSE
    VALIDMIN first value: -65534
    From 2020-01-01 00:00:00.000000000 to 2020-01-01 23:59:59.000000000

Global and variable attributes
------------------------------

CDF has two kinds of attributes. They look alike but are stored differently.

- A **global attribute** (``cdf::Attribute``) describes the whole file.
  It is a list of *entries*. Each entry is a ``cdf::data_t`` with its own type.
  ``Project`` above has two entries.
- A **variable attribute** (``cdf::VariableAttribute``) describes one variable.
  It holds exactly one ``cdf::data_t``. Get it with ``.value()``.

Text is stored as ``CDF_CHAR``. It is not null-terminated: build a ``std::string`` from the chars.

.. tip::

    Look names up with ``at()`` or ``count()``.
    Like ``std::map``, ``operator[]`` on a non-const map **inserts** an empty entry when the name
    is missing. ``cdf["name"]`` on the CDF itself is safe: it calls ``variables.at()``.

Fill values
-----------

Missing data is marked with the value of the ``FILLVAL`` attribute, often ``-1e31``.
CDFpp returns it as it is stored. Compare values to ``FILLVAL`` before you compute with them.


Working with time
=================

CDF has three time types. Each is a small struct in namespace ``cdf``:

.. list-table::
   :header-rows: 1

   * - Type
     - Field
     - Meaning
   * - ``cdf::epoch``
     - ``double mseconds``
     - milliseconds since 0000-01-01, no leap seconds
   * - ``cdf::epoch16``
     - ``double seconds, picoseconds``
     - seconds since 0000-01-01, plus picoseconds
   * - ``cdf::tt2000_t``
     - ``int64_t nseconds``
     - nanoseconds since 2000-01-01T12:00 (J2000), **with** leap seconds

``cdf::to_time_point`` turns any of them into a ``std::chrono::system_clock::time_point``.
``cdf::to_tt2000``, ``cdf::to_epoch`` and ``cdf::to_epoch16`` go the other way.
They accept one time point, or a container of them.

.. code-block:: cpp

    #include <cdfpp/cdf.hpp>
    #include <chrono>
    #include <format>
    #include <iostream>

    int main()
    {
        using namespace std::chrono;
        const system_clock::time_point t = sys_days { year { 2020 } / 1 / 1 } + 12h + 30min;

        // From std::chrono to the three CDF time types...
        cdf::tt2000_t tt = cdf::to_tt2000(t);
        cdf::epoch ep = cdf::to_epoch(t);
        cdf::epoch16 ep16 = cdf::to_epoch16(t);
        std::cout << "TT2000: " << tt.nseconds << " ns since J2000\n";
        std::cout << "EPOCH: " << ep.mseconds << " ms since year 0\n";
        std::cout << "EPOCH16: " << ep16.seconds << " s + " << ep16.picoseconds << " ps\n";

        // ...and back.
        std::cout << std::format("Back from TT2000: {:%F %T}\n", cdf::to_time_point(tt));

        // TT2000 counts leap seconds: 2016-12-31 had one, so this "1 second" lasts 2 in TT2000.
        const auto before = sys_days { year { 2016 } / 12 / 31 } + 23h + 59min + 59s;
        const auto after = sys_days { year { 2017 } / 1 / 1 };
        std::cout << "UTC gap: " << duration_cast<seconds>(after - before).count() << " s, TT2000 gap: "
                  << (cdf::to_tt2000(after).nseconds - cdf::to_tt2000(before).nseconds) / 1'000'000'000
                  << " s\n";
        std::cout << "Leap second table last updated: " << cdf::chrono::leap_seconds::last_updated
                  << '\n';
    }

Output:

.. code-block:: text

    TT2000: 631153869184000000 ns since J2000
    EPOCH: 6.37451e+13 ms since year 0
    EPOCH16: 6.37451e+10 s + 0 ps
    Back from TT2000: 2020-01-01 12:30:00.000000000
    UTC gap: 1 s, TT2000 gap: 2 s
    Leap second table last updated: 20170101

``std::chrono::system_clock`` ignores leap seconds. TT2000 does not.
The conversion functions take care of the difference, using the built-in leap second table.

For large arrays, ``cdf::to_ns_from_1970(span, output)`` converts a whole span at once
into nanoseconds since 1970. See the lazy loading example above.


Creating a CDF file
===================

This is the main task for data producers. The steps are:

1. Create an empty ``cdf::CDF``.
2. Add global attributes with ``cdf::add_attribute``.
3. Build each ``cdf::Variable``, add its attributes, then add it with ``cdf::add_variable``.
4. Save with ``cdf::io::save``.

A ``Variable`` is built from a name, a number, its data and its shape.
Optional arguments follow: majority, whether it is NRV, and its compression.

.. code-block:: cpp

    #include <cdfpp/cdf.hpp>
    #include <chrono>
    #include <cmath>
    #include <iostream>
    #include <string_view>
    #include <vector>

    using namespace cdf;

    // CDF strings are CDF_CHAR data: build them with an explicit type.
    data_t text(std::string_view s)
    {
        return { cdf_values_t { no_init_vector<char>(std::cbegin(s), std::cend(s)) },
            CDF_Types::CDF_CHAR };
    }

    int main()
    {
        constexpr std::size_t records = 10;

        CDF file;
        file.compression = cdf_compression_type::gzip_compression; // whole-file compression

        // Global attributes: each one is a list of entries.
        add_attribute(file, "Project", { text("My Mission>My Space Mission") });
        add_attribute(file, "Logical_source", { text("mm_l2_mag") });
        add_attribute(file, "TEXT", { text("Line one of the description."), text("Line two.") });

        // Time variable: TT2000, one value per record.
        std::vector<std::chrono::system_clock::time_point> times;
        const auto start = std::chrono::sys_days { std::chrono::year { 2020 } / 1 / 1 };
        for (std::size_t i = 0; i < records; ++i)
            times.push_back(start + std::chrono::seconds { i });
        Variable epoch { "Epoch", 0, data_t { to_tt2000(times) }, { records } };
        epoch.attributes.emplace("UNITS", "UNITS", text("ns"));
        add_variable(file, "Epoch", std::move(epoch));

        // Data variable: a 3-component vector per record, shape = {records, 3}.
        no_init_vector<float> b(records * 3);
        for (std::size_t i = 0; i < std::size(b); ++i)
            b[i] = static_cast<float>(std::sin(0.1 * i));
        Variable mag { "B_GSE", 1, data_t { std::move(b) }, { records, 3 }, cdf_majority::row,
            false, cdf_compression_type::gzip_compression }; // per-variable compression
        mag.attributes.emplace("DEPEND_0", "DEPEND_0", text("Epoch"));
        mag.attributes.emplace("UNITS", "UNITS", text("nT"));
        mag.attributes.emplace("FILLVAL", "FILLVAL", data_t { no_init_vector<float> { -1e31f } });
        mag.attributes.emplace("LABL_PTR_1", "LABL_PTR_1", text("B_GSE_labels"));
        add_variable(file, "B_GSE", std::move(mag));

        // Non-record-varying (NRV) label variable: 1 record of 3 strings of 6 characters.
        Variable labels { "B_GSE_labels", 2, text("Bx GSEBy GSEBz GSE"), { 1, 3, 6 },
            cdf_majority::row, true };
        add_variable(file, "B_GSE_labels", std::move(labels));

        if (!io::save(file, "my_mission.cdf"))
        {
            std::cerr << "Could not write my_mission.cdf\n";
            return 1;
        }

        // Or serialize in memory, e.g. to send it over the network.
        auto bytes = io::save(file);
        std::cout << "Serialized " << bytes.size() << " bytes\n";

        // Read it back to check.
        auto check = io::load("my_mission.cdf");
        std::cout << *check;
    }

Output:

.. code-block:: text

    Serialized 1093 bytes
    CDF:
      version: 3.8.0
      majority: row
      compression: GNU GZIP

    Attributes:
      Project: "My Mission>My Space Mission"
      Logical_source: "mm_l2_mag"
      TEXT: [ [ "Line one of the description.", "Line two." ] ]

    Variables:
      Epoch: [ 10 ], [CDF_TIME_TT2000], record vary:True, compression: None
      B_GSE: [ 10, 3 ], [CDF_FLOAT], record vary:True, compression: GNU GZIP
      B_GSE_labels: [ 1, 3, 6 ], [CDF_CHAR], record vary:False, compression: None

The rules to remember:

- **Shape.** The first number is the number of records. The rest is the shape of one record.
  The data vector holds ``records × record size`` values, in row-major order.
- **Types.** ``data_t { no_init_vector<T> {...} }`` picks the CDF type from ``T``:
  ``float`` gives ``CDF_FLOAT``, ``int32_t`` gives ``CDF_INT4``, ``tt2000_t`` gives ``CDF_TIME_TT2000``, and so on.
  For text, or to choose ``CDF_REAL4`` over ``CDF_FLOAT``, pass the type explicitly as in ``text()`` above.
- **Strings.** For ``CDF_CHAR`` variables, the last number of the shape is the string length.
  Pad shorter strings with spaces so that they all have that length.
- **NRV variables.** Pass ``true`` as the NRV argument and give them a single record.
  Use them for data that doesn't change with time: labels, energy tables, calibration constants.
- **Attribute types.** A ``FILLVAL``, ``VALIDMIN`` or ``VALIDMAX`` must have the same type as its variable.
  Here ``FILLVAL`` is a ``float`` for a ``CDF_FLOAT`` variable.
- **Compression.** Set ``file.compression`` to compress the whole file,
  or pass a compression to a ``Variable`` to compress only its values. Both work in any CDF reader.

Naming variables and attributes like ``DEPEND_0``, ``UNITS`` and ``FILLVAL``
follows the ISTP conventions used by most space physics archives.
See :doc:`the writing guide <writing>` for what each one means.
You can check a file against ISTP with `CDFpp Explorer <https://sciqlop.github.io/CDFpp/>`_.


Modifying an existing file
==========================

Load the file, change it in memory, then save it.

.. code-block:: cpp

    #include <cdfpp/cdf.hpp>
    #include <iostream>
    #include <string_view>

    using namespace cdf;

    data_t text(std::string_view s)
    {
        return { cdf_values_t { no_init_vector<char>(std::cbegin(s), std::cend(s)) },
            CDF_Types::CDF_CHAR };
    }

    int main()
    {
        auto file = io::load("ac_h0_mfi_20200101_v07.cdf");
        if (!file)
            return 1;

        // Drop a variable we don't want to distribute.
        if (auto it = file->variables.find("Time_PB5"); it != file->variables.end())
            file->variables.erase(it);

        // Replace a global attribute's entries.
        file->attributes.at("Data_version") = Attribute::attr_data_t { text("8") };

        // Replace a variable attribute's value (emplace would NOT overwrite an existing one).
        auto& b_gse = file->variables.at("BGSEc");
        b_gse.attributes.at("VAR_NOTES") = text("Reprocessed");

        // Add a new variable attribute.
        b_gse.attributes.emplace("SCALETYP", "SCALETYP", text("linear"));

        // Compress one variable that was stored uncompressed.
        file->variables.at("Epoch").set_compression_type(cdf_compression_type::gzip_compression);

        // Save under a new name.
        if (!io::save(*file, "ac_h0_mfi_20200101_v08.cdf"))
            return 1;

        auto check = io::load("ac_h0_mfi_20200101_v08.cdf");
        std::cout << "Variables: " << check->variables.size()
                  << ", has Time_PB5: " << check->variables.count("Time_PB5") << '\n';
    }

Two things can surprise you:

- ``emplace`` does nothing when the name already exists, like ``std::map::emplace``.
  To replace a value, assign to ``at(name)``.
- The maps store their elements in a ``std::vector``.
  Adding or removing a variable or attribute can move the others in memory.
  Don't keep references across an ``erase`` or an ``emplace``: look the element up again.
  That is why the example erases first, then takes ``b_gse``.

Saving over the file you loaded is safe, even with lazy loading: ``io::save`` reads every
value before it opens the file. It returns ``false`` if the file can't be written.


Errors and threads
==================

.. code-block:: cpp

    #include <cdfpp/cdf.hpp>
    #include <future>
    #include <iostream>
    #include <string>
    #include <variant>
    #include <vector>

    int main()
    {
        // A missing or non-CDF file gives an empty optional, not an exception.
        if (!cdf::io::load("does_not_exist.cdf"))
            std::cout << "missing file: nullopt\n";

        // Asking for the wrong C++ type throws std::bad_variant_access.
        auto file = cdf::io::load("ac_h0_mfi_20200101_v07.cdf");
        try
        {
            [[maybe_unused]] const auto& wrong = file->variables.at("BGSEc").get<double>();
        }
        catch (const std::bad_variant_access&)
        {
            std::cout << "BGSEc is CDF_REAL4: use get<float>()\n";
        }

        // Unknown names throw std::out_of_range with at() (and with CDF::operator[]).
        try
        {
            [[maybe_unused]] const auto& v = file->variables.at("NoSuchVariable");
        }
        catch (const std::out_of_range&)
        {
            std::cout << "no such variable\n";
        }

        // No global state: load (and save) different files from several threads at once.
        std::vector<std::future<std::size_t>> jobs;
        for (int i = 0; i < 4; ++i)
            jobs.push_back(std::async(std::launch::async,
                []
                {
                    auto f = cdf::io::load("ac_h0_mfi_20200101_v07.cdf");
                    return f->variables.at("BGSEc").get<float>().size();
                }));
        for (auto& job : jobs)
            std::cout << "thread read " << job.get() << " values\n";
    }

In short:

- ``load`` returns an empty optional when it can't read a CDF file.
  Loading a directory throws ``std::runtime_error``.
- A wrong type in ``get<T>()`` throws ``std::bad_variant_access``.
- A missing name in ``at()`` throws ``std::out_of_range``.
- Building a ``Variable`` whose data doesn't match its shape throws ``std::invalid_argument``.
  So does an empty variable or attribute name.

.. warning::

    ``cdf::io::save`` currently returns ``true`` even when the file can't be written,
    for example when the folder doesn't exist.
    If it matters, check that the file exists after saving.

**Threads.** Unlike NASA's C library, CDFpp has no global state.
You can load and save *different* ``CDF`` objects from as many threads as you like.
As with any C++ object, don't modify one ``CDF`` object from two threads at the same time.
