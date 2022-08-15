#!/usr/bin/env python
from mesonpy import build_wheel, build_sdist
import os

if not os.path.exists('dist'):
    os.mkdir('dist')

build_wheel('dist')
