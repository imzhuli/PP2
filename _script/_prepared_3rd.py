#!/usr/bin/env python3

import _check_env as ce


def prepare_3rd():
    if not ce.check_env():
        return False
    if not ce.remake_dirs():
        return False
    return True
