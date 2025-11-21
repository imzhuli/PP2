#!/usr/bin/env python3

import tarfile
import os
import _cmake_util as cu
import xsetup

cwd = os.getcwd()
unzip_dir = f"{cwd}/_3rd_build"

src_file = f"{cwd}/_3rd_source/librdkafka-v2.10.0.tar.gz"
unzipped_src_dir = f"{unzip_dir}/librdkafka-2.10.0"
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
        os.system(f'./configure --prefix={install_dir!r} --enable-static --disable-lz4 --disable-zlib --disable-zstd --disable-syslog --disable-shared')
        os.system(f"make -j 8")
        os.system(f"make install")
    except Exception as e:
        print(e)
        return False
    finally:
        os.chdir(cwd)
    return True


if __name__ == "__main__":
    build()
