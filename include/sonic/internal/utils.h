/*
 * Copyright 2022 ByteDance Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <cstddef>
#include <cstdint>

#include "sonic/macro.h"

namespace sonic_json {
namespace internal {

static sonic_force_inline bool IsSpace(uint8_t ch) {
  return ch == ' ' || ch == '\r' || ch == '\n' || ch == '\t';
}

static sonic_force_inline bool IsDigit(char c) { return '0' <= c && c <= '9'; }

// SkipNumberSafe will validate the number defined from JSON RFC.
// And return the ending position.
static sonic_force_inline size_t SkipNumberSafe(const char *digits,
                                                size_t len) {
#define SONIC_MUST(exp)          \
  do {                           \
    if (np >= end || (!(exp))) { \
      return 0;                  \
    }                            \
  } while (0)

  const char *np = digits;
  const char *end = np + len;

  // check sign, '+/d' is not allowed in JSON RFC
  if (np < end && *np == '-') {
    np++;
  }

  // skip integer part, check leading zero at first
  if (np < end && *np == '0') {
    np++;
    if (np < end && IsDigit(*np)) {
      return 0;
    }
  } else {
    SONIC_MUST(IsDigit(*np));
    while (np < end && IsDigit(*np)) np++;
  }

  // skip fraction part
  if (np < end && *np == '.') {
    np++;
    SONIC_MUST(IsDigit(*np));
    while (np < end && IsDigit(*np)) np++;
  }

  // skip exponent part
  if (np < end && (*np == 'e' || *np == 'E')) {
    np++;
    if (np < end && (*np == '-' || *np == '+')) {
      np++;
    }
    SONIC_MUST(IsDigit(*np));
    while (np < end && IsDigit(*np)) np++;
  }
#undef SONIC_MUST
  return np - digits;
}

}  // namespace internal
}  // namespace sonic_json
