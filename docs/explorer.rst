===============
CDFpp Explorer
===============

The `CDFpp Explorer <https://sciqlop.github.io/CDFpp/>`_ is a web app to look at CDF
files. There is nothing to install.

It runs CDFpp itself, compiled to WebAssembly, inside your browser. **Your files never
leave your computer**: nothing is uploaded to a server.

👉 `Open the CDFpp Explorer <https://sciqlop.github.io/CDFpp/>`_

What it does
============

Inspect
-------

Drop a file on the page, or paste the URL of a public CDF file and click **Fetch**.

The variables are grouped by their ISTP ``VAR_TYPE``: data, support data, metadata.
Search them by name. Click a variable to see its shape, type, attributes and a preview
of its values.

Plot
----

Time series are drawn as lines, and spectrograms as colour maps. The Explorer reads the
ISTP attributes to do it right: ``DEPEND_0`` for the time axis, ``DISPLAY_TYPE``,
``SCALETYP`` for log scales, and ``FILLVAL``, ``VALIDMIN`` and ``VALIDMAX`` to hide
invalid values. You can export the plotted data as CSV or JSON.

Validate
--------

**Validate** checks the file against the ISTP conventions with
`AstraLint <https://sciqlop.github.io/AstraLint/>`_. You get the list of problems, each
with a link to the rule. See :doc:`istp`.

Compare
-------

**Compare ↔** shows the differences between two files: added, removed or renamed
variables, and changed attributes. Changed text is highlighted word by word, like a
code review. Use it to check what changed between two versions of a data product.

Convert
-------

**Convert codec** re-compresses the file with every codec CDFpp supports, and shows the
size you get with each one. Download the version you like. See :doc:`compression`.

Export the structure
--------------------

**Export YAML** saves the file's structure and attributes, without the data. It is handy
to document a data product or to review metadata.

Linking to a file
=================

You can link directly to a file in the Explorer. Add the file's URL to the address:

- Open one file: ``https://sciqlop.github.io/CDFpp/?url=<file URL>``
- Compare two files: ``https://sciqlop.github.io/CDFpp/?a=<first URL>&b=<second URL>``

For example, `this link <https://sciqlop.github.io/CDFpp/?url=https://spdf.gsfc.nasa.gov/pub/data/ace/mag/level_2_cdaweb/mfi_h0/2020/ac_h0_mfi_20200101_v07.cdf>`_
opens the ACE file used throughout this documentation.

.. note::

   The server hosting the file must allow the browser to fetch it (CORS). NASA's SPDF
   archive does. If a URL fails, download the file and drop it on the page instead.

Large files
===========

The Explorer handles files of several gigabytes, but everything happens in your
browser's memory. Very large files may be refused, with a message explaining how much
memory they need. For those, use ``pycdfpp`` from Python.

Using CDFpp in your own web app
===============================

The WebAssembly module behind the Explorer can be embedded in other web applications,
or used from Node.js. It reads CDF files into zero-copy typed arrays, and saves them with
any codec. See :doc:`wasm`.
