=================
Working with time
=================

CDF has three time types. :doc:`concepts` explains them. The short version:

- ``CDF_TIME_TT2000``: nanoseconds, counts leap seconds. **Use it for new files.**
- ``CDF_EPOCH``: milliseconds. Common in older files.
- ``CDF_EPOCH16``: picoseconds. Rare.

``pycdfpp`` converts all three for you. Your code doesn't need to know which one a file
uses.

.. tip::

   Work with numpy ``datetime64[ns]`` whenever you can. It is the fastest option, it
   plays well with numpy, pandas, xarray and matplotlib, and it has no timezone
   surprises.

From CDF time to Python
=======================

To numpy datetime64
-------------------

:func:`pycdfpp.to_datetime64` takes a time variable and returns a ``datetime64[ns]``
array of the same shape:

.. code-block:: python

    import urllib.request
    import pycdfpp

    url = ("https://spdf.gsfc.nasa.gov/pub/data/ace/mag/level_2_cdaweb/"
           "mfi_h0/2020/ac_h0_mfi_20200101_v07.cdf")
    urllib.request.urlretrieve(url, "ac_h0_mfi_20200101_v07.cdf")
    cdf = pycdfpp.load("ac_h0_mfi_20200101_v07.cdf")

    time = pycdfpp.to_datetime64(cdf["Epoch"])
    time.dtype      # dtype('<M8[ns]')
    time.shape      # (5401, 1): same shape as the variable
    time = time.ravel()

It is fast: about 2 nanoseconds per value. On recent x86 processors it uses SIMD
instructions to convert several values at once.

To Python datetime
------------------

:func:`pycdfpp.to_datetime` returns a list of :class:`datetime.datetime` objects. They
hold UTC times, whatever your computer's timezone, and have no ``tzinfo``:

.. code-block:: python

    first_times = pycdfpp.to_datetime(cdf["Epoch"].values.ravel()[:3])

It is about 100 times slower than :func:`pycdfpp.to_datetime64`. Use it only for a few
values, for display for example. For a multi-dimensional variable, like this ``(5401, 1)``
``Epoch``, it returns nested lists; ``.values.ravel()`` flattens the input first.

To text
-------

:func:`pycdfpp.to_time_string` formats times as fixed-width strings. It uses the
``strftime`` codes you already know. ``%S`` includes the fraction of a second:

.. code-block:: python

    pycdfpp.to_time_string(cdf["Epoch"], "%Y-%m-%dT%H:%M:%SZ")[:2]
    # [[b'2020-01-01T00:00:00.000000000Z']
    #  [b'2020-01-01T00:00:15.000000000Z']]

    pycdfpp.to_time_string(cdf["Epoch"], "%Y/%j")[0]    # [b'2020/001']: day of year

It returns bytes, which is compact for large arrays. Call ``.astype(str)`` on the result
for Python strings.

Single values
-------------

Each time type has a small Python class: :class:`pycdfpp.tt2000_t`,
:class:`pycdfpp.epoch` and :class:`pycdfpp.epoch16`. Attribute values and
:func:`pycdfpp.default_fill_value` return them. They print as readable dates, and the
conversion functions accept them:

.. code-block:: python

    t0 = pycdfpp.epoch(63745056000000.0)   # milliseconds since year 0
    print(t0)                              # 2020-01-01T00:00:00.000000000
    pycdfpp.to_datetime64(t0)              # array('2020-01-01T00:00:00.000000000', dtype='datetime64[ns]')

From Python to CDF time
=======================

When writing a file
-------------------

You don't need to convert anything. Pass ``datetime64`` values to
:meth:`pycdfpp.CDF.add_variable` and they are stored as TT2000:

.. code-block:: python

    import numpy as np

    out = pycdfpp.CDF()
    time = np.arange("2024-03-01", "2024-03-02", np.timedelta64(10, "m"),
                     dtype="datetime64[ns]")
    out.add_variable("Epoch", values=time)
    out["Epoch"].type        # DataType.CDF_TIME_TT2000

To store another time type, say so with ``data_type``:

.. code-block:: python

    out.add_variable("Epoch_ms", values=time, data_type=pycdfpp.DataType.CDF_EPOCH)

Lists of :class:`datetime.datetime` work too.

Explicit conversions
--------------------

:func:`pycdfpp.to_tt2000`, :func:`pycdfpp.to_epoch` and :func:`pycdfpp.to_epoch16`
convert ``datetime64`` arrays, or lists of ``datetime``, to CDF time values:

.. code-block:: python

    from datetime import datetime

    pycdfpp.to_tt2000(np.array(["2024-03-01T12:00"], dtype="datetime64[ns]"))
    pycdfpp.to_epoch([datetime(2024, 3, 1, 12)])

A :class:`datetime.datetime` without a timezone ("naive") is taken as UTC, whatever
your computer's timezone. One with a timezone is converted to UTC.

Leap seconds
============

TT2000 counts leap seconds. EPOCH and EPOCH16 don't. ``pycdfpp`` has the leap second
table built in and applies it when converting TT2000 values.

Numpy ``datetime64`` has no leap seconds: it cannot show ``23:59:60``. A value that falls
inside a leap second is shown as ``23:59:59``.

Fill values in time variables
=============================

Time variables have fill values too. For TT2000 the standard fill value is the largest
negative 64-bit integer. It reads as ``9999-12-31T23:59:59.999999999``. If you see that
date, the record has no valid time.

.. code-block:: python

    pycdfpp.default_fill_value(pycdfpp.DataType.CDF_TIME_TT2000)
    # 9999-12-31T23:59:59.999999999
