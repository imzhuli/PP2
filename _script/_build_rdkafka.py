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
    finally:
        file.close()

    cmake_file = f"{unzipped_src_dir}/CMakeLists.txt"
    if not cu.fix_cmake(cmake_file):
        return False

    try:
        os.chdir(unzipped_src_dir)
        os.system(
            "cmake "
            f"{xsetup.cmake_build_type} "
            "-Wno-dev "
            "-DCMAKE_EXPORT_COMPILE_COMMANDS=1 "
            "-DRDKAFKA_BUILD_STATIC=ON "
            "-DBUILD_SHARED_LIBS=OFF "
            "-DENABLE_LZ4_EXT=OFF "
            "-DWITH_ZLIB=OFF "
            "-DWITH_ZSTD=OFF "
            "-DRDKAFKA_BUILD_TESTS=OFF -DRDKAFKA_BUILD_EXAMPLES=OFF "
            "-DCMAKE_CXX_STANDARD=20 "
            f"-DCMAKE_INSTALL_PREFIX={install_dir!r} -B build ."
        )
        os.system(f"cmake --build build {xsetup.cmake_build_config} -- all")
        os.system(f"cmake --install build {xsetup.cmake_build_config}")
    except Exception as e:
        return False
    finally:
        os.chdir(cwd)
    return True


def build_traditional():
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
