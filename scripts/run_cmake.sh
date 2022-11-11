#!/usr/bin/env bash
set -e

CMAKE=cmake
function install_cmake()
{
    if ! [ -f cmake-3.24.2-linux-x86_64.tar.gz ]; then
        wget https://github.com/Kitware/CMake/releases/download/v3.24.2/cmake-3.24.2-linux-x86_64.tar.gz
    fi
    if ! [ -d cmake-3.24.2-linux-x86_64 ]; then
        tar zxvf cmake-3.24.2-linux-x86_64.tar.gz
    fi
    CMAKE=`pwd`/cmake-3.24.2-linux-x86_64/bin/cmake
}

# check cmake version: cmake require version is 3.14
if ! type cmake >/dev/null 2>&1; then
    install_cmake
fi
cur_version="$(cmake --version | head -n1 | cut -d" " -f3)"
require_mini_version="3.14.0"
if ! [ "$(printf '%s\n' "${require_mini_version}" "${cur_version}" | sort -V | head -n1)" = "${require_mini_version}" ]; then 
    install_cmake
fi

rm -rf ./build/tests/unittest
$CMAKE -S . -B build
$CMAKE --build build --target unittest -j

if [ -x "./build/tests/unittest" ]; then
    ./build/tests/unittest
fi

$CMAKE -S . -B build -DBUILD_BENCH=ON
$CMAKE --build build --target bench -j
./build/benchmark/bench
