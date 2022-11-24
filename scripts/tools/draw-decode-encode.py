#!/usr/bin/env python3
import tempfile
import os
import sys
import subprocess
import argparse

def run(cmd):
    print(cmd)
    if os.system(cmd):
        print ("Failed to run cmd: %s"%(cmd))
        exit(1)

def run_r(cmd):
    print (cmd)
    try:
        cmds = cmd.split(' ')
        data = subprocess.check_output(cmds, stderr=subprocess.STDOUT)
    except subprocess.CalledProcessError as e:
        if e.returncode:
            print (e.output)
            exit(1)
    
    return data.decode("utf-8") 

def main():
    argparser = argparse.ArgumentParser(description='Tools to test the performance')
    argparser.add_argument('-f', '--benchmark_filter', dest='filter', required=False,
        help='Specify the filter for google benchmark')
    args = argparser.parse_args()

    if args.filter:
        gbench_args = "--benchmark_filter=%s"%(args.filter)
    else:
        gbench_args = ""

    (fd, target) = tempfile.mkstemp(".target.json")
    run("bazel build :benchmark")
    run("bazel-bin/benchmark %s --benchmark_out_format=json --benchmark_out=%s"%(gbench_args, target))
    run("scripts/tools/draw-png.py %s"%(target))

if __name__ == "__main__":
    main()
