====================
cdflib compatibility
====================

``pycdfpp.cdflib`` offers the API of `cdflib <https://github.com/MAVENSDC/cdflib>`_ 1.3.16
on top of ``pycdfpp``.

Differences from cdflib
=======================

Where cdflib and NASA's CDF library disagree, ``pycdfpp.cdflib`` follows NASA. Where cdflib
fails, ``pycdfpp.cdflib`` returns the right value.

Time conversions (``cdfepoch``)
-------------------------------

- ``to_datetime`` of ``CDF_EPOCH`` values keeps the fraction of a millisecond the value holds.
  cdflib cuts it off.
- ``to_datetime`` of ``CDF_EPOCH`` fill values (``-1e31``) gives ``NaT``, as it does for the
  other time types. cdflib raises ``OverflowError`` for the whole array. Real dates
  ``datetime64[ns]`` can't hold (before 1678 or after 2262) still raise ``OverflowError``, as in
  cdflib.
- The first leap second, 1972-06-30T23:59:60, breaks down as second 60, like every other leap
  second. cdflib shows it as 23:59:59, the same as the second before it. ``encode``,
  ``to_datetime`` and ``unixtime`` follow.
- ``compute_epoch`` of the fill date (9999-12-31 23:59:59.999) gives the fill value ``-1e31``,
  as NASA's ``computeEPOCH`` does. For a single value cdflib returns the date's millisecond count,
  ``315569519999999.0``, not the fill value. For a list it adds an extra ``-1e31`` element.
  Out of range components (hour 25, ...) give one value each;
  cdflib adds extra elements.
- ``encode_tt2000`` of a list holding a fill or pad value encodes every value. cdflib returns
  only the fill or pad string for the whole list.
