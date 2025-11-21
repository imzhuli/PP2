#!/usr/bin/env python3

import sys
import os
import shutil

MIN_PY_VERSION_MAJOR = 3
MIN_PY_VERSION_MINOR = 7

cwd = os. getcwd()
build_dir = cwd + "/_build"
install_dir = cwd + "/_install"
cpp_dir = cwd + "/cpp"


def check_version():
    py_version_major = sys.version_info[0]
    py_version_minor = sys.version_info[1]
    if py_version_major < MIN_PY_VERSION_MAJOR:
        print("invalid python major version: at least %u" %
              MIN_PY_VERSION_MAJOR)
        return False
    if py_version_major == MIN_PY_VERSION_MAJOR and py_version_minor < MIN_PY_VERSION_MINOR:
        print("invalid python ninor version: at least %u" %
              MIN_PY_VERSION_MINOR)
        return False
    return True


def check_tag_file():
    tag_filename = cwd + "/CoreXApp.tag"
    if not os.path.isfile(tag_filename):
        print("tag file not found, please check the working directory")
        return False
    return True


def check_env():
    valid = check_version() and check_tag_file()
    if not valid:
        print("Failed to pass all env requirements")
        return False
    return True


def remake_dir(dir):
    try:
        shutil.rmtree(dir)
    except Exception as e:
        pass
    try:
        os.mkdir(dir)
    except Exception as e:
        return False
    return True


def remake_dirs():
    if not remake_dir(build_dir):
        print("Failed to remake build dir")
        return False
    if not remake_dir(install_dir):
        print("Failed to remake install dir")
        return False
    return True
