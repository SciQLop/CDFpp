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

CDFpp is a C++20 library, made mostly of headers. To use it in your program, you add its
``include/`` folder and three small header-only dependencies to your include path.

The :ref:`cpp:Adding CDFpp to your project` section of the :doc:`cpp` gives the exact
compiler flags, and the current packaging limitations.
