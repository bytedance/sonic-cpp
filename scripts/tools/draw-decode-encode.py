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

def compare(args):
    # detech current branch.
    result = run_r("git branch")
    current_branch = None
    for br in result.split('\n'):
        if br.startswith("* "):
            current_branch = br.lstrip('* ')
            break

    if not current_branch:
        print ("Failed to detech current branch")
        return None
    
    # get the current diff
    (fd, diff) = tempfile.mkstemp();
    run("git diff > %s"%diff)

    # early return if currrent is master and not diff found.
    # notice that, the new file won't be counted in.
    print ("Current branch: %s"%(current_branch))
    if current_branch == "master" and os.stat(diff).st_size == 0:
        print ("No change found.")
        return None

    # benchmark current branch    
    (fd, target) = tempfile.mkstemp(".target.json")
    run("bazel build :benchmark")
    run("bazel-bin/benchmark %s --benchmark_out_format=json --benchmark_out=%s"%(args, target))

    # trying to switch to the latest master
    run("git checkout -- .")
    if current_branch != "master":
        run("git checkout master")
    run("git pull")

    # benchmark master branch
    (fd, master) = tempfile.mkstemp(".master.json")
    run("bazel build :benchmark")
    run("bazel-bin/benchmark %s --benchmark_out_format=json --benchmark_out=%s"%(args, master))

    # restore branch
    if current_branch != "master":
        run("git checkout %s"%(current_branch))
    run("patch -p1 < %s" % (diff))

    # diff the result
    run("scripts/tools/compare.py benchmarks %s %s"%(master, target))
    return target

def main():
    argparser = argparse.ArgumentParser(description='Tools to test the performance')
    argparser.add_argument('-f', '--benchmark_filter', dest='filter', required=False,
        help='Specify the filter for google benchmark')
    argparser.add_argument('-c', '--compare', dest='compare', action='store_true', required=False,
        help='Compare with the master benchmarking')
    args = argparser.parse_args()
    
    if args.filter:
        gbench_args = "--benchmark_filter=%s"%(args.filter)
    else:
        gbench_args = ""

    if args.compare:
        target = compare(gbench_args)
    else:
        target = None

    if not target:
        (fd, target) = tempfile.mkstemp(".target.json")
        run("bazel build :benchmark")
        run("bazel-bin/benchmark %s --benchmark_out_format=json --benchmark_out=%s"%(gbench_args, target))
    run("scripts/tools/draw-png.py %s"%(target))

if __name__ == "__main__":
    main()
