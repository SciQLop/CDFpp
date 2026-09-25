========================
JavaScript (WebAssembly)
========================

CDFpp compiles to WebAssembly. The result is a JavaScript module, ``cdfpp.js``, that reads
CDF files in a web page, a Web Worker, or Node.js. Nothing is sent to a server: the files
are decoded where your code runs. The :doc:`explorer` is built on it.

This page shows how to use it in your own application. If you work in Python in the
browser (Pyodide, JupyterLite), use ``pycdfpp`` instead: see :doc:`installation`.

What it can do
==============

- Open a CDF file from bytes, and list its variables and attributes.
- Get variable values as typed arrays (``Float32Array``, ``Int32Array``, …), without
  copying them.
- Convert time variables to nanoseconds since 1970, and time attributes to ISO dates.
- Save the file again, with the same or another compression.

It doesn't create CDF files from scratch or modify their content: for that, use
``pycdfpp`` or the C++ library.

Getting the module
==================

The module is two files: ``cdfpp.js`` and ``cdfpp.wasm``. They must be served from the
same folder.

**Build it** with `Emscripten <https://emscripten.org/>`_ and meson:

.. code-block:: console

    $ meson setup build_wasm --cross-file wacdfpp/wasm.txt -Dwith_experimental_wasm=true
    $ ninja -C build_wasm
    $ ls build_wasm/wacdfpp/cdfpp.*
    build_wasm/wacdfpp/cdfpp.js  build_wasm/wacdfpp/cdfpp.wasm

Add ``-Dwith_experimental_zstd=true -Dwith_experimental_blosc2=true`` to include the
experimental codecs (see :doc:`compression`).

**Or use the copy published with the Explorer**, at
``https://sciqlop.github.io/CDFpp/cdfpp.js``. It can be imported from any site, but it
always follows the latest development version. For an application that must keep working,
copy the two files into your project.

TypeScript declarations for the whole API are in
`wacdfpp/cdfpp.d.ts <https://github.com/SciQLop/CDFpp/blob/main/wacdfpp/cdfpp.d.ts>`_.

Loading the module
==================

``cdfpp.js`` is an ES module. Its default export creates the module, asynchronously:

.. code-block:: javascript

    import createCdfModule from "./cdfpp.js";

    const Module = await createCdfModule();

Create it once, and reuse it for every file.

Opening a file
==============

``Module.load`` takes the file content as a ``Uint8Array``.

In a browser, from a URL or from a file the user picked:

.. code-block:: javascript
    :class: browser-only

    const response = await fetch("https://spdf.gsfc.nasa.gov/pub/data/ace/mag/level_2_cdaweb/mfi_h0/2020/ac_h0_mfi_20200101_v07.cdf");
    const cdf = Module.load(new Uint8Array(await response.arrayBuffer()));

    // or, from an <input type="file">:
    const picked = Module.load(new Uint8Array(await input.files[0].arrayBuffer()));

In Node.js:

.. code-block:: javascript

    import { readFileSync } from "node:fs";

    const cdf = Module.load(new Uint8Array(readFileSync("ac_h0_mfi_20200101_v07.cdf")));

Check that the file was valid. ``load`` doesn't throw for a file that isn't a CDF:

.. code-block:: javascript

    if (!cdf.is_valid())
        throw new Error("not a CDF file");

    console.log(cdf.variable_names());   // [ 'Epoch', 'Time_PB5', 'Magnitude', 'BGSEc', ... ]
    console.log(cdf.attribute_names());  // [ 'TITLE', 'Project', 'Discipline', ... ]

.. warning::

    A ``CdfFile`` lives in WebAssembly memory, which JavaScript's garbage collector doesn't
    manage. **Call** ``cdf.delete()`` **when you are done with it**, or the memory is never
    freed.

Reading a variable
==================

``get_variable`` returns a plain object with everything about the variable:

.. code-block:: javascript

    const b = cdf.get_variable("BGSEc");
    b.name          // 'BGSEc'
    b.type_name     // 'CDF_REAL4'
    b.shape         // [ 5401, 3 ]: 5401 records of 3 values
    b.is_nrv        // false
    b.compression   // 'GNU GZIP'
    b.values        // Float32Array(16203) [ 2.291, -1.483, -0.067, ... ]

``values`` is flat, in row-major order: the values of record ``i`` start at
``i * 3`` here. For any shape, the stride of the first dimension is the product of the
others:

.. code-block:: javascript

    const [nRecords, nComponents] = b.shape;
    const bz100 = b.values[100 * nComponents + 2];   // Z component of record 100: 0.915

The typed array type follows the CDF type: ``Float32Array`` for ``CDF_FLOAT``,
``Float64Array`` for ``CDF_DOUBLE``, ``Int32Array`` for ``CDF_INT4``,
``BigInt64Array`` for ``CDF_INT8``, and so on. Time variables give their raw CDF values
(``BigInt64Array`` for TT2000, ``Float64Array`` for EPOCH and EPOCH16): convert them as
shown in `Time`_. A variable without records has ``values`` set to ``undefined``.

``values`` or ``copy_values``?
------------------------------

``values`` is a **view** into WebAssembly memory. Nothing is copied, which is what makes
it fast. But the view breaks when the ``CdfFile`` is deleted, or when WebAssembly memory
grows, which can happen whenever you load another file.

To keep values around, take ``copy_values`` instead. It's an ordinary typed array that you
own:

.. code-block:: javascript

    const field = b.copy_values;   // safe to keep after cdf.delete()

Use ``values`` to process data right away, and ``copy_values`` to store it.

Strings
-------

String variables come as raw bytes (``Uint8Array``): fixed-width strings, one after the
other. The last dimension of the shape is the width. This helper turns them into
JavaScript strings:

.. code-block:: javascript

    function decodeStrings(variable) {
        const width = variable.shape[variable.shape.length - 1];
        const text = new TextDecoder();
        const strings = [];
        for (let i = 0; i < variable.values.length; i += width)
            strings.push(text.decode(variable.values.subarray(i, i + width)).replace(/\0+$/, "").trimEnd());
        return strings;
    }

    decodeStrings(cdf.get_variable("label_BGSE"));   // [ 'Bx GSE', 'By GSE', 'Bz GSE' ]

Attributes
==========

A variable's attributes are in its ``attributes`` object:

.. code-block:: javascript

    b.attributes.UNITS      // 'nT'
    b.attributes.FILLVAL    // Float32Array(1) [ -1e+31 ]
    b.attributes.VALIDMIN   // Float32Array(3) [ -65534, -65534, -65534 ]
    b.attributes.DEPEND_0   // 'Epoch'

Text attributes are strings, and numbers are typed arrays. Attributes whose CDF type is a
time type are decoded to ISO dates, in UTC:

.. code-block:: javascript

    cdf.get_variable("Epoch").attributes.VALIDMIN   // [ '1996-01-01T00:00:00.000000000' ]

``attribute_types`` gives the CDF type code of each attribute, if you need it.

Global attributes can have several entries. ``get_attribute`` returns them all:

.. code-block:: javascript

    const project = cdf.get_attribute("Project");
    project.entries   // [ 'ACE>Advanced Composition Explorer', 'ISTP>International Solar-Terrestrial Physics' ]
    project.types     // [ 51, 51 ]: CDF_CHAR

Time
====

``time_values_as_ns_since_1970`` converts a time variable (TT2000, EPOCH or EPOCH16) to
nanoseconds since 1970-01-01, in UTC, with leap seconds handled. It is the JavaScript
equivalent of numpy's ``datetime64[ns]``:

.. code-block:: javascript

    const ns = cdf.time_values_as_ns_since_1970("Epoch");   // BigInt64Array(5401)
    ns[0]                                                     // 1577836800000000000n

JavaScript ``Date`` objects have millisecond precision:

.. code-block:: javascript

    const dates = Array.from(ns, (t) => new Date(Number(t / 1_000_000n)));
    dates[0].toISOString()   // '2020-01-01T00:00:00.000Z'

For plotting, milliseconds as plain numbers are usually enough:
``Array.from(ns, (t) => Number(t / 1_000_000n))``.

It returns ``undefined`` for a variable that isn't a time variable.

Saving and converting
=====================

``save()`` serializes the file to a ``Uint8Array``. ``save_as(codec)`` does the same, with
every variable compressed with ``codec``:

.. code-block:: javascript

    const gzipped = cdf.save_as(Module.CompressionType.gzip);

    // Check the result before using it:
    const check = Module.load(gzipped);
    console.log(check.same_values(cdf));   // true
    check.delete();

In a browser, offer the result as a download:

.. code-block:: javascript
    :class: browser-only

    const link = document.createElement("a");
    link.href = URL.createObjectURL(new Blob([gzipped]));
    link.download = "ac_h0_mfi_20200101_v07_gzip.cdf";
    link.click();

The available codecs depend on how the module was built. Test for the experimental ones
before using them:

.. code-block:: javascript

    if ("zstd" in Module.CompressionType)
        console.log("this build can write zstd");

Huffman codecs are listed but not supported: ``save_as`` throws an ``Error`` for them.

Large files
===========

``Module.load`` reads variable values only when you access them, so opening is fast
even for big files. ``Module.load_eager`` decodes everything up front: use it when you will
read every variable anyway, or to measure decoding time.

The standard module, ``cdfpp.js``, can use up to 4 GiB of memory. For bigger files, build
the 64-bit variant (``--cross-file wacdfpp/wasm64.txt``, which produces ``cdfpp64.js``);
it needs a browser with WebAssembly Memory64 support (recent Chrome and Firefox). Browsers
also refuse single ``ArrayBuffer``\\ s of 2 GiB or more: ``save_as_chunks(codec, maxBytes)``
returns the result in pieces that ``new Blob(chunks)`` can join.

Decoding a big file takes time. Run it in a Web Worker so the page stays responsive: the
module works the same in a worker.

Cleaning up
===========

.. code-block:: javascript

    cdf.delete();
