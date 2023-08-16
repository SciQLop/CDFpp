#!/usr/bin/env python
import os
import sys
import subprocess

src_root = os.environ.get('SRC_ROOT', '.')

def version_from_git():
    #try:
    #    version = subprocess.run(['git', 'describe', '--tags'], capture_output=True).stdout.decode().strip()[1:]
    #    if version.startswith('v'):
    #        version = version[1:]
    #    return version
    #except:
    #    return ""
    return ""

def version_from_file():
    try:
        version_file = f"{os.environ.get('SRC_ROOT', '.')}/version.txt"
        version = ""
        if os.path.exists(version_file):
            with open(version_file, 'r') as f:
                version = f.read().strip()
        return version
    except:
        return ""

def update_file_version(version):
    version_file = f"{os.environ.get('SRC_ROOT', '.')}/version.txt"
    with open(version_file, 'w') as f:
        f.write(version)

if __name__ == '__main__':
    git_version = version_from_git()
    file_version = version_from_file()
    if git_version != "":
        print(git_version)
        if git_version != file_version:
            update_file_version(git_version)
    elif file_version != "":
        print(file_version)
    else:
        sys.exit("can't find any version")


