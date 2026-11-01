"""cdflib 1.3.16 compatible API on top of pycdfpp."""
try:
    import cdflib as _real_cdflib  # noqa: F401
except ImportError as error:
    raise ImportError("pycdfpp.cdflib needs cdflib: pip install pycdfpp[cdflib]") from error

__version__ = "1.3.16"

from . import cdfread, cdfwrite  # noqa: E402
from .cdfread import CDF  # noqa: E402
from .epochs import CDFepoch as cdfepoch  # noqa: E402

__all__ = ["CDF"]
