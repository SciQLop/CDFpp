=================
PyCDFpp (PyCDF++)
=================

.. toctree::
   :titlesonly:
   :hidden:
   :maxdepth: 2

   generated/pycdfpp
   installation
   examples/index
   history
   contributing
   authors

A NASA's `CDF <https://cdf.gsfc.nasa.gov/>`_ modern C++ library with Python bindings thanks to `PyBind11 <https://pybind11.readthedocs.io/en/stable/>`_.
Why? CDF files are still used for space physics missions but few implementations are available.
The main one is NASA's C implementation available `here <https://cdf.gsfc.nasa.gov/>`_ but it lacks multi-threads support (global shared state), has an old C interface and has a license which isn't compatible with most Linux distributions policy.
There are also Java and Python implementations which are not usable in C++.

Quickstart
==========

Reading CDF files
-----------------

Basic example from a local file:

.. code-block:: python

    import pycdfpp
    cdf = pycdfpp.load("some_cdf.cdf")
    cdf_var_data = cdf["var_name"].values #builds a numpy view or a list of strings
    attribute_name_first_value = cdf.attributes['attribute_name'][0]


Note that you can also load in memory files:

.. code-block:: python

    import pycdfpp
    import requests
    import matplotlib.pyplot as plt
    tha_l2_fgm = pycdfpp.load(requests.get("https://spdf.gsfc.nasa.gov/pub/data/themis/tha/l2/fgm/2016/tha_l2_fgm_20160101_v01.cdf").content)
    plt.plot(tha_l2_fgm["tha_fgl_gsm"])
    plt.show()


Buffer protocol support:

.. code-block:: python

    import pycdfpp
    import requests
    import xarray as xr
    import matplotlib.pyplot as plt

    tha_l2_fgm = pycdfpp.load(requests.get("https://spdf.gsfc.nasa.gov/pub/data/themis/tha/l2/fgm/2016/tha_l2_fgm_20160101_v01.cdf").content)
    xr.DataArray(tha_l2_fgm['tha_fgl_gsm'], dims=['time', 'components'], coords={'time':tha_l2_fgm['tha_fgl_time'].values, 'components':['x', 'y', 'z']}).plot.line(x='time')
    plt.show()

    # Works with matplotlib directly too

    plt.plot(tha_l2_fgm['tha_fgl_time'], tha_l2_fgm['tha_fgl_gsm'])
    plt.show()


Datetimes handling:

.. code-block:: python

    import pycdfpp
    import os
    # Due to an issue with pybind11 you have to force your timezone to UTC for
    # datetime conversion (not necessary for numpy datetime64)
    os.environ['TZ']='UTC'

    mms2_fgm_srvy = pycdfpp.load("mms2_fgm_srvy_l2_20200201_v5.230.0.cdf")

    # to convert any CDF variable holding any time type to python datetime:
    epoch_dt = pycdfpp.to_datetime(mms2_fgm_srvy["Epoch"])

    # same with numpy datetime64:
    epoch_dt64 = pycdfpp.to_datetime64(mms2_fgm_srvy["Epoch"])

    # note that using datetime64 is ~100x faster than datetime (~2ns/element on an average laptop)


Lazy loading
------------

By default, ``pycdfpp.load`` uses lazy loading: variable metadata (name, shape, type,
attributes) is read immediately, but variable **values** are only loaded from disk when
first accessed. This makes opening large CDF files very fast when you only need a
subset of variables.

.. code-block:: python

    import pycdfpp

    # Lazy loading (default) — fast open, values loaded on demand
    cdf = pycdfpp.load("large_file.cdf")
    cdf["Epoch"].values_loaded  # False — not yet read from disk
    data = cdf["Epoch"].values  # triggers the actual read
    cdf["Epoch"].values_loaded  # True

    # Eager loading — all values read upfront
    cdf = pycdfpp.load("large_file.cdf", lazy_load=False)


Writing CDF files
-----------------

Creating a basic CDF file:

.. code-block:: python

    import pycdfpp
    import numpy as np
    from datetime import datetime

    cdf = pycdfpp.CDF()
    cdf.add_attribute("some attribute", [[1,2,3], [datetime(2018,1,1), datetime(2018,1,2)], "hello\nworld"])
    cdf.add_variable("some variable", values=np.ones((10),dtype=np.float64))
    pycdfpp.save(cdf, "some_cdf.cdf")


Working with variable attributes
---------------------------------

Variables can have their own attributes (distinct from global CDF attributes).
These are commonly used for ISTP metadata such as ``DEPEND_0``, ``FILLVAL``, ``UNITS``, etc.

.. code-block:: python

    import pycdfpp
    import numpy as np

    cdf = pycdfpp.CDF()
    var = cdf.add_variable("Flux", values=np.ones((100, 3), dtype=np.float64))

    # Add attributes to a variable
    var.add_attribute("UNITS", "cm^-2 s^-1")
    var.add_attribute("DEPEND_0", "Epoch")
    var.add_attribute("FILLVAL", [-1e31])

    # Read variable attributes
    print(var.attributes["UNITS"].value)      # "cm^-2 s^-1"
    print(list(var.attributes))               # ["UNITS", "DEPEND_0", "FILLVAL"]

    # Modify an existing attribute value
    var.attributes["UNITS"].set_value("m^-2 s^-1")


Modifying variables
-------------------

You can update the values of an existing variable using ``set_values``:

.. code-block:: python

    import pycdfpp
    import numpy as np

    cdf = pycdfpp.CDF()
    var = cdf.add_variable("data", values=np.zeros(10, dtype=np.float64))

    # Replace values (must match shape and type, unless force=True)
    var.set_values(np.ones(10, dtype=np.float64))

    # Force replacement with different shape or type
    var.set_values(np.arange(20, dtype=np.int32), force=True)


Using compression
-----------------

Variables and entire CDF files can use GZip or RLE compression:

.. code-block:: python

    import pycdfpp
    import numpy as np

    cdf = pycdfpp.CDF()

    # Per-variable compression
    cdf.add_variable("compressed_var",
                     values=np.arange(1000, dtype=np.float64),
                     compression=pycdfpp.CompressionType.gzip_compression)

    # Whole-file compression
    cdf.compression = pycdfpp.CompressionType.gzip_compression

    pycdfpp.save(cdf, "compressed.cdf")


Filtering CDF files
-------------------

Use ``CDF.filter`` to create a copy containing only the variables and attributes
you need:

.. code-block:: python

    import pycdfpp

    cdf = pycdfpp.load("large_file.cdf")

    # Keep only specific variables by name
    filtered = cdf.filter(variables=["Epoch", "Flux"])

    # Keep variables matching a regex pattern
    filtered = cdf.filter(variables="tha_fg.*")

    # Keep variables matching a callable
    filtered = cdf.filter(variables=lambda v: v.type == pycdfpp.DataType.CDF_DOUBLE)

    # Keep specific global attributes
    filtered = cdf.filter(variables=["Epoch"], attributes=["Project", "Source_name"])

    # Filter in-place (modifies the original)
    cdf.filter(variables=["Epoch"], inplace=True)


Exporting to dictionary
------------------------

Use ``pycdfpp.to_dict_skeleton`` to export the structure of a CDF, variable, or attribute
as a plain dictionary (useful for JSON serialization or inspection):

.. code-block:: python

    import pycdfpp
    import json

    cdf = pycdfpp.load("some_cdf.cdf")

    # Export full CDF structure (without variable data)
    skeleton = pycdfpp.to_dict_skeleton(cdf)
    print(json.dumps(skeleton, indent=2, default=str))

    # Export a single variable's metadata
    var_info = pycdfpp.to_dict_skeleton(cdf["some_var"])


FAQ
===

How to control integer attributes types?
----------------------------------------

Use numpy types to control the type of integer attributes:

.. code-block:: python

    import pycdfpp
    import numpy as np

    cdf = pycdfpp.CDF()
    cdf.add_attribute("int8 attribute",  np.array([[1, 2, 3]], dtype=np.int8))
    cdf.add_attribute("int16 attribute", np.array([[1, 2, 3]], dtype=np.int16))
    cdf.add_attribute("int32 attribute", np.array([[1, 2, 3]], dtype=np.int32))
    cdf.add_attribute("int64 attribute", np.array([[1, 2, 3]], dtype=np.int64))

    # or:
    cdf.add_attribute("int8 attribute2", [[np.int8(1)]])

Why do I get a segfault when accessing attributes after modifying a CDF?
------------------------------------------------------------------------

PyCDFpp returns lightweight references into the underlying C++ data structures.
Adding or removing variables or attributes may reallocate internal storage, which
invalidates any previously obtained reference. Always re-fetch references after
mutating the CDF:

.. code-block:: python

    import pycdfpp
    import numpy as np

    cdf = pycdfpp.CDF()
    cdf.add_variable("var1", values=np.ones(10))
    var1 = cdf["var1"]

    # UNSAFE — var1 may be invalidated by the next add_variable call
    cdf.add_variable("var2", values=np.zeros(5))
    # var1.values  # may segfault!

    # SAFE — re-fetch after mutation
    var1 = cdf["var1"]
    var1.values  # ok

The same applies to attribute references: do not hold onto a reference returned by
``var.attributes["name"]`` while adding or removing attributes on the same variable.

How to make special values (FILLVAL, Pad values, etc.)?
-------------------------------------------------------

Use the `pycdfpp.default_fill_value` and `pycdfpp.default_pad_value` functions to create Fill or Pad values depending on the CDF type:

.. code-block:: python

    import pycdfpp
    import numpy as np

    cdf = pycdfpp.CDF()
    cdf.add_variable("int8 variable", values=np.ones((10), dtype=np.int8), attributes={"FILLVAL": [pycdfpp.default_fill_value(pycdfpp.DataType.CDF_INT1)]})
    cdf.add_variable("tt2000 variable", values=np.ones((10), dtype=np.int64), attributes={"FILLVAL": [pycdfpp.default_fill_value(pycdfpp.DataType.CDF_TIME_TT2000)]})
