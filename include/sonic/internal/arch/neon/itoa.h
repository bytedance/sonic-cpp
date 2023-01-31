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

#include <arm_neon.h>
#include <sonic/macro.h>

#include <cstddef>
#include <cstdint>

namespace sonic_json {
namespace internal {
namespace neon {

// Convert num {abcd} to {axxxx, abxxxx, abcxxxx, abcdxxxx}
static sonic_force_inline uint16x8_t Utoa_4_helper(uint16_t num) {
  uint16_t v = num << 2;
  // v00 = vector{abcd * 4, abcd * 4, abcd * 4, abcd * 4}
  uint16x4_t v00 = vdup_n_u16(v);

  uint16x4_t kVecDiv = vreinterpret_u16_u64(vdup_n_u64(0x80003334147b20c5));
  uint32x4_t v01 = vmull_u16(v00, kVecDiv);
  uint16x4_t v02 = vshrn_n_u32(v01, 16);
  uint16x4_t kVecShift = vreinterpret_u16_u64(vdup_n_u64(0x8000200008000080));
  uint32x4_t v03 = vmull_u16(v02, kVecShift);
  return vreinterpretq_u16_u32(v03);
}

// Convert num's each digit as packed 16-bit in a vector.
// num's digits as abcdefgh (high bits is 0 if not enough)
// The converted vector is { a, b, c, d, e, f, g, h }
sonic_force_inline uint16x8_t UtoaNeon(uint32_t num) {
  uint16_t hi = num % 10000;  // {efgh}
  uint16_t lo = num / 10000;  // {abcd}

  // v10 = {a, ab, abc, abcd, e, ef, efg, efgh}
  uint16x8_t v10 = vuzp2q_u16(Utoa_4_helper(lo), Utoa_4_helper(hi));

  // v12 = {0, a0, ab0, abc0, 0, e0, ef0, efg0}
  uint16x8_t v11 = vmulq_u16(v10, vdupq_n_u16(10));
  uint16x8_t v12 =
      vreinterpretq_u16_u64(vshlq_n_u64(vreinterpretq_u64_u16(v11), 16));

  // v13 = {a, b, c, d, e, f, g, h}
  uint16x8_t v13 = vsubq_u16(v10, v12);
  return v13;
}

static sonic_force_inline char *Utoa_8(uint32_t val, char *out) {
  /* convert to digits */
  uint16x8_t v0 = UtoaNeon(val);
  uint16x8_t v1 = vdupq_n_u16(0);

  /* convert to bytes, add '0' */
  uint8x16_t v2 = vcombine_u8(vqmovun_s16(vreinterpretq_s16_u16(v0)),
                              vqmovun_s16(vreinterpretq_s16_u16(v1)));
  uint8x16_t v3 = vaddq_u8(v2, vdupq_n_u8('0'));

  /* store high 64 bits */
  vst1q_u8((uint8_t *)(out), v3);
  return out + 8;
}

static sonic_force_inline char *Utoa_16(uint64_t val, char *out) {
  /* remaining digits */
  uint16x8_t v0 = UtoaNeon((uint32_t)(val / 100000000));
  uint16x8_t v1 = UtoaNeon((uint32_t)(val % 100000000));
  /* convert to bytes, add '0' */
  uint8x16_t v2 = vcombine_u8(vqmovun_s16(vreinterpretq_s16_u16(v0)),
                              vqmovun_s16(vreinterpretq_s16_u16(v1)));
  uint8x16_t v3 = vaddq_u8(v2, vdupq_n_u8('0'));

  vst1q_u8((uint8_t *)(out), v3);
  return out + 16;
}

}  // namespace neon
}  // namespace internal
}  // namespace sonic_json
