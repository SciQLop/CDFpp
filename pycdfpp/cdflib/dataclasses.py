# Alias the real module so isinstance checks hold and every public name is kept (a star import only follows __all__).
import sys

import cdflib.dataclasses

sys.modules[__name__] = cdflib.dataclasses
