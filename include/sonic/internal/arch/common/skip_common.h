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

#include "sonic/macro.h"

namespace sonic_json {
namespace internal {
namespace common {

static sonic_force_inline bool EqBytes4(const uint8_t *src, uint32_t target) {
  uint32_t val;
  static_assert(sizeof(uint32_t) <= SONICJSON_PADDING,
                "SONICJSON_PADDING must be larger than 4 bytes");
  std::memcpy(&val, src, sizeof(uint32_t));
  return val == target;
}

static sonic_force_inline bool IsValidSeparator(uint8_t c) {
  //  return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == ',' ||
  //         c == ']' || c == '}' || c == '\0';
  constexpr uint64_t mask = (1ULL << 0) |   // '\0'
                            (1ULL << 9) |   // '\t'
                            (1ULL << 10) |  // '\n'
                            (1ULL << 13) |  // '\r'
                            (1ULL << 32) |  // ' '
                            (1ULL << 44);   // ','

  return c < 64 ? (mask >> c) & 1 : (c == 93 || c == 125);
}

sonic_force_inline bool SkipLiteral(const uint8_t *data, size_t &pos,
                                    size_t len, uint8_t token) {
  // the binary of 'ull' in null
  static constexpr uint32_t kNullBin = 0x6c6c756e;
  // the binary of 'rue' in true
  static constexpr uint32_t kTrueBin = 0x65757274;
  // the binary of 'alse' in false
  static constexpr uint32_t kFalseBin = 0x65736c61;
  auto start = data + pos - 1;
  auto end = data + len;
  switch (token) {
    case 't':
      if (start + 4 <= end && EqBytes4(start, kTrueBin) &&
          (start + 4 == end || IsValidSeparator(start[4]))) {
        pos += 3;
        return true;
      }
      break;
    case 'n':
      if (start + 4 <= end && EqBytes4(start, kNullBin) &&
          (start + 4 == end || IsValidSeparator(start[4]))) {
        pos += 3;
        return true;
      }
      break;
    case 'f':
      if (start + 5 <= end && EqBytes4(start + 1, kFalseBin) &&
          (start + 5 == end || IsValidSeparator(start[5]))) {
        pos += 4;
        return true;
      }
      break;
    default:
      return false;
  }
  return false;
}

}  // namespace common
}  // namespace internal
}  // namespace sonic_json
