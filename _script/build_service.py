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

x_path = ""
j_threads = ""

try:
    argv = sys.argv[1:]
    opts, args = getopt.getopt(argv, "x:j:")
except getopt.GetoptError:
    sys.exit(2)
for opt, arg in opts:
    if opt == "-x":
        x_path = os.path.abspath(arg)
        print("xlib path: %s" % (x_path))
    if opt == "-j":
        j_threads = " -j%s " % arg
    pass

if not os.path.isdir(x_path):
    print("x_path not found")
    sys.exit(2)
x_install_path = x_path + "/_corex_installed/"
if not os.path.isdir(x_install_path):
    print("x_install_path not found")
    sys.exit(2)

src_dir = ce.cpp_dir
build_path = ce.cwd + "/_build/cpp"
full_install_dir = ce.cwd + "/_install"

if os.path.isdir(build_path):
    shutil.rmtree(build_path)
os.makedirs(build_path)

os.system(
    'cmake -Wno-dev '
    f'-DX_LIB={x_path!r} '
    f'-DCMAKE_INSTALL_PREFIX={full_install_dir!r} -B {build_path!r} {src_dir!r}')
os.system(f"cmake --build {build_path} -- {j_threads} all")
os.system(f"cmake --build {build_path} -- install")
