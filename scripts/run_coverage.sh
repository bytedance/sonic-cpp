#!/usr/bin/env bash
set -e
apt update
apt install unzip openjdk-11-jdk -y

BAZEL=bazel
if ! type bazel >/dev/null 2>&1; then
	wget https://github.com/bazelbuild/bazel/releases/download/4.0.0/bazel-4.0.0-installer-linux-x86_64.sh; chmod +x bazel-4.0.0-installer-linux-x86_64.sh  
	./bazel-4.0.0-installer-linux-x86_64.sh --prefix=`pwd`
	BAZEL=`pwd`/bin/bazel
fi
COPT=
if [ $# -gt 0 ]; then
    COPT=$1
fi

$BAZEL coverage --combined_report=lcov --instrument_test_targets unittest-gcc-coverage
mv bazel-out/_coverage/_coverage_report.dat ./cov.out
