// Copyright 2018-2019 The simdjson authors

// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0

// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// This file may have been modified by ByteDance authors. All ByteDance
// Modifications are Copyright 2022 ByteDance Authors.

#pragma once

#include <sonic/macro.h>

#include <cstdint>
#include <cstring>

#include "../common/unicode_common.h"
#include "base.h"
#include "simd.h"

namespace sonic_json {
namespace internal {
namespace neon {

using sonic_json::internal::common::handle_unicode_codepoint;

struct StringBlock {
 public:
  sonic_force_inline static StringBlock Find(const uint8_t *src);
  sonic_force_inline static StringBlock Find(uint8x16_t &v);
  sonic_force_inline bool HasQuoteFirst() const {
    return (((bs_bits - 1) & quote_bits) != 0) && !HasUnescaped();
  }
  sonic_force_inline bool HasBackslash() const {
    return ((quote_bits - 1) & bs_bits) != 0;
  }
  sonic_force_inline bool HasUnescaped() const {
    return ((quote_bits - 1) & unescaped_bits) != 0;
  }
  sonic_force_inline int QuoteIndex() const {
    // return TrailingZeroes(quote_bits);
    return TrailingZeroes(quote_bits) >> 2;
  }
  sonic_force_inline int BsIndex() const {
    // return TrailingZeroes(bs_bits);
    return TrailingZeroes(bs_bits) >> 2;
  }
  sonic_force_inline int UnescapedIndex() const {
    // return TrailingZeroes(unescaped_bits);
    return TrailingZeroes(unescaped_bits) >> 2;
  }

  uint64_t bs_bits;
  uint64_t quote_bits;
  uint64_t unescaped_bits;
};

sonic_force_inline StringBlock StringBlock::Find(const uint8_t *src) {
  uint8x16_t v = vld1q_u8(src);
  return {
      to_bitmask(vceqq_u8(v, vdupq_n_u8('\\'))),
      to_bitmask(vceqq_u8(v, vdupq_n_u8('"'))),
      to_bitmask(vcleq_u8(v, vdupq_n_u8('\x1f'))),
  };
}

sonic_force_inline StringBlock StringBlock::Find(uint8x16_t &v) {
  return {
      to_bitmask(vceqq_u8(v, vdupq_n_u8('\\'))),
      to_bitmask(vceqq_u8(v, vdupq_n_u8('"'))),
      to_bitmask(vcleq_u8(v, vdupq_n_u8('\x1f'))),
  };
}

sonic_force_inline uint64_t GetNonSpaceBits(const uint8_t *data) {
  uint8x16_t v = vld1q_u8(data);
  uint8x16_t m1 = vceqq_u8(v, vdupq_n_u8(' '));
  uint8x16_t m2 = vceqq_u8(v, vdupq_n_u8('\t'));
  uint8x16_t m3 = vceqq_u8(v, vdupq_n_u8('\n'));
  uint8x16_t m4 = vceqq_u8(v, vdupq_n_u8('\r'));

  uint8x16_t m5 = vorrq_u8(m1, m2);
  uint8x16_t m6 = vorrq_u8(m3, m4);
  uint8x16_t m7 = vorrq_u8(m5, m6);
  uint8x16_t m8 = vmvnq_u8(m7);

  return to_bitmask(m8);
}

}  // namespace neon
}  // namespace internal
}  // namespace sonic_json
