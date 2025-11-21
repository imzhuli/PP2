import _check_env as ce
import sys
import getopt

if __name__ != "__main__":
    print("not valid entry, name=%s" % (__name__))
    exit

if not ce.check_env():
    print("invalid env check result")
    exit

ce.remake_dir(ce.install_dir)
