import _check_env as ce
import getopt
import os
import shutil
import sys

if __name__ != "__main__":
    print("not valid entry, name=%s" % (__name__))
    exit

if not ce.check_env():
    print("invalid env check result")
    exit

src_dir = ce.proto_dir
dst_dir_cpp = ce.cpp_dir + "/pb"

protoc_dir = ce.cwd + "/_3rd_installed/bin"
protoc = protoc_dir + "/protoc"
if not os.path.exists(protoc):
    print("protoc not found")
    exit

# remake dirs
if os.path.exists(dst_dir_cpp):
    shutil.rmtree(dst_dir_cpp)
os.mkdir(dst_dir_cpp)


def make_pb(subname):
    sub_src = src_dir + "/" + subname
    sub_dst = dst_dir_cpp + "/" + subname
    os.mkdir(sub_dst)
    gen_server_arch_cmd = f"cd {sub_src!r}; {protoc!r} --cpp_out={sub_dst!r} *.proto"
    os.system(gen_server_arch_cmd)

make_pb("pp2")
