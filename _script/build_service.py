import _check_env as ce
import getopt
import os
import shutil
import sys
import platform

using_single_build=False
if platform.system() == 'Linux':
    using_single_build=True
print(f"using_single_build={using_single_build}")

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
    opts, args = getopt.getopt(argv, "rd:x:j:")
except getopt.GetoptError:
    sys.exit(2)
for opt, arg in opts:
    if opt == "-x":
        x_path = os.path.abspath(arg)
        print("xlib path: %s" % (x_path))
    if opt == '-d':
        db_path = os.path.abspath(arg)
        print("dblib path: %s" % (db_path))
    if opt == "-j":
        j_threads = " -j%s " % arg
    if opt == '-r':
        os.environ["PS_BUILD_CONFIG_TYPE"] = "Release"
    pass

if os.getenv("PS_BUILD_CONFIG_TYPE") is None:
    os.environ["PS_BUILD_CONFIG_TYPE"]="Debug"
build_type=os.getenv("PS_BUILD_CONFIG_TYPE")

if not os.path.isdir(x_path):
    print("x_path not found")
    sys.exit(2)
x_install_path = x_path + "/_corex_installed/"
if not os.path.isdir(x_install_path):
    print("x_install_path not found")
    sys.exit(2)

if not os.path.isdir(db_path):
    print("db_path not found")
    sys.exit(2)
db_install_path = db_path + "/_install/"
if not os.path.isdir(db_install_path):
    print("db_install_path not found")
    sys.exit(2)

src_dir = ce.cpp_dir
build_path = ce.cwd + "/_build/cpp"
full_install_dir = ce.cwd + "/_install"

if os.path.isdir(build_path):
    shutil.rmtree(build_path)
os.makedirs(build_path)

config_setup=""
if using_single_build:
    config_setup=f"-DCMAKE_BUILD_TYPE={build_type}"

os.system(
    'cmake -Wno-dev '
    f'{config_setup} ' \
    f'-DX_LIB={x_path!r} '
    f'-DDB_LIB={db_path!r} '
    f'-DCMAKE_INSTALL_PREFIX={full_install_dir!r} -B {build_path!r} {src_dir!r}')
os.system(f"cmake --build {build_path} --config {build_type} -- {j_threads} all")
os.system(f"cmake --install {build_path} --config {build_type}")
