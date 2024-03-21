#!/usr/bin/env python3

import _prepared_3rd as p3
import getopt
import os
import shutil
import sys

import _build_mmdb as mmdb
import _build_abseil as abseil
import _build_protobuf as protobuf

if __name__ != "__main__":
    print("not valid entry, name=%s" % (__name__))
    exit

server_side_lib_only = "OFF"
try:
    argv = sys.argv[1:]
    opts, args = getopt.getopt(argv, "s")
except getopt.GetoptError:
    sys.exit(2)
for opt, arg in opts:
    if opt == "-s":
        server_side_lib_only = "ON"
        print("server-side lib only")
    pass

# remove temp dir
cwd = os.getcwd()
dependency_unzip_dir = f"{cwd}/_3rd_build"

if not mmdb.build():
    print("failed to build mmdb")
    exit -1

if not abseil.build():
    print("failed to build abseil")
    exit -1

if not protobuf.build():
    print("failed to build protobuf")
    exit -1

if os.path.isdir(dependency_unzip_dir):
    shutil.rmtree(dependency_unzip_dir)
