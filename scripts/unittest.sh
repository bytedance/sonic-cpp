#!/usr/bin/env bash
set -e

export UNIT_TEST_TARGET=unittest-clang
case $1 in
    --help)
        echo "bash unittest.sh [-g|-c], -g build gcc unittest target, -c build clang unittest target"
        ;;
    -g)
        export UNIT_TEST_TARGET=unittest-gcc
        ;;
    -c)
        export UNITTEST_TARGET=unittest-clang
        ;;
    *)
esac

BAZEL=bazel
if ! type bazel >/dev/null 2>&1; then
	wget https://github.com/bazelbuild/bazel/releases/download/4.0.0/bazel-4.0.0-installer-linux-x86_64.sh; chmod +x bazel-4.0.0-installer-linux-x86_64.sh  
	./bazel-4.0.0-installer-linux-x86_64.sh --prefix=`pwd`
	BAZEL=`pwd`/bin/bazel
fi

${BAZEL} build :benchmark
${BAZEL} build :benchmark --copt="-DSONIC_DEBUG"
${BAZEL} build :${UNIT_TEST_TARGET} --copt="-DSONIC_DEBUG"
${BAZEL} run :${UNIT_TEST_TARGET} --copt="-DSONIC_LOCKED_ALLOCATOR"
${BAZEL} run :${UNIT_TEST_TARGET} --copt="-DSONIC_NOT_VALIDATE_UTF8"
