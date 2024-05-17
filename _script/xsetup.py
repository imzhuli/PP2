import platform

#init to default, will be fixed at the end of this file
cmake_build_type = f"-DCMAKE_BUILD_TYPE=Debug"
cmake_build_config = f"--config Debug"

def Debug():
    global cmake_build_type
    global cmake_build_config
    build_type = "Debug"
    cmake_build_type = f"-DCMAKE_BUILD_TYPE={build_type}"
    cmake_build_config = f"--config {build_type}"
    PostCheck()
    pass


def Release():
    global cmake_build_type
    global cmake_build_config
    build_type = "Release"
    cmake_build_type = f"-DCMAKE_BUILD_TYPE={build_type}"
    cmake_build_config = f"--config {build_type}"
    PostCheck()
    pass

def PostCheck():
    global cmake_build_type
    global cmake_build_config
    cmake_use_single_build=(platform.system() != 'Windows')
    if not cmake_use_single_build: #windows
        cmake_build_type = ""
    pass

def Output():
    print(f"cmake_build_type: {cmake_build_type}")
    print(f"cmake_build_config: {cmake_build_config}")

PostCheck()
