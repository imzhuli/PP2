#!/usr/bin/env python3

import os
import shutil

cwd = os.getcwd()
unzip_dir = f"{cwd}/_3rd_build"

libname = "nlohmann_json"
install_dir = f"{cwd}/_3rd_installed"
install_header_dir = f"{install_dir}/include"
src_filename = f"nlohmann_json.hpp"
src_filepath = f"{cwd}/_3rd_source/{src_filename}"
dst_filepath = f"{install_header_dir}/{src_filename}"

def build():
	if not os.path.isfile(src_filepath):
		return False

	if not os.path.exists(install_header_dir):
		os.makedirs(install_header_dir)

	if not os.path.isdir(install_header_dir):
		print("failed to setup install dir")
		return False

	try:
		shutil.copy2(src_filepath, dst_filepath)
	except Exception as e:
		print(f"copy nlohmann_json files error {e}")
		return False

	return True
