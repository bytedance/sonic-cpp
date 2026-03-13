#!/usr/bin/env bash
set -euo pipefail

CUR_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
REPO_ROOT="$(cd -- "${CUR_DIR}/.." && pwd -P)"

# You can override these via env vars.
CMAKE_BIN="${CMAKE_BIN:-cmake}"
CMAKE_VERSION="${CMAKE_VERSION:-3.31.10}"
REQUIRE_MIN_VERSION="${REQUIRE_MIN_VERSION:-3.11.0}"
BUILD_DIR="${BUILD_DIR:-${REPO_ROOT}/build}"

die() {
  echo "[run_cmake.sh] $*" 1>&2
  exit 1
}

have() { command -v "$1" > /dev/null 2>&1; }

download() {
  local url="$1"
  local out="$2"
  if have curl; then
    curl -fsSL "${url}" -o "${out}"
  elif have wget; then
    wget -qO "${out}" "${url}"
  else
    die "neither curl nor wget found; cannot download ${url}"
  fi
}

version_ge() {
  # returns 0 if $1 >= $2
  # Pure bash comparison for dot-separated numeric versions.
  # Example: 3.28.1 >= 3.11.0
  local v1="$1" v2="$2"

  # Strip to digits and dots only (defensive for versions like "3.28.1-rc1").
  v1="$(printf '%s' "${v1}" | tr -cd '0-9.')"
  v2="$(printf '%s' "${v2}" | tr -cd '0-9.')"

  local IFS=.
  local -a a b
  read -r -a a <<< "${v1}"
  read -r -a b <<< "${v2}"

  local i n
  n=${#a[@]}
  ((${#b[@]} > n)) && n=${#b[@]}
  for ((i = 0; i < n; i++)); do
    local ai="${a[i]:-0}" bi="${b[i]:-0}"
    # Force base-10 to avoid leading-zero issues.
    if ((10#${ai} > 10#${bi})); then return 0; fi
    if ((10#${ai} < 10#${bi})); then return 1; fi
  done
  return 0
}

cmake_pkg_platform() {
  local os arch
  os="$(uname -s)"
  arch="$(uname -m)"
  case "${os}" in
    Linux)
      case "${arch}" in
        x86_64 | amd64) echo "linux-x86_64" ;;
        aarch64 | arm64) echo "linux-aarch64" ;;
        *) die "unsupported Linux arch: ${arch}" ;;
      esac
      ;;
    Darwin)
      # Prefer universal package so one URL works for both intel/apple-silicon.
      echo "macos-universal"
      ;;
    *)
      die "unsupported OS: ${os} (only Linux/Darwin are handled by auto-install)"
      ;;
  esac
}

cmake_pkg_cmake_path() {
  local platform="$1"
  if [[ "${platform}" == macos-* ]]; then
    echo "CMake.app/Contents/bin/cmake"
  else
    echo "bin/cmake"
  fi
}

install_cmake() {
  local platform tarball base url tool_dir extract_dir
  platform="$(cmake_pkg_platform)"

  tool_dir="${CUR_DIR}/.tools/cmake/${CMAKE_VERSION}/${platform}"
  mkdir -p "${tool_dir}"

  tarball="${tool_dir}/cmake-${CMAKE_VERSION}-${platform}.tar.gz"
  extract_dir="${tool_dir}/cmake-${CMAKE_VERSION}-${platform}"
  base="cmake-${CMAKE_VERSION}-${platform}"
  url="https://github.com/Kitware/CMake/releases/download/v${CMAKE_VERSION}/${base}.tar.gz"

  if [[ ! -f "${tarball}" ]]; then
    download "${url}" "${tarball}"
  fi
  if [[ ! -d "${extract_dir}" ]]; then
    mkdir -p "${extract_dir}"
    tar -xzf "${tarball}" -C "${extract_dir}" --strip-components=1
  fi

  local cmake_rel
  cmake_rel="$(cmake_pkg_cmake_path "${platform}")"
  CMAKE_BIN="${extract_dir}/${cmake_rel}"
  [[ -x "${CMAKE_BIN}" ]] || die "cmake not found after install: ${CMAKE_BIN}"
}

ensure_cmake() {
  if ! have "${CMAKE_BIN}"; then
    install_cmake
    return
  fi
  local cur_version
  cur_version="$("${CMAKE_BIN}" --version | head -n1 | awk '{print $3}')"
  if ! version_ge "${cur_version}" "${REQUIRE_MIN_VERSION}"; then
    install_cmake
  fi
}

jobs() {
  if have nproc; then
    nproc
    return
  fi
  if [[ "$(uname -s)" == Darwin ]] && have sysctl; then
    sysctl -n hw.ncpu
    return
  fi
  echo 4
}

ensure_cmake

UNITTEST_BIN="${BUILD_DIR}/tests/unittest"
rm -f "${UNITTEST_BIN}" || true

CONFIG_ARGS=("-S" "${REPO_ROOT}" "-B" "${BUILD_DIR}")
if [[ -n "${CMAKE_GENERATOR:-}" ]]; then
  CONFIG_ARGS+=("-G" "${CMAKE_GENERATOR}")
fi
if [[ -n "${CMAKE_BUILD_TYPE:-}" ]]; then
  CONFIG_ARGS+=("-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}")
fi

"${CMAKE_BIN}" "${CONFIG_ARGS[@]}"
"${CMAKE_BIN}" --build "${BUILD_DIR}" --target unittest -j "$(jobs)"

if [[ -x "${UNITTEST_BIN}" ]]; then
  "${UNITTEST_BIN}"
fi

#====================================================================
# The benchmark build frequently fails in CI due to incompatible
# CMake configurations across dependent JSON libraries.
# ===================================================================
# $CMAKE -S . -B build -DBUILD_BENCH=ON
# $CMAKE --build build --target bench -j
# ./build/benchmark/bench
