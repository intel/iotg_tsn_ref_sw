import argparse
import os.path
import sys
import time
import os
import json
import math

def initialize():
    global IS_DRY
    global genfile

    IS_DRY=False
    genfile = open("setup-generated.sh", "w")
    genfile.write("#!/bin/sh\n")

def teardown():
    genfile.close()

def err_exit(msg):
    print(msg)
    exit(1)

def ensure_keys_exist(name, mapd, *keys):
    for k in keys:
        if not k in mapd:
            print('Key {} does not exist in {}'.format(k, name))
            exit(2)

def sh_run(cmd):
    genfile.write('echo "Running {}"\n'.format(cmd))
    genfile.write(cmd + "\n")

def run_with_out(cmd, outfile):
    cmd += ['2&>', outfile, '&\n']
    cmd2 = ' '.join(cmd)
    genfile.write('echo "Running (async) {}"\n'.format(cmd2))
    genfile.write(cmd2)

