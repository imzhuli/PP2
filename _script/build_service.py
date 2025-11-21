import _check_env as ce
import getopt
import os
import shutil
import sys
import xsetup

if __name__ != "__main__":
    print("not valid entry, name=%s" % (__name__))
    exit

if not ce.check_env():
    print("invalid env check result")
    exit

x_path = ""
db_path = ""
j_threads = ""

try:
    argv = sys.argv[1:]
    opts, args = getopt.getopt(argv, "rx:j:")
except getopt.GetoptError:
    sys.exit(2)
for opt, arg in opts:
    if opt == "-x":
        x_path = os.path.abspath(arg)
        print("xlib path: %s" % (x_path))
    if opt == "-j":
        j_threads = " -j%s " % arg
    if opt == "-r":
        xsetup.Release()
    pass
xsetup.Output()

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

cm = (
    "cmake -Wno-dev "
    "-DCMAKE_EXPORT_COMPILE_COMMANDS=1 "
    f"{xsetup.cmake_build_type} "
    f'-DX_LIB="{x_path}" '
    f'-DCMAKE_INSTALL_PREFIX={full_install_dir!r} -B "{build_path}" "{src_dir}"'
)
print(cm)
if 0 != os.system(cm):
    sys.exit(3)

cmb = f'cmake --build "{build_path}" {xsetup.cmake_build_config} -- {j_threads}'
print(cmb)
if 0 != os.system(cmb):
    sys.exit(4)
if 0 != os.system(f'cmake --install "{build_path}" {xsetup.cmake_build_config}'):
    sys.exit(5)
