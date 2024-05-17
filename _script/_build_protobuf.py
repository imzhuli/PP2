#!/usr/bin/env python3

import tarfile
import os
import _cmake_util as cu
import xsetup

cwd = os.getcwd()
unzip_dir = f"{cwd}/_3rd_build"

src_file = f"{cwd}/_3rd_source/protobuf-25.3.tar.gz"
unzipped_src_dir = f"{unzip_dir}/protobuf-25.3"
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
        abseil_dir = unzip_dir + "/abseil-cpp-20240116.1"
        os.chdir(unzipped_src_dir)
        os.system(
            'cmake '
            f'{xsetup.cmake_build_type} '
            '-Wno-dev '
            '-DBUILD_SHARED_LIBS=OFF '
            '-Dprotobuf_BUILD_TESTS=OFF '
            '-DCMAKE_CXX_STANDARD=20 '
            '-Dprotobuf_ABSL_PROVIDER=package '
            f'-DCMAKE_INSTALL_PREFIX={install_dir!r} -B build .')
        os.system(f"cmake --build build {xsetup.cmake_build_config} -- all")
        os.system(f"cmake --install build {xsetup.cmake_build_config}")
    except Exception as e:
        return False
    finally:
        os.chdir(cwd)
    return True


if __name__ == "__main__":
    build()
