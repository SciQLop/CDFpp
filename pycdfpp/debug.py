"""
pycdfpp.debug
-------------
Structured access to CDFpp's on-disk record structure - the physical layer
underneath ``load()``'s reconstructed Variable/Attribute view. Useful for building
tools like cdfdump, or diagnosing a file ``load()`` itself can't fully parse.
"""

from . import _pycdfpp

for_each_record = _pycdfpp.debug_for_each_record

__all__ = ['for_each_record']
