#!/usr/bin/env bash
set -e

CUR_DIR="$(dirname "$(realpath "${BASH_SOURCE[0]}")")"

usage() {
    echo "
Usage:
    -g, --gcc       compiler is gcc
    -c, --clang     compiler is clang
    -h, --help      display this message
    --arch={arm|haswell|westmere} target architecture, default is haswell
    --dispatch={dynamic|static} sonic dispatch mode, default is static

    example: bash unittest.sh -g --arch=westmere --dispatch=static
"
}

UNIT_TEST_ARCH=haswell
UNIT_TEST_DISPATCH=static
UNIT_TEST_SANITIZER=gcc

PARSED_ARGUMENTS=$(getopt -o gch --long gcc,clang,help,arch::,dispatch:: -- "$@")
VALID_ARGUMENTS=$?
if [ "$VALID_ARGUMENTS" != "0" ]; then
    usage
    exit
fi
eval set -- "$PARSED_ARGUMENTS"
while true
do
    case "$1" in
    -g|--gcc)
        UNIT_TEST_SANITIZER=gcc
        shift ;;
    -c|--clang)
        UNIT_TEST_SANITIZER=clang
        shift ;;
    -h|--help)
        usage
        exit
        ;;
    --arch)
        case "$2" in
            "") shift 2 ;;
            arm|haswell|westmere)
                UNIT_TEST_ARCH="$2"
                shift 2 ;;
            *)
                echo "Invalid argument for --arch: $2"
                usage
                exit 1
                ;;
        esac ;;
    --dispatch)
        case "$2" in
            "") shift 2 ;;
            dynamic|static)
                UNIT_TEST_DISPATCH="$2"
                shift 2 ;;
            *)
                echo "Invalid argument for --dispatch: $2"
                usage
                exit 1
                ;;
        esac ;;
    --) shift ; break ;;
    *)
        echo "Invalid option: $1"
        usage
        exit 1
        ;;
    esac
done

BAZEL=bazel
if ! type bazel >/dev/null 2>&1; then
	wget https://github.com/bazelbuild/bazel/releases/download/4.0.0/bazel-4.0.0-installer-linux-x86_64.sh; chmod +x bazel-4.0.0-installer-linux-x86_64.sh
	./bazel-4.0.0-installer-linux-x86_64.sh --prefix="${CUR_DIR}"
	BAZEL="${CUR_DIR}/bin/bazel"
fi

# default target
set -x

${BAZEL} run :unittest --//:sonic_arch=$UNIT_TEST_ARCH \
    --//:sonic_sanitizer=${UNIT_TEST_SANITIZER} \
    --//:sonic_dispatch=${UNIT_TEST_DISPATCH} \
    --copt="-DSONIC_LOCKED_ALLOCATOR" -s

${BAZEL} build :unittest --//:sonic_sanitizer=${UNIT_TEST_SANITIZER} --copt="-DSONIC_DEBUG"

${BAZEL} build :benchmark
${BAZEL} build :benchmark --copt="-DSONIC_DEBUG"

set +x
