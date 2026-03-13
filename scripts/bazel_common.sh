#!/usr/bin/env bash

# Pick a Bazel executable.
#
# Policy:
# - Prefer system `bazel` if it matches repo `.bazelversion` (or USE_BAZEL_VERSION / BAZEL_VERSION).
# - Otherwise use `bazelisk` (honors `.bazelversion`).
# - If `bazelisk` is not installed, download a suitable prebuilt bazelisk binary.
#
# Bazelisk download controls (for reproducibility):
# - Default Bazelisk version is pinned via `SONIC_BAZELISK_VERSION` (or `BAZELISK_VERSION`).
# - You may override the full download URL via `SONIC_BAZELISK_URL`.
# - Downloaded Bazelisk is cached under `${XDG_CACHE_HOME:-$HOME/.cache}` when possible.
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

    local bazelisk_version cache_root cache_dir cache_path
    bazelisk_version="${SONIC_BAZELISK_VERSION:-${BAZELISK_VERSION:-v1.25.0}}"

    cache_root="${XDG_CACHE_HOME:-}"
    if [[ -z "${cache_root}" ]]; then
      if [[ -n "${HOME:-}" ]]; then
        cache_root="${HOME}/.cache"
      fi
    fi

    cache_dir=""
    cache_path=""
    if [[ -n "${cache_root}" ]]; then
      cache_dir="${cache_root}/sonic-cpp/bazelisk/${bazelisk_version}"
      cache_path="${cache_dir}/${asset}"
    fi

    if [[ -n "${cache_path}" && -x "${cache_path}" ]]; then
      bazel_bin="${cache_path}"
    else
      url="${SONIC_BAZELISK_URL:-https://github.com/bazelbuild/bazelisk/releases/download/${bazelisk_version}/${asset}}"

      if [[ -n "${cache_path}" ]] && mkdir -p "${cache_dir}" 2> /dev/null; then
        local tmp_path
        tmp_path="${cache_path}.tmp.$$"
        if command -v curl > /dev/null 2>&1; then
          curl -fsSL "${url}" -o "${tmp_path}"
        elif command -v wget > /dev/null 2>&1; then
          wget -qO "${tmp_path}" "${url}"
        else
          echo "Neither curl nor wget is available to download bazelisk" >&2
          return 1
        fi
        chmod +x "${tmp_path}"
        mv -f "${tmp_path}" "${cache_path}"
        bazel_bin="${cache_path}"
      else
        _SONIC_BAZELISK_TMP_DIR="$(mktemp -d)"
        _SONIC_BAZELISK_PATH="${_SONIC_BAZELISK_TMP_DIR}/bazelisk"

        local prev_trap prev_cmd
        prev_trap="$(trap -p EXIT || true)"
        prev_cmd=""
        if [[ "${prev_trap}" =~ trap\ --\ \'(.*)\'\ EXIT ]]; then
          prev_cmd="${BASH_REMATCH[1]}"
        fi
        if [[ -n "${prev_cmd}" ]]; then
          trap "rm -rf \"${_SONIC_BAZELISK_TMP_DIR}\"; ${prev_cmd}" EXIT
        else
          trap "rm -rf \"${_SONIC_BAZELISK_TMP_DIR}\"" EXIT
        fi

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
    fi
  fi

  printf '%s\n' "${bazel_bin}"
}
