#!/usr/bin/env python3

import _prepared_3rd as p3
import getopt
import os
import shutil
import sys
import xsetup

import _build_mmdb as mmdb
import _build_json as nlohmann_json
import _build_rapidcsv as rapidcsv
import _build_rdkafka as rdkafka
import _build_abseil as abseil
import _build_protobuf as protobuf

if __name__ != "__main__":
    print("not valid entry, name=%s" % (__name__))
    exit

try:
    argv = sys.argv[1:]
    opts, args = getopt.getopt(argv, "r")
except getopt.GetoptError:
    sys.exit(2)
for opt, arg in opts:
    if opt == '-r':
        xsetup.Release()
    pass
xsetup.Output()

# remove temp dir
cwd = os.getcwd()
dependency_unzip_dir = f"{cwd}/_3rd_build"

if not nlohmann_json.build():
    print("failed to build nlohmann_json")
    exit -1

if not rapidcsv.build():
    print("failed to build rapidcsv")
    exit -1

if not mmdb.build():
    print("failed to build mmdb")
    exit -1

if not rdkafka.build():
    print("failed to build rdkafka")
    exit -1


# if not abseil.build():
#     print("failed to build abseil")
#     exit -1

# if not protobuf.build():
#     print("failed to build protobuf")
#     exit -1

if os.path.isdir(dependency_unzip_dir):
    shutil.rmtree(dependency_unzip_dir)
