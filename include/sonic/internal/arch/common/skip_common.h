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

sonic_force_inline bool SkipLiteral(const uint8_t *data, size_t &pos,
                                    size_t len, uint8_t token) {
  static constexpr uint32_t kNullBin = 0x6c6c756e;
  static constexpr uint32_t kTrueBin = 0x65757274;
  static constexpr uint32_t kFalseBin =
      0x65736c61;  // the binary of 'alse' in false
  auto start = data + pos - 1;
  auto end = data + len;
  switch (token) {
    case 't':
      if (start + 4 <= end && EqBytes4(start, kTrueBin)) {
        pos += 3;
        return true;
      };
      break;
    case 'n':
      if (start + 4 <= end && EqBytes4(start, kNullBin)) {
        pos += 3;
        return true;
      };
      break;
    case 'f':
      if (start + 5 <= end && EqBytes4(start + 1, kFalseBin)) {
        pos += 4;
        return true;
      }
  }
  return false;
}

}  // namespace common
}  // namespace internal
}  // namespace sonic_json
