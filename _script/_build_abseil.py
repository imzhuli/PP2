#!/usr/bin/env python3

import tarfile
import os
import _cmake_util as cu

cwd = os.getcwd()
unzip_dir = f"{cwd}/_3rd_build"

src_file = f"{cwd}/_3rd_source/abseil-cpp-20240116.1.tar.gz"
unzipped_src_dir = f"{unzip_dir}/abseil-cpp-20240116.1"
install_dir = f"{cwd}/_3rd_installed"


def build():
    if os.getenv("PS_BUILD_CONFIG_TYPE") is None:
        os.environ["PS_BUILD_CONFIG_TYPE"]="Debug"
    build_type=os.getenv("PS_BUILD_CONFIG_TYPE")
    print(f"=============> {build_type}")

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
            'cmake '
            '-Wno-dev '
            '-DABSL_BUILD_TESTING=OFF '
            '-DABSL_PROPAGATE_CXX_STD=ON '
            '-DCMAKE_CXX_STANDARD=20 '
            f'-DCMAKE_INSTALL_PREFIX={install_dir!r} -B build .')
        os.system(f"cmake --build build --config {build_type} -- all")
        os.system(f"cmake --install build --config {build_type}")
    except Exception as e:
        return False
    finally:
        os.chdir(cwd)
    return True


if __name__ == "__main__":
    build()
