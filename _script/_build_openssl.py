#!/usr/bin/env python3

import tarfile
import os
import _cmake_util as cu
import xsetup

cwd = os.getcwd()
unzip_dir = f"{cwd}/_3rd_build"

src_file = f"{cwd}/_3rd_source/openssl-3.5.7.tar.gz"
unzipped_src_dir = f"{unzip_dir}/openssl-3.5.7"
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
        os.system(f'./Configure --prefix={install_dir!r} '
        "no-shared "                        # 只生静态库
        "no-tls1 no-tls1_1 no-tls1_2 "      # 禁用不需要的TLS版本
        "no-ssl3 no-ssl3-method "           # 禁用SSL 
        "no-dtls no-dtls1 no-dtls1_2 "      # 禁用DTLS
        "no-ecdh no-ecdsa no-dsa no-dh "    # 禁用非对称加密算法
        "no-aria no-bf no-blake2 "          # 禁用对称加密算法
        "no-camellia no-cast no-chacha "    # 禁用更多对称加密算法
        "no-des no-idea no-md4 no-mdc2 "    # 禁用弱算法
        "no-ocb no-poly1305 no-rc2 no-rc4 " # 禁用更多算法
        "no-rc5 no-rmd160 no-scrypt "       # 禁用scrypt（非SCRAM）
        "no-seed no-siphash no-sm2 no-sm3 " # 禁用SM系列国密算法
        "no-sm4 no-whirlpool "              # 禁用whirlpool
        "no-comp "                          # 禁用压缩
        "no-ec "                            # 禁用椭圆曲线（SCRAM不需要）
        "no-gost no-srp no-srtp no-ts "     # 禁用其他协议
        "no-cms no-ct no-cmp "              # 禁用证书管理
        "no-ocsp "                 
        # "no-__asm "                       
        # "no-afalgeng "                     # 禁用硬件引擎
        "no-tests "                          # 禁用测试
        "no-docs "
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
