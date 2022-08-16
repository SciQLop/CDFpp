#!/usr/bin/env python
import os
from glob import glob
import sys
import subprocess
from distutils.core import Extension
from setuptools.dist import Distribution

def wheel_name(name, version):
    # taken from https://stackoverflow.com/questions/60643710/setuptools-know-in-advance-the-wheel-filename-of-a-native-library
    fuzzlib = Extension('fuzzlib', ['fuzz.pyx'])  # the files don't need to exist
    dist = Distribution(attrs={'name': name, 'version': version, 'ext_modules': [fuzzlib]})
    bdist_wheel_cmd = dist.get_command_obj('bdist_wheel')
    bdist_wheel_cmd.ensure_finalized()

    distname = bdist_wheel_cmd.wheel_dist_name
    tag = '-'.join(bdist_wheel_cmd.get_tag())
    return f'{distname}-{tag}.whl'

def build_wheel():
    subprocess.run([sys.executable, "-m", "build", "."])

def fix_wheel_name(wheel):
    name, version = wheel.split('-')[:2]
    new_name = wheel_name(name, version)
    print(f"New wheel name: dist{os.path.sep}{new_name}")
    os.rename(f'dist{os.path.sep}{wheel}', f'dist{os.path.sep}{new_name}')

if __name__ == '__main__':

    wheels_before = glob('dist/*.whl')
    build_wheel()
    wheels_after = glob('dist/*.whl')
    diff = list(set(wheels_after) - set(wheels_before))

    if len(diff) == 1:
        print(f"Found one wheel to rename {diff[0]}")
        fix_wheel_name(os.path.basename(diff[0]))
