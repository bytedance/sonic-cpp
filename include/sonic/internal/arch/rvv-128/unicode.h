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

#include "../common/unicode_common.h"
#include "base.h"
#include "simd.h"

namespace sonic_json {
namespace internal {
namespace rvv_128 {

using sonic_json::internal::common::handle_unicode_codepoint;

struct StringBlock {
 public:
  sonic_force_inline static StringBlock Find(const uint8_t *src);
  sonic_force_inline static StringBlock Find(vuint8m1_t &v);
  sonic_force_inline bool HasQuoteFirst() const {
    // return (((bs_bits - 1) & quote_bits) != 0) && !HasUnescaped();
    return (((bs_bits)-1) & (quote_bits)) != 0 && !HasUnescaped();
  }
  sonic_force_inline bool HasBackslash() const {
    // return ((quote_bits - 1) & bs_bits) != 0;
    return (((quote_bits)-1) & (bs_bits)) != 0;
  }
  sonic_force_inline bool HasUnescaped() const {
    // return ((quote_bits - 1) & unescaped_bits) != 0;
    return (((quote_bits)-1) & (unescaped_bits)) != 0;
  }
  sonic_force_inline int QuoteIndex() const {
    // return TrailingZeroes(quote_bits);
    return TrailingZeroes(quote_bits);
  }
  sonic_force_inline int BsIndex() const {
    // return TrailingZeroes(bs_bits);
    return TrailingZeroes(bs_bits);
  }
  sonic_force_inline int UnescapedIndex() const {
    // return TrailingZeroes(unescaped_bits);
    return TrailingZeroes(unescaped_bits);
  }

  uint16_t bs_bits;
  uint16_t quote_bits;
  uint16_t unescaped_bits;
};

sonic_force_inline StringBlock StringBlock::Find(const uint8_t *src) {
  vuint8m1_t v =
      __riscv_vle8_v_u8m1(reinterpret_cast<const uint8_t *>(src), 16);
  vuint16m1_t m1 = __riscv_vreinterpret_v_b8_u16m1(
      __riscv_vmseq_vv_u8m1_b8(v, __riscv_vmv_v_x_u8m1('\\', 16), 16));
  vuint16m1_t m2 = __riscv_vreinterpret_v_b8_u16m1(
      __riscv_vmseq_vv_u8m1_b8(v, __riscv_vmv_v_x_u8m1('"', 16), 16));
  vuint16m1_t m3 = __riscv_vreinterpret_v_b8_u16m1(
      __riscv_vmsleu_vv_u8m1_b8(v, __riscv_vmv_v_x_u8m1('\x1f', 16), 16));
  return {__riscv_vmv_x_s_u16m1_u16(m1), __riscv_vmv_x_s_u16m1_u16(m2),
          __riscv_vmv_x_s_u16m1_u16(m3)};
}

sonic_force_inline StringBlock StringBlock::Find(vuint8m1_t &v) {
  vuint16m1_t m1 = __riscv_vreinterpret_v_b8_u16m1(
      __riscv_vmseq_vv_u8m1_b8(v, __riscv_vmv_v_x_u8m1('\\', 16), 16));
  vuint16m1_t m2 = __riscv_vreinterpret_v_b8_u16m1(
      __riscv_vmseq_vv_u8m1_b8(v, __riscv_vmv_v_x_u8m1('"', 16), 16));
  vuint16m1_t m3 = __riscv_vreinterpret_v_b8_u16m1(
      __riscv_vmsleu_vv_u8m1_b8(v, __riscv_vmv_v_x_u8m1('\x1f', 16), 16));
  return {__riscv_vmv_x_s_u16m1_u16(m1), __riscv_vmv_x_s_u16m1_u16(m2),
          __riscv_vmv_x_s_u16m1_u16(m3)};
}

sonic_force_inline uint16_t GetNonSpaceBits(const uint8_t *data) {
  vuint8m1_t v =
      __riscv_vle8_v_u8m1(reinterpret_cast<const uint8_t *>(data), 16);
  vbool8_t m1 = __riscv_vmseq_vv_u8m1_b8(v, __riscv_vmv_v_x_u8m1(' ', 16), 16);
  vbool8_t m2 = __riscv_vmseq_vv_u8m1_b8(v, __riscv_vmv_v_x_u8m1('\t', 16), 16);
  vbool8_t m3 = __riscv_vmseq_vv_u8m1_b8(v, __riscv_vmv_v_x_u8m1('\n', 16), 16);
  vbool8_t m4 = __riscv_vmseq_vv_u8m1_b8(v, __riscv_vmv_v_x_u8m1('\r', 16), 16);
  vbool8_t m5 = __riscv_vmor_mm_b8(m1, m2, 16);
  vbool8_t m6 = __riscv_vmor_mm_b8(m3, m4, 16);
  vbool8_t m7 = __riscv_vmor_mm_b8(m5, m6, 16);
  vbool8_t m8 = __riscv_vmnot_m_b8(m7, 16);
  return __riscv_vmv_x_s_u16m1_u16(__riscv_vreinterpret_v_b8_u16m1(m8));
}

}  // namespace rvv_128
}  // namespace internal
}  // namespace sonic_json
