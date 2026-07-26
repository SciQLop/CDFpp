"""
pycdfpp.debug
-------------
Structured access to CDFpp's on-disk record structure - the physical layer
underneath ``load()``'s reconstructed Variable/Attribute view. Useful for building
tools like cdfdump, or diagnosing a file ``load()`` itself can't fully parse.
"""

from . import _pycdfpp

for_each_record = _pycdfpp.debug_for_each_record
nasa_compat_dump = _pycdfpp.nasa_compat_dump
nasa_compat_dump_from_offset = _pycdfpp.nasa_compat_dump_from_offset
nasa_compat_dump_brief = _pycdfpp.nasa_compat_dump_brief

__all__ = ['for_each_record', 'nasa_compat_dump', 'nasa_compat_dump_from_offset',
           'nasa_compat_dump_brief']
