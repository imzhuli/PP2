#!/usr/bin/env python3

import tarfile
import os
import _cmake_util as cu
import xsetup

cwd = os.getcwd()
unzip_dir = f"{cwd}/_3rd_build"

src_file = f"{cwd}/_3rd_source/cyrus-sasl-2.1.28.tar.gz"
unzipped_src_dir = f"{unzip_dir}/cyrus-sasl-2.1.28"
install_dir = f"{cwd}/_3rd_installed"

def build():
    try:
        file = tarfile.open(src_file)
        file.extractall(unzip_dir)
    except Exception as e:
        print(e)
        return False
    finally:
        file.close()

    try:
        print(f"start building..., src_dir={unzipped_src_dir}")
        os.chdir(unzipped_src_dir)
        os.system(f'./configure --prefix={install_dir!r} --enable-static --disable-shared '
            "--enable-scram "
            "--disable-saslauthd "
            "--disable-sample "
            "--disable-obsolete_cram_attr "
            "--disable-obsolete_digest_attr "
            "--enable-static "
            "--disable-shared "
            "--disable-checkapop "
            "--disable-cram "
            "--disable-digest "
            "--disable-otp "
            "--disable-gssapi "
            "--with-dblib=none "
            "--with-pic "        
        )
        os.system(f"make -j 8")
        os.system(f"make install")
    except Exception as e:
        print(e)
        return False
    finally:
        os.chdir(cwd)
    return True



if __name__ == "__main__":
    build()
