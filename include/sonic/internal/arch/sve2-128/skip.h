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

#include <sonic/internal/arch/common/skip_common.h>
#include <sonic/internal/utils.h>

#include "../neon/simd.h"
#include "base.h"
#include "simd.h"

namespace sonic_json {
namespace internal {
namespace sve2_128 {

using sonic_json::internal::common::EqBytes4;
using sonic_json::internal::common::SkipLiteral;

#include "../common/arm_common/skip.inc.h"

sonic_force_inline bool SkipContainer(const uint8_t *data, size_t &pos,
                                      size_t len, uint8_t left, uint8_t right) {
  // We use neon for the on demand parser since it is currently faster for
  // comparisons than sve
  return skip_container<sonic_json::internal::neon::simd8x64<uint8_t>>(
      data, pos, len, left, right);
}

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
