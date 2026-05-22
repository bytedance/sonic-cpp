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

#include "base.h"
#include "simd.h"

namespace sonic_json {
namespace internal {
namespace rvv_128 {

using sonic_json::internal::common::EqBytes4;
using sonic_json::internal::common::SkipLiteral;

#include "../common/riscv_common/skip.inc.h"

sonic_force_inline bool SkipContainer(const uint8_t *data, size_t &pos,
                                      size_t len, uint8_t left, uint8_t right) {
  return skip_container<simd8x64<uint8_t>>(data, pos, len, left, right);
}

sonic_force_inline uint8_t skip_space(const uint8_t *data, size_t &pos,
                                      size_t &, uint64_t &) {
  // fast path for single space
  if (!IsSpace(data[pos++])) return data[pos - 1];
  if (!IsSpace(data[pos++])) return data[pos - 1];

  // current pos is out of block
  while (1) {
    uint16_t nonspace = GetNonSpaceBits(data + pos);
    if (nonspace) {
      int tmp = __builtin_ctz(nonspace);
      pos += tmp;
      return data[pos++];
    } else {
      pos += 16;
    }
  }
  sonic_assert(false && "!should not happen");
}

sonic_force_inline uint8_t skip_space_safe(const uint8_t *data, size_t &pos,
                                           size_t len, size_t &, uint64_t &) {
  while (pos < len && IsSpace(data[pos++]));
  // if not found, still return the space chars
  return data[pos - 1];
}

}  // namespace rvv_128
}  // namespace internal
}  // namespace sonic_json

#undef VEC_LEN
