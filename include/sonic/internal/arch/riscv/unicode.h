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
namespace riscv {

using sonic_json::internal::common::handle_unicode_codepoint;

struct StringBlock {
 public:
  sonic_force_inline static StringBlock Find(const uint8_t *src);
  sonic_force_inline static StringBlock Find(simd8<uint8_t> &v);
  template <ParseFlags parseFlags>
  sonic_force_inline bool HasQuoteFirst() const {
    constexpr bool kAllowUnescapedControlChars =
        (parseFlags & ParseFlags::kParseAllowUnescapedControlChars) != 0;
    if constexpr (kAllowUnescapedControlChars) {
      return (((bs_bits - 1) & quote_bits) != 0);
    } else {
      return (((bs_bits - 1) & quote_bits) != 0) && (!HasUnescaped());
    }
  }
  sonic_force_inline bool HasBackslash() const {
    return ((quote_bits - 1) & bs_bits) != 0;
  }
  sonic_force_inline bool HasUnescaped() const {
    return ((quote_bits - 1) & unescaped_bits) != 0;
  }
  sonic_force_inline int QuoteIndex() const {
    return TrailingZeroes(quote_bits) >> 2;
  }
  sonic_force_inline int BsIndex() const {
    return TrailingZeroes(bs_bits) >> 2;
  }

  uint64_t bs_bits;
  uint64_t quote_bits;
  uint64_t unescaped_bits;
};

sonic_force_inline StringBlock StringBlock::Find(const uint8_t *src) {
  simd8<uint8_t> v = simd8<uint8_t>::load(src);
  return StringBlock::Find(v);
}

sonic_force_inline StringBlock StringBlock::Find(simd8<uint8_t> &v) {
  simd8<uint8_t> bs = simd8<uint8_t>::splat('\\');
  simd8<uint8_t> quote = simd8<uint8_t>::splat('"');
  simd8<uint8_t> ctrl = simd8<uint8_t>::splat('\x1f');

  simd8<bool> bs_cmp = (v == bs);
  simd8<bool> quote_cmp = (v == quote);
  simd8<bool> ctrl_cmp = (v <= ctrl);

  return {
      bs_cmp.to_bitmask(),
      quote_cmp.to_bitmask(),
      ctrl_cmp.to_bitmask(),
  };
}

sonic_force_inline uint64_t GetNonSpaceBits(const uint8_t *data) {
  simd8<uint8_t> v = simd8<uint8_t>::load(data);
  simd8<uint8_t> space = simd8<uint8_t>::splat(' ');
  simd8<uint8_t> tab = simd8<uint8_t>::splat('\t');
  simd8<uint8_t> nl = simd8<uint8_t>::splat('\n');
  simd8<uint8_t> cr = simd8<uint8_t>::splat('\r');

  simd8<bool> m1 = (v == space);
  simd8<bool> m2 = (v == tab);
  simd8<bool> m3 = (v == nl);
  simd8<bool> m4 = (v == cr);

  simd8<bool> m5 = m1 | m2;
  simd8<bool> m6 = m3 | m4;
  simd8<bool> m7 = m5 | m6;
  simd8<bool> m8 = ~m7;

  return m8.to_bitmask();
}

sonic_force_inline bool IsAscii(const simd8x64<uint8_t> &input) {
  simd8<uint8_t> bits = input.reduce_or();
  return bits.max_val() < 0x80u;
}

sonic_force_inline simd8<bool> MustBe2_3Continuation(
    const simd8<uint8_t> prev2, const simd8<uint8_t> prev3) {
  simd8<bool> is_third_byte = prev2 >= uint8_t(0xe0u);
  simd8<bool> is_fourth_byte = prev3 >= uint8_t(0xf0u);
  return is_third_byte ^ is_fourth_byte;
}

}  // namespace riscv
}  // namespace internal
}  // namespace sonic_json
