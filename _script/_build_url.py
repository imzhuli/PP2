#!/usr/bin/env python3

import tarfile
import os
import _cmake_util as cu
import xsetup

cwd = os.getcwd()
unzip_dir = f"{cwd}/_3rd_build"

src_file = f"{cwd}/_3rd_source/skyr_url-3.0.0.tgz"
unzipped_src_dir = f"{unzip_dir}/skyr_url-3.0.0"
install_dir = f"{cwd}/_3rd_installed"

def build():
    try:
        file = tarfile.open(src_file)
        file.extractall(unzipped_src_dir)
    except Exception as e:
        print(e)
        return False
    finally:
        file.close()

    try:
        print(f"start installing url-3.0.0..., src_dir={unzipped_src_dir}")
        os.chdir(unzipped_src_dir)
        os.system(f'cp -R include/skyr {install_dir!r}/include')
        print(f"done")
    except Exception as e:
        print(e)
        return False
    finally:
        os.chdir(cwd)
    return True


if __name__ == "__main__":
    build()
