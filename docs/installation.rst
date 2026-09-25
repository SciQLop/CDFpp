============
Installation
============

Python
======

Install ``pycdfpp`` with pip:

.. code-block:: console

    $ python -m pip install pycdfpp

That's all. Ready-made packages (wheels) exist for:

- Python 3.9 to 3.15, including free-threaded builds.
- Linux (x86_64 and ARM64), Windows (x86_64) and macOS (Intel and Apple Silicon).

You don't need a compiler, and you don't need NASA's CDF library. The only runtime
dependencies are ``numpy`` and ``pyyaml``.

Check that it works:

.. code-block:: python

    import pycdfpp
    print(pycdfpp.__version__)

Command-line tools
------------------

The :doc:`cli` need two extra packages. Install them with the ``cli`` extra:

.. code-block:: console

    $ python -m pip install "pycdfpp[cli]"

In the browser (Pyodide, JupyterLite)
-------------------------------------

``pycdfpp`` is packaged on
`emscripten-forge <https://github.com/emscripten-forge/recipes/tree/main/recipes/recipes_emscripten/pycdfpp>`_.
You can use it in JupyterLite and other WebAssembly Python environments.

If you only want to look at a file, you don't need Python at all: open it in the
:doc:`explorer`.

From source
-----------

You need a C++20 compiler: a recent GCC or Clang, or MSVC 2022.

.. code-block:: console

    $ git clone https://github.com/SciQLop/CDFpp.git
    $ cd CDFpp
    $ python -m pip install .

The build uses `meson <https://mesonbuild.com/>`_ and downloads its own dependencies.

C++
===

CDFpp is a C++20 library, built with Meson. The easiest way to use it is as a Meson
subproject: add a ``cdfpp.wrap`` file, then ``dependency('cdfpp')``.

.. code-block:: ini

    # subprojects/cdfpp.wrap
    [wrap-git]
    url = https://github.com/SciQLop/CDFpp.git
    revision = v0.13.0
    depth = 1

CMake and other build systems can use an installed CDFpp through pkg-config. The
:ref:`cpp:Adding CDFpp to your project` section of the :doc:`cpp` explains every option.
