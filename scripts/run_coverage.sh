#!/usr/bin/env bash
set -euo pipefail

CUR_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TOP_DIR="$(cd "${CUR_DIR}/.." && pwd)"

source "${CUR_DIR}/bazel_common.sh"
BAZEL_BIN="$(sonic_pick_bazel "${TOP_DIR}")"
#
# Pass through user-provided Bazel flags (e.g. --config=xxx / --copt=xxx).
# Note: Bazel options must appear before the target.
"${BAZEL_BIN}" coverage --combined_report=lcov "$@" unittest-gcc-coverage

OUTPUT_PATH="$("${BAZEL_BIN}" info output_path)"
COV_DAT="${OUTPUT_PATH}/_coverage/_coverage_report.dat"
if [[ ! -f "${COV_DAT}" ]]; then
  echo "Coverage report not found: ${COV_DAT}" >&2
  exit 1
fi

mv -f "${COV_DAT}" "${TOP_DIR}/coverage.dat"
