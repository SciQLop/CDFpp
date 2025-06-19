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


Writing CDF files
-----------------

Creating a basic CDF file:

.. code-block:: python

    import pycdfpp
    import numpy as np
    from datetime import datetime

    cdf = pycdfpp.CDF()
    cdf.add_attribute("some attribute", [[1,2,3], [datetime(2018,1,1), datetime(2018,1,2)], "hello\nworld"])
    cdf.add_variable(f"some variable", values=np.ones((10),dtype=np.float64))
    pycdfpp.save(cdf, "some_cdf.cdf")



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

How to make special values (FILLVAL, Pad valuse, etc.)?
-------------------------------------------------------

Use the `pycdfpp.default_fill_value` and `pycdfpp.default_pad_value` functions to create Fill or Pad values depending on the CDF type:

.. code-block:: python

    import pycdfpp
    import numpy as np

    cdf = pycdfpp.CDF()
    cdf.add_variable("int8 variable", values=np.ones((10), dtype=np.int8), attributes={"FILLVAL": [pycdfpp.default_fill_value(pycdfpp.DataType.CDF_INT1)]})
    cdf.add_variable("tt2000 variable", values=np.ones((10), dtype=np.int64), attributes={"FILLVAL": [pycdfpp.default_fill_value(pycdfpp.DataType.CDF_TIME_TT2000)]})
