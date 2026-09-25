=============
API reference
=============

.. currentmodule:: pycdfpp

This page lists everything ``pycdfpp`` offers. For explanations and examples, see
:doc:`reading` and :doc:`writing`.

Loading and saving
==================

.. autofunction:: load

.. autofunction:: save

Files, variables, attributes
============================

.. autoclass:: CDF
    :members: add_variable, add_attribute, filter, items, keys, attributes, compression,
              majority, distribution_version, lazy_loaded

.. autoclass:: Variable
    :members: name, type, shape, values, values_encoded, values_loaded, attributes,
              add_attribute, set_values, is_nrv, is_zvariable, compression, majority,
              is_contiguous

.. autoclass:: Attribute
    :members: name, type, set_values

.. autoclass:: VariableAttribute
    :members: name, value, type, set_value

Time
====

.. autofunction:: to_datetime64

.. autofunction:: to_datetime

.. autofunction:: to_time_string

.. autofunction:: to_tt2000

.. autofunction:: to_epoch

.. autofunction:: to_epoch16

.. autoclass:: tt2000_t
    :members: nseconds

.. autoclass:: epoch
    :members: mseconds

.. autoclass:: epoch16
    :members: seconds, picoseconds

Helpers
=======

.. autofunction:: default_fill_value

.. autofunction:: default_pad_value

.. autofunction:: to_dict_skeleton

Enumerations
============

.. autoclass:: DataType
    :members:
    :undoc-members:

.. autoclass:: CompressionType
    :members:
    :undoc-members:

.. autoclass:: Majority
    :members:
    :undoc-members:

Warnings
========

.. autoclass:: ExperimentalCompressionWarning

Low-level inspection
====================

.. automodule:: pycdfpp.debug
    :members:
