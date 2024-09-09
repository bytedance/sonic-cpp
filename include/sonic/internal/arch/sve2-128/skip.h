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

#define VEC_LEN 16

#include "../common/arm_common/skip.h"
#include "base.h"
#include "simd.h"

namespace sonic_json {
namespace internal {
namespace sve2_128 {

using sonic_json::internal::arm_common::GetNextToken;
using sonic_json::internal::arm_common::skip_space_safe;
using sonic_json::internal::arm_common::SkipContainer;
using sonic_json::internal::arm_common::SkipString;
using sonic_json::internal::common::EqBytes4;
using sonic_json::internal::common::SkipLiteral;

// TODO: optimize by removing bound checking.
sonic_force_inline uint8_t skip_space(const uint8_t *data, size_t &pos,
                                      size_t &, uint64_t &) {
  // fast path for single space
  if (!IsSpace(data[pos++])) return data[pos - 1];
  if (!IsSpace(data[pos++])) return data[pos - 1];

  // current pos is out of block
  while (1) {
    const svbool_t pmatch = GetNonSpaceBits(data + pos);
    const svbool_t ptrue = svptrue_b8();
    if (svptest_any(ptrue, pmatch)) {
      // nonspace = bit position of first non-space token
      const uint64_t nonspace = svcntp_b8(ptrue, svbrkb_z(ptrue, pmatch));
      pos += nonspace;
      return data[pos++];
    } else {
      pos += 16;
    }
  }
  sonic_assert(false && "!should not happen");
}

}  // namespace sve2_128
}  // namespace internal
}  // namespace sonic_json

#undef VEC_LEN
