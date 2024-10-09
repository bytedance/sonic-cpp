/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES.
 * All rights reserved. SPDX-License-Identifier: Apache-2.0 Copyright 2022
 * ByteDance Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <arm_sve.h>

namespace sonic_json {
namespace internal {
namespace sve2_128 {

const int16_t d8d[] = {1000, 100, 10, 1, 1000, 100, 10, 1};
const int16_t d4d[] = {1000, 100, 10, 1, 0, 0, 0, 0};
const int64_t d2d[] = {10000, 1, 100000000, 10000};
const uint8_t ldigits[] = {'1', '2', '3', '4', '5', '6', '7', '8',
                           '9', '0', '0', '0', '0', '0', '0', '0'};

sonic_force_inline uint64_t digit_cnt(svint8_t in) {
  const svbool_t ptrue = svptrue_b8();
  svint8_t dd =
      svld1_s8(ptrue, reinterpret_cast<const signed char*>(&ldigits[0]));
  svbool_t cg = svmatch_s8(ptrue, in, dd);
  cg = sveor_b_z(ptrue, cg, ptrue);
  cg = svbrkb_z(ptrue, cg);
  return svcntp_b8(ptrue, cg);
}

sonic_force_inline uint64_t simd_str2int_sve_8(svint16_t in) {
  const svbool_t ptrue = svptrue_b16();
  svint16_t lu1 = svld1_s16(ptrue, reinterpret_cast<const int16_t*>(&d8d[0]));
  svint64_t lu2 =
      svld1_s64(svptrue_b64(), reinterpret_cast<const int64_t*>(&d2d[0]));

  svint64_t r1 = svdot(svdup_n_s64(0), lu1, in);
  r1 = svmul_m(svptrue_b64(), lu2, r1);
  return svaddv_s64(svptrue_b64(), r1);
}

sonic_force_inline uint64_t simd_str2int_sve_l8(svint16_t in, uint32_t len) {
  const svbool_t ptrue = svptrue_b16();
  svbool_t cg = svwhilelt_b16(0u, len);
  cg = sveor_b_z(ptrue, cg, ptrue);
  svint16_t d = svsplice_s16(cg, svdup_s16(0), in);

  return simd_str2int_sve_8(d);
}

sonic_force_inline uint64_t simd_str2int_sve_12(svint16_t inlo,
                                                svint16_t inhi) {
  const svbool_t ptrue = svptrue_b16();
  svint16_t lu1 = svld1_s16(ptrue, reinterpret_cast<const int16_t*>(&d8d[0]));
  svint64_t lu2 =
      svld1_s64(svptrue_b64(), reinterpret_cast<const int64_t*>(&d2d[2]));

  svint64_t r1 = svdot(svdup_n_s64(0), lu1, inlo);
  r1 = svmul_m(svptrue_b64(), lu2, r1);

  lu1 = svld1_s16(ptrue, &d4d[0]);
  svint64_t r2 = svdot(svdup_n_s64(0), lu1, inhi);

  r2 = svadd_s64_x(svptrue_b64(), r2, r1);
  return svaddv_s64(svptrue_b64(), r2);
}

sonic_force_inline uint64_t simd_str2int(const char* c, int& man_nd) {
  svint8_t in =
      svld1_s8(svptrue_b8(), reinterpret_cast<const signed char*>(&c[0]));
  int len = (int)digit_cnt(in);
  uint64_t ret = 1;
  man_nd = man_nd < len ? man_nd : len;
  in = svsub_n_s8_x(svptrue_b8(), in, '0');
  switch (man_nd) {
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
      ret = simd_str2int_sve_l8(svunpklo_s16(in), man_nd);
      break;
    case 8:
      ret = simd_str2int_sve_8(svunpklo_s16(in));
      break;
    case 9:
      ret = simd_str2int_sve_8(svunpklo_s16(in)) * 10ull +
            simd_str2int_sve_l8(svunpkhi_s16(in), 1);
      break;
    case 10:
      ret = simd_str2int_sve_8(svunpklo_s16(in)) * 100ull +
            simd_str2int_sve_l8(svunpkhi_s16(in), 2);
      break;
    case 11:
      ret = simd_str2int_sve_8(svunpklo_s16(in)) * 1000ull +
            simd_str2int_sve_l8(svunpkhi_s16(in), 3);
      break;
    case 12:
      ret = simd_str2int_sve_12(svunpklo_s16(in), svunpkhi_s16(in));
      break;
    case 13:
      ret = simd_str2int_sve_8(svunpklo_s16(in)) * 100000ull +
            simd_str2int_sve_l8(svunpkhi_s16(in), 5);
      break;
    case 14:
      ret = simd_str2int_sve_8(svunpklo_s16(in)) * 1000000ull +
            simd_str2int_sve_l8(svunpkhi_s16(in), 6);
      break;
    case 15:
      ret = simd_str2int_sve_8(svunpklo_s16(in)) * 10000000ull +
            simd_str2int_sve_l8(svunpkhi_s16(in), 7);
      break;
    case 16:
      ret = simd_str2int_sve_8(svunpklo_s16(in)) * 100000000ull +
            simd_str2int_sve_8(svunpkhi_s16(in));
      break;
    default:
      ret = 0;
      break;
  }
  return ret;
}

}  // namespace sve2_128
}  // namespace internal
}  // namespace sonic_json
