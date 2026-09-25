========
Cookbook
========

Short recipes for common tasks. Each one runs as-is. They all use the ACE file from the
:doc:`quickstart`:

.. code-block:: python

    import urllib.request
    import numpy as np
    import pycdfpp

    url = ("https://spdf.gsfc.nasa.gov/pub/data/ace/mag/level_2_cdaweb/"
           "mfi_h0/2020/ac_h0_mfi_20200101_v07.cdf")
    urllib.request.urlretrieve(url, "ac_h0_mfi_20200101_v07.cdf")
    cdf = pycdfpp.load("ac_h0_mfi_20200101_v07.cdf")

Replace fill values with NaN
============================

Invalid measurements hold the ``FILLVAL`` value, or a value outside
``VALIDMIN``/``VALIDMAX``. Turn them into ``NaN`` before you compute anything, or a
single fill value of ``-1e31`` will ruin your average.

.. code-block:: python

    def clean_values(var):
        """Values of `var` as floats, with fill and out-of-range values set to NaN."""
        values = var.values.astype(np.float64)
        attrs = var.attributes
        invalid = np.zeros(values.shape, dtype=bool)
        if "FILLVAL" in attrs:
            invalid |= values == attrs["FILLVAL"].value[0]
        if "VALIDMIN" in attrs:
            invalid |= values < np.asarray(attrs["VALIDMIN"].value)
        if "VALIDMAX" in attrs:
            invalid |= values > np.asarray(attrs["VALIDMAX"].value)
        values[invalid] = np.nan
        return values

    b = clean_values(cdf["BGSEc"])
    print(np.nanmean(b, axis=0))

``VALIDMIN`` and ``VALIDMAX`` can hold one value per component. ``np.asarray`` makes them
compare component by component.

.. note::

   ``FILLVAL`` is stored with the variable's own type. For a ``float32`` variable,
   ``-1e31`` is rounded to about ``-9.9999998e+30``. Comparing the ``float32`` values to
   the ``float32`` fill value, as above, avoids any rounding trouble.

Make an xarray DataArray
========================

`xarray <https://xarray.dev/>`_ keeps the values, the time axis, the labels and the
units together. Here is a function that builds a :class:`xarray.DataArray` from any
ISTP time series variable:

.. code-block:: python

    import xarray as xr

    def to_xarray(cdf, name):
        var = cdf[name]
        attrs = var.attributes
        time = pycdfpp.to_datetime64(cdf[attrs["DEPEND_0"].value]).ravel()
        values = var.values.reshape(len(time), -1)
        coords = {"time": time}
        dims = ["time", "component"]
        if "LABL_PTR_1" in attrs:
            coords["component"] = cdf[attrs["LABL_PTR_1"].value].values_encoded[0]
        return xr.DataArray(
            values, dims=dims, coords=coords, name=name,
            attrs={key: attrs[key].value for key in ("UNITS", "CATDESC") if key in attrs},
        )

    bgse = to_xarray(cdf, "BGSEc")
    print(bgse.sel(component="Bz GSE").mean().item())

With xarray, plotting is one line: ``bgse.plot.line(x="time")``.

Make a pandas DataFrame
=======================

.. code-block:: python

    import pandas as pd

    time = pycdfpp.to_datetime64(cdf["Epoch"]).ravel()
    labels = cdf["label_BGSE"].values_encoded[0]
    df = pd.DataFrame(cdf["BGSEc"].values, index=time, columns=labels)
    df["|B|"] = cdf["Magnitude"].values.ravel()

    print(df.resample("1h").mean().head())

Join several daily files
========================

Archives usually store one file per day. Load each file, then concatenate the arrays:

.. code-block:: python

    base = ("https://spdf.gsfc.nasa.gov/pub/data/ace/mag/level_2_cdaweb/"
            "mfi_h0/2020/ac_h0_mfi_202001{day:02d}_v07.cdf")

    def load_url(url):
        with urllib.request.urlopen(url) as response:
            return pycdfpp.load(response.read())

    days = [load_url(base.format(day=day)) for day in (1, 2, 3)]

    time = np.concatenate([pycdfpp.to_datetime64(d["Epoch"]).ravel() for d in days])
    b = np.concatenate([d["BGSEc"].values for d in days])
    print(time[0], time[-1], b.shape)

.. tip::

   Don't want to deal with file names, versions and URLs? `Speasy
   <https://speasy.readthedocs.io/>`_ fetches data from CDAWeb and other archives by
   product name and time range. It uses ``pycdfpp`` under the hood.

Export a variable to CSV
========================

.. code-block:: python
    :class: no-playground

    time = pycdfpp.to_time_string(cdf["Epoch"], "%Y-%m-%dT%H:%M:%SZ").ravel().astype(str)
    b = cdf["BGSEc"].values

    with open("bgse.csv", "w") as f:
        f.write("time,bx,by,bz\n")
        for t, (bx, by, bz) in zip(time, b):
            f.write(f"{t},{bx},{by},{bz}\n")

Save a smaller copy of a file
=============================

Keep only the variables you need, then save. ``DEPEND_0`` and label variables are not
added automatically, so list them too:

.. code-block:: python

    small = cdf.filter(variables=["Epoch", "BGSEc", "label_BGSE"], attributes=".*")
    pycdfpp.save(small, "ace_bgse_only.cdf")

Find every time variable in a file
==================================

.. code-block:: python

    time_types = {pycdfpp.DataType.CDF_EPOCH, pycdfpp.DataType.CDF_EPOCH16,
                  pycdfpp.DataType.CDF_TIME_TT2000}
    print([name for name, var in cdf.items() if var.type in time_types])   # ['Epoch']
