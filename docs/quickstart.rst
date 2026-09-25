==========
Quickstart
==========

This page takes five minutes. You will read a real CDF file, plot it, and write a new
one. Every example on this page runs as-is: copy it into Python and try it.

Get a file
==========

We use one day of magnetic field data from NASA's ACE spacecraft. The file is small
(250 kB) and comes from NASA's public archive,
`SPDF <https://spdf.gsfc.nasa.gov/>`_.

.. code-block:: python

    import urllib.request

    url = ("https://spdf.gsfc.nasa.gov/pub/data/ace/mag/level_2_cdaweb/"
           "mfi_h0/2020/ac_h0_mfi_20200101_v07.cdf")
    urllib.request.urlretrieve(url, "ac_h0_mfi_20200101_v07.cdf")

Open it
=======

.. code-block:: python

    import pycdfpp

    cdf = pycdfpp.load("ac_h0_mfi_20200101_v07.cdf")
    print(cdf)

``print`` shows a summary: the file's global attributes (information about the whole
file), then each variable with its shape, type and attributes. Here is the start:

.. code-block:: text

    CDF:
      version: 3.7.1
      majority: column
      compression: None

    Attributes:
      TITLE: "ACE> Magnetometer Parameters"
      Project: [ [ "ACE>Advanced Composition Explorer", "ISTP>International Solar-Terrestrial Physics" ] ]
      ...

Opening is instant, even for big files. CDFpp reads the data of a variable only when
you ask for it.

See what's inside
=================

A CDF file holds **variables**. You use it like a dictionary of variables:

.. code-block:: python

    print(list(cdf))          # all variable names
    print("BGSEc" in cdf)     # True

    for name, var in cdf.items():
        print(name, var.shape, var.type)

.. code-block:: text

    Epoch (5401, 1) DataType.CDF_EPOCH
    Magnitude (5401, 1) DataType.CDF_REAL4
    BGSEc (5401, 3) DataType.CDF_REAL4
    ...

``BGSEc`` is the magnetic field vector. It has 5401 **records** (one every 16 seconds)
of 3 values each: the X, Y and Z components.

Get the data
============

``.values`` gives you a numpy array:

.. code-block:: python

    b = cdf["BGSEc"].values
    print(b.shape, b.dtype)   # (5401, 3) float32

The time is in the ``Epoch`` variable. Convert it to numpy ``datetime64``:

.. code-block:: python

    time = pycdfpp.to_datetime64(cdf["Epoch"])
    print(time[0])            # ['2020-01-01T00:00:00.000000000']

Read the metadata
=================

Each variable has its own **attributes**. They tell you what the data means:

.. code-block:: python

    bgse = cdf["BGSEc"]
    print(bgse.attributes["CATDESC"].value)   # Magnetic Field Vector in GSE Cartesian coordinates (16 sec)
    print(bgse.attributes["UNITS"].value)     # nT
    print(bgse.attributes["DEPEND_0"].value)  # Epoch  <- the time variable to use

Global attributes describe the whole file:

.. code-block:: python

    print(cdf.attributes["TITLE"][0])         # ACE> Magnetometer Parameters

Plot it
=======

.. code-block:: python

    import matplotlib.pyplot as plt

    plt.plot(time.ravel(), b, label=["Bx", "By", "Bz"])
    plt.ylabel(bgse.attributes["UNITS"].value)
    plt.legend()
    plt.gcf().autofmt_xdate()
    plt.show()

Write a file
============

Now the other way around. Build a CDF in memory, then save it:

.. code-block:: python

    import numpy as np

    out = pycdfpp.CDF()
    out.add_attribute("Project", ["My mission"])

    time = np.arange("2024-01-01", "2024-01-02", np.timedelta64(1, "h"),
                     dtype="datetime64[ns]")
    out.add_variable("Epoch", values=time)
    out.add_variable("Temperature",
                     values=np.linspace(20.0, 25.0, len(time)),
                     attributes={"UNITS": "degC", "DEPEND_0": "Epoch"})

    pycdfpp.save(out, "my_first.cdf")

And read it back:

.. code-block:: python

    check = pycdfpp.load("my_first.cdf")
    print(check["Temperature"].values[:3])   # [20.    20.2173913  20.43478261]

Next steps
==========

- New to CDF? :doc:`concepts` explains records, attributes and time types in plain words.
- Want more on reading? Go to :doc:`reading` and :doc:`cookbook`.
- Producing files for a mission? Go to :doc:`writing`, then :doc:`istp`.
