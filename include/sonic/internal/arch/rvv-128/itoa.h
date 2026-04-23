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

#include <sonic/macro.h>
#include <stdint.h>

#include <string>

#include "simd.h"

namespace sonic_json {
namespace internal {
namespace rvv_128 {
// Convert num {abcd} to {axxxx, abxxxx, abcxxxx, abcdxxxx}
static sonic_force_inline vuint16m1_t Utoa_4_helper(uint16_t num) {
  uint16_t v = num << 2;
  vuint16m1_t v00 = __riscv_vmv_v_x_u16m1(v, 4);

  vuint16m1_t kVecDiv = __riscv_vreinterpret_v_u64m1_u16m1(
      __riscv_vmv_v_x_u64m1(0x80003334147b20c5, 1));
  vuint32m1_t v01 = __riscv_vlmul_trunc_v_u32m2_u32m1(
      __riscv_vwmulu_vv_u32m2(v00, kVecDiv, 4));
  vuint16m1_t v02 =
      __riscv_vnsrl_wx_u16m1(__riscv_vlmul_ext_v_u32m1_u32m2(v01), 16, 4);
  vuint16m1_t kVecShift = __riscv_vreinterpret_v_u64m1_u16m1(
      __riscv_vmv_v_x_u64m1(0x8000200008000080, 1));
  vuint32m1_t v03 = __riscv_vlmul_trunc_v_u32m2_u32m1(
      __riscv_vwmulu_vv_u32m2(v02, kVecShift, 4));
  return __riscv_vreinterpret_v_u32m1_u16m1(v03);
}

static sonic_force_inline vuint16m1_t rvv_uzp2q_u16(vuint16m1_t a,
                                                    vuint16m1_t b, size_t vl) {
  vuint16m2_t ab =
      __riscv_vset_v_u16m1_u16m2(__riscv_vlmul_ext_v_u16m1_u16m2(a), 1, b);
  vuint32m2_t ab_ = __riscv_vreinterpret_v_u16m2_u32m2(ab);
  vuint16m1_t res = __riscv_vnsrl_wx_u16m1(ab_, 16, 16);
  return res;
}

static sonic_force_inline vuint8m1_t vqmovun_s16(vint16m1_t a) {
  vuint16m1_t a_non_neg =
      __riscv_vreinterpret_v_i16m1_u16m1(__riscv_vmax_vx_i16m1(a, 0, 8));
  return __riscv_vlmul_ext_v_u8mf2_u8m1(
      __riscv_vnclipu_wx_u8mf2(a_non_neg, 0, __RISCV_VXRM_RDN, 8));
}

// Convert num's each digit as packed 16-bit in a vector.
// num's digits as abcdefgh (high bits is 0 if not enough)
// The converted vector is { a, b, c, d, e, f, g, h }
sonic_force_inline vuint16m1_t UtoaNeon(uint32_t num) {
  uint16_t hi = num % 10000;  // {efgh}
  uint16_t lo = num / 10000;  // {abcd}

  // v10 = {a, ab, abc, abcd, e, ef, efg, efgh}
  vuint16m1_t v10 = rvv_uzp2q_u16(Utoa_4_helper(lo), Utoa_4_helper(hi), 8);

  // v12 = {0, a0, ab0, abc0, 0, e0, ef0, efg0}
  vuint16m1_t v11 = __riscv_vmul_vv_u16m1(v10, __riscv_vmv_v_x_u16m1(10, 8), 8);

  vuint16m1_t v12 = __riscv_vreinterpret_v_u64m1_u16m1(
      __riscv_vsll_vx_u64m1(__riscv_vreinterpret_v_u16m1_u64m1(v11), 16, 2));
  // v13 = {a, b, c, d, e, f, g, h}
  vuint16m1_t v13 = __riscv_vsub_vv_u16m1(v10, v12, 8);
  return v13;
}

static sonic_force_inline char *Utoa_8(uint32_t val, char *out) {
  /* convert to digits */
  vuint16m1_t v0 = UtoaNeon(val);
  vuint16m1_t v1 = __riscv_vmv_v_x_u16m1(0, 8);

  /* convert to bytes, add '0' */
  vuint8m1_t v2 = __riscv_vslideup_vx_u8m1(
      vqmovun_s16(__riscv_vreinterpret_v_u16m1_i16m1(v0)),
      vqmovun_s16(__riscv_vreinterpret_v_u16m1_i16m1(v1)), 8, 16);
  vuint8m1_t v3 = __riscv_vadd_vv_u8m1(v2, __riscv_vmv_v_x_u8m1('0', 16), 16);

  /* store high 64 bits */
  __riscv_vse8_v_u8m1((uint8_t *)(out), v3, 16);
  return out + 8;
}

static sonic_force_inline char *Utoa_16(uint64_t val, char *out) {
  /* remaining digits */
  vuint16m1_t v0 = UtoaNeon((uint32_t)(val / 100000000));

  vuint16m1_t v1 = UtoaNeon((uint32_t)(val % 100000000));

  /* convert to bytes, add '0' */
  vuint8m1_t v2 = __riscv_vslideup_vx_u8m1(
      vqmovun_s16(__riscv_vreinterpret_v_u16m1_i16m1(v0)),
      vqmovun_s16(__riscv_vreinterpret_v_u16m1_i16m1(v1)), 8, 16);

  vuint8m1_t v3 = __riscv_vadd_vv_u8m1(v2, __riscv_vmv_v_x_u8m1('0', 16), 16);

  __riscv_vse8_v_u8m1((uint8_t *)(out), v3, 16);
  return out + 16;
}

}  // namespace rvv_128
}  // namespace internal
}  // namespace sonic_json
