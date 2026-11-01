# Alias the real module: epochs_astropy.__all__ omits CDFEpoch, CDFEpoch16, CDFTT2000, so a star import would lose public names.
import sys

import cdflib.epochs_astropy

sys.modules[__name__] = cdflib.epochs_astropy
