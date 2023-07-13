import sys
import os
__here__ = os.path.dirname(os.path.abspath(__file__))
sys.path.append(__here__)
if sys.platform == 'win32' and sys.version_info[0] == 3 and sys.version_info[1] >= 8:
    os.add_dll_directory(__here__)

from .cdfpp_wrapper import *
