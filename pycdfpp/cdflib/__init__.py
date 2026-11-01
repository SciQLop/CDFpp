"""cdflib 1.3.16 compatible API on top of pycdfpp."""
try:
    import cdflib as _real_cdflib  # noqa: F401
except ImportError as error:
    raise ImportError("pycdfpp.cdflib needs cdflib: pip install pycdfpp[cdflib]") from error

__version__ = "1.3.16"

import importlib  # noqa: E402
import sys  # noqa: E402

from . import cdfread, cdfwrite  # noqa: E402
from .cdfread import CDF  # noqa: E402
from .epochs import CDFepoch as cdfepoch  # noqa: E402

__all__ = ["CDF", "install"]

OUR_SUBMODULES = ("cdfread", "cdfwrite", "epochs", "s3", "utils", "logging")
OUR_XARRAY_SUBMODULES = ("xarray", "xarray.cdf_to_xarray", "xarray.xarray_to_cdf")
REAL_SUBMODULES = ("dataclasses", "epochs_astropy")


def _import_submodules(names):
    return {f"cdflib.{name}": importlib.import_module(f"{__name__}.{name}") for name in names}


def _our_xarray_modules():
    try:
        return _import_submodules(OUR_XARRAY_SUBMODULES)
    except ImportError:  # xarray is not installed: cdflib.xarray can't be imported either
        return {}


def install():
    """Make ``import cdflib`` give this package, for code that imports cdflib itself.

    Run it before any other library imports cdflib: modules that already hold the real one
    keep it. ``cdflib.dataclasses`` and ``cdflib.epochs_astropy`` stay the real modules.
    Calling it again does nothing."""
    this = sys.modules[__name__]
    real = _import_submodules(REAL_SUBMODULES)
    ours = {"cdflib": this, **_import_submodules(OUR_SUBMODULES), **_our_xarray_modules()}
    sys.modules.update(real | ours)
    vars(this).update({name.removeprefix("cdflib."): module for name, module in real.items()})
