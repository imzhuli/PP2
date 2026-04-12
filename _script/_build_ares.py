#!/usr/bin/env python3

import tarfile
import os
import _cmake_util as cu
import xsetup

cwd = os.getcwd()
unzip_dir = f"{cwd}/_3rd_build"

src_file = f"{cwd}/_3rd_source/c-ares-1.34.6.tar.gz"
unzipped_src_dir = f"{unzip_dir}/c-ares-1.34.6"
install_dir = f"{cwd}/_3rd_installed"


def build():
    try:
        file = tarfile.open(src_file)
        file.extractall(unzip_dir)
    except Exception as e:
        print(e)
        return False
    finally:
        file.close()

    try:
        print(f"start building..., src_dir={unzipped_src_dir}")
        os.chdir(unzipped_src_dir)
        os.system(f'./configure --prefix={install_dir!r} --enable-static --disable-shared')
        os.system(f"make -j 8")
        os.system(f"make install")
        os.system(f"rm {install_dir!r}/lib/libcares.la")
    except Exception as e:
        print(e)
        return False
    finally:
        os.chdir(cwd)
    return True


if __name__ == "__main__":
    build()
