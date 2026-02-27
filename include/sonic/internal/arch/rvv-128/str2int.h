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

#include "simd.h"

namespace sonic_json {
namespace internal {
namespace rvv_128 {

const int16_t d8d[] = {1000, 100, 10, 1, 1000, 100, 10, 1};

sonic_force_inline uint64_t digit_cnt(vint8m1_t in) {
  vbool8_t m1 = __riscv_vmsgt_vx_i8m1_b8(in, '9', 16);
  vbool8_t m2 = __riscv_vmslt_vx_i8m1_b8(in, '0', 16);
  vbool8_t m3 = __riscv_vmor_mm_b8(m1, m2, 16);
  return __riscv_vfirst_m_b8(m3, 16) == -1 ? 16 : __riscv_vfirst_m_b8(m3, 16);
}

sonic_force_inline uint64_t simd_str2int_rvv_8(vint16m1_t in) {
  vint16m1_t lu1 = __riscv_vle16_v_i16m1(d8d, 8);
  vint32m2_t mul = __riscv_vwmul_vv_i32m2(in, lu1, 8);
  vint32m1_t a = __riscv_vlmul_trunc_v_i32m2_i32m1(mul);
  vint32m1_t b =
      __riscv_vlmul_trunc_v_i32m2_i32m1(__riscv_vslidedown_vx_i32m2(mul, 4, 8));
  vint32m1_t c = __riscv_vmacc_vx_i32m1(b, 10000, a, 4);
  vint64m1_t zero = __riscv_vmv_v_x_i64m1(0, 2);
  vint64m1_t reds = __riscv_vwredsum_vs_i32m1_i64m1(c, zero, 4);
  return __riscv_vmv_x_s_i64m1_i64(reds);
}

sonic_force_inline uint64_t simd_str2int_rvv_l8(vint16m1_t in, uint32_t len) {
  vint16m1_t _d =
      __riscv_vslideup_vx_i16m1(__riscv_vmv_v_x_i16m1(0, 8), in, 8 - len, 8);

  return simd_str2int_rvv_8(_d);
}

sonic_force_inline uint64_t simd_str2int(const char* c, int& man_nd) {
  vint8m1_t in =
      __riscv_vle8_v_i8m1(reinterpret_cast<const int8_t*>(&c[0]), 16);
  int len = (int)digit_cnt(in);
  uint64_t ret = 1;
  man_nd = man_nd < len ? man_nd : len;
  in = __riscv_vsub_vx_i8m1(in, '0', 16);
  switch (man_nd) {
    vint8m1_t hi;
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
      ret = simd_str2int_rvv_l8(
          __riscv_vwcvt_x_x_v_i16m1(__riscv_vlmul_trunc_v_i8m1_i8mf2(in), 16),
          man_nd);

      // ret = simd_str2int_sve_l8(svunpklo_s16(in), man_nd);
      break;
    case 8:
      ret = simd_str2int_rvv_8(
          __riscv_vwcvt_x_x_v_i16m1(__riscv_vlmul_trunc_v_i8m1_i8mf2(in), 16));
      break;
    case 9:
      hi = __riscv_vslidedown_vx_i8m1(in, 8, 16);
      ret = simd_str2int_rvv_8(__riscv_vwcvt_x_x_v_i16m1(
                __riscv_vlmul_trunc_v_i8m1_i8mf2(in), 16)) *
                10ull +
            simd_str2int_rvv_l8(__riscv_vwcvt_x_x_v_i16m1(
                                    __riscv_vlmul_trunc_v_i8m1_i8mf2(hi), 16),
                                1);
      break;
    case 10:
      hi = __riscv_vslidedown_vx_i8m1(in, 8, 16);
      ret = simd_str2int_rvv_8(__riscv_vwcvt_x_x_v_i16m1(
                __riscv_vlmul_trunc_v_i8m1_i8mf2(in), 16)) *
                100ull +
            simd_str2int_rvv_l8(__riscv_vwcvt_x_x_v_i16m1(
                                    __riscv_vlmul_trunc_v_i8m1_i8mf2(hi), 16),
                                2);
      break;
    case 11:
      hi = __riscv_vslidedown_vx_i8m1(in, 8, 16);
      ret = simd_str2int_rvv_8(__riscv_vwcvt_x_x_v_i16m1(
                __riscv_vlmul_trunc_v_i8m1_i8mf2(in), 16)) *
                1000ull +
            simd_str2int_rvv_l8(__riscv_vwcvt_x_x_v_i16m1(
                                    __riscv_vlmul_trunc_v_i8m1_i8mf2(hi), 16),
                                3);
      break;
    case 12:
      hi = __riscv_vslidedown_vx_i8m1(in, 8, 16);
      ret = simd_str2int_rvv_8(__riscv_vwcvt_x_x_v_i16m1(
                __riscv_vlmul_trunc_v_i8m1_i8mf2(in), 16)) *
                10000ull +
            simd_str2int_rvv_l8(__riscv_vwcvt_x_x_v_i16m1(
                                    __riscv_vlmul_trunc_v_i8m1_i8mf2(hi), 16),
                                4);
      break;
    case 13:
      hi = __riscv_vslidedown_vx_i8m1(in, 8, 16);
      ret = simd_str2int_rvv_8(__riscv_vwcvt_x_x_v_i16m1(
                __riscv_vlmul_trunc_v_i8m1_i8mf2(in), 16)) *
                100000ull +
            simd_str2int_rvv_l8(__riscv_vwcvt_x_x_v_i16m1(
                                    __riscv_vlmul_trunc_v_i8m1_i8mf2(hi), 16),
                                5);
      break;
    case 14:
      hi = __riscv_vslidedown_vx_i8m1(in, 8, 16);
      ret = simd_str2int_rvv_8(__riscv_vwcvt_x_x_v_i16m1(
                __riscv_vlmul_trunc_v_i8m1_i8mf2(in), 16)) *
                1000000ull +
            simd_str2int_rvv_l8(__riscv_vwcvt_x_x_v_i16m1(
                                    __riscv_vlmul_trunc_v_i8m1_i8mf2(hi), 16),
                                6);
      break;
    case 15:
      hi = __riscv_vslidedown_vx_i8m1(in, 8, 16);
      ret = simd_str2int_rvv_8(__riscv_vwcvt_x_x_v_i16m1(
                __riscv_vlmul_trunc_v_i8m1_i8mf2(in), 16)) *
                10000000ull +
            simd_str2int_rvv_l8(__riscv_vwcvt_x_x_v_i16m1(
                                    __riscv_vlmul_trunc_v_i8m1_i8mf2(hi), 16),
                                7);
      break;
    case 16:
      hi = __riscv_vslidedown_vx_i8m1(in, 8, 16);
      ret = simd_str2int_rvv_8(__riscv_vwcvt_x_x_v_i16m1(
                __riscv_vlmul_trunc_v_i8m1_i8mf2(in), 16)) *
                100000000ull +
            simd_str2int_rvv_8(__riscv_vwcvt_x_x_v_i16m1(
                __riscv_vlmul_trunc_v_i8m1_i8mf2(hi), 16));
      break;
    default:
      ret = 0;
      break;
  }
  return ret;
}
}  // namespace rvv_128
}  // namespace internal
}  // namespace sonic_json
