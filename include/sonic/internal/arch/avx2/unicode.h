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

SONIC_PUSH_HASWELL

namespace sonic_json {
namespace internal {
namespace avx2 {

using namespace simd;
using sonic_json::internal::common::handle_unicode_codepoint;

struct StringBlock {
 public:
  sonic_force_inline static StringBlock Find(const uint8_t *src);
  sonic_force_inline bool HasQuoteFirst() {
    return (((bs_bits - 1) & quote_bits) != 0) && !HasUnescaped();
  }
  sonic_force_inline bool HasBackslash() {
    return ((quote_bits - 1) & bs_bits) != 0;
  }
  sonic_force_inline bool HasUnescaped() {
    return ((quote_bits - 1) & unescaped_bits) != 0;
  }
  sonic_force_inline int QuoteIndex() { return TrailingZeroes(quote_bits); }
  sonic_force_inline int BsIndex() { return TrailingZeroes(bs_bits); }
  sonic_force_inline int UnescapedIndex() {
    return TrailingZeroes(unescaped_bits);
  }

  uint32_t bs_bits;
  uint32_t quote_bits;
  uint32_t unescaped_bits;
};

sonic_force_inline StringBlock StringBlock::Find(const uint8_t *src) {
  simd256<uint8_t> v(src);
  return {
      static_cast<uint32_t>((v == '\\').to_bitmask()),
      static_cast<uint32_t>((v == '"').to_bitmask()),
      static_cast<uint32_t>((v <= '\x1f').to_bitmask()),
  };
}

sonic_force_inline uint64_t GetNonSpaceBits(const uint8_t *data) {
  const simd::simd8x64<uint8_t> v(data);
  const auto whitespace_table =
      simd256<uint8_t>::repeat_16(' ', 100, 100, 100, 17, 100, 113, 2, 100,
                                  '\t', '\n', 112, 100, '\r', 100, 100);

  uint64_t space = v.eq({_mm256_shuffle_epi8(whitespace_table, v.chunks[0]),
                         _mm256_shuffle_epi8(whitespace_table, v.chunks[1])});
  return ~space;
}

}  // namespace avx2
}  // namespace internal
}  // namespace sonic_json

SONIC_POP_TARGET
