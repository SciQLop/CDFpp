=======
History
=======

0.6.1 (2023-12-05)
------------------

* Fix writing empty attributes strings (ensure numElements cdf fields > 0)
* set_values assume values=[] if only data_type is provided
* Attributes values reset, user can now change attributes values once set

0.6.0 (2023-10-18)
------------------

* Fixes + Unfinished skeletons export
* Adds Python 3.12 support.
* Builds with O3 optimizations instead of O2.
* Always expose record count as first dimension (even with NRV variables)

0.5.0 (2023-09-06)
------------------

* Add support for writing CDF files.
* Add support for lazy loading variables (default behavior now).
* Read performances improvements.
* Exposes string variables values as numpy array of unencoded strings by default (.values).
* Add support for encoding string variables values (.values_encoded).
* Exposes CDF version, compression, majority,...

0.4.6 (2023-06-22)
------------------

* Fixes Windows 'access violation' error.


0.4.5 (2023-06-21)
------------------

* Mostly CI refactoring.


0.4.4 (2023-06-19)
------------------

* Packaging fix.
