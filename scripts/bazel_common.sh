#!/usr/bin/env bash

# Pick a Bazel executable.
#
# Policy:
# - Prefer system `bazel` if it matches repo `.bazelversion` (or USE_BAZEL_VERSION / BAZEL_VERSION).
# - Otherwise use `bazelisk` (honors `.bazelversion`).
# - If `bazelisk` is not installed, download a suitable prebuilt bazelisk binary.
#
# Usage:
#   source "${CUR_DIR}/bazel_common.sh"
#   BAZEL_BIN="$(sonic_pick_bazel "${TOP_DIR}")"

sonic_pick_bazel() {
  local top_dir="${1:-}"
  local desired_bazel_version=""

  if [[ -n "${top_dir}" && -f "${top_dir}/.bazelversion" ]]; then
    desired_bazel_version="$(tr -d ' \t\r\n' < "${top_dir}/.bazelversion")"
  fi
  if [[ -z "${desired_bazel_version}" ]]; then
    desired_bazel_version="${USE_BAZEL_VERSION:-${BAZEL_VERSION:-}}"
  fi
  if [[ -n "${desired_bazel_version}" ]]; then
    export USE_BAZEL_VERSION="${desired_bazel_version}"
  fi

  local bazel_bin=""

  if command -v bazel > /dev/null 2>&1; then
    if [[ -z "${desired_bazel_version}" ]]; then
      bazel_bin="bazel"
    else
      local sys_ver_raw sys_ver
      sys_ver_raw="$(bazel --version 2> /dev/null || true)"
      sys_ver="${sys_ver_raw##* }"
      if [[ -n "${sys_ver}" && "${sys_ver}" == "${desired_bazel_version}" ]]; then
        bazel_bin="bazel"
      fi
    fi
  fi

  if [[ -z "${bazel_bin}" ]] && command -v bazelisk > /dev/null 2>&1; then
    bazel_bin="bazelisk"
  fi

  if [[ -z "${bazel_bin}" ]]; then
    local os arch asset url
    os="$(uname -s | tr '[:upper:]' '[:lower:]')"
    arch="$(uname -m)"

    case "${os}" in
      linux)
        case "${arch}" in
          x86_64 | amd64) asset="bazelisk-linux-amd64" ;;
          aarch64 | arm64) asset="bazelisk-linux-arm64" ;;
          *)
            echo "Unsupported arch for bazelisk: ${arch}" >&2
            return 1
            ;;
        esac
        ;;
      darwin)
        case "${arch}" in
          x86_64 | amd64) asset="bazelisk-darwin-amd64" ;;
          arm64) asset="bazelisk-darwin-arm64" ;;
          *)
            echo "Unsupported arch for bazelisk: ${arch}" >&2
            return 1
            ;;
        esac
        ;;
      *)
        echo "Unsupported OS for bazelisk: ${os}" >&2
        return 1
        ;;
    esac

    _SONIC_BAZELISK_TMP_DIR="$(mktemp -d)"
    trap 'rm -rf "${_SONIC_BAZELISK_TMP_DIR:-}"' EXIT
    _SONIC_BAZELISK_PATH="${_SONIC_BAZELISK_TMP_DIR}/bazelisk"
    url="https://github.com/bazelbuild/bazelisk/releases/latest/download/${asset}"

    if command -v curl > /dev/null 2>&1; then
      curl -fsSL "${url}" -o "${_SONIC_BAZELISK_PATH}"
    elif command -v wget > /dev/null 2>&1; then
      wget -qO "${_SONIC_BAZELISK_PATH}" "${url}"
    else
      echo "Neither curl nor wget is available to download bazelisk" >&2
      return 1
    fi
    chmod +x "${_SONIC_BAZELISK_PATH}"
    bazel_bin="${_SONIC_BAZELISK_PATH}"
  fi

  printf '%s\n' "${bazel_bin}"
}
