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

#include <immintrin.h>
#include <sonic/macro.h>

#include <cstdint>
#include <cstring>

#include "../common/unicode_common.h"
#include "base.h"

SONIC_PUSH_WESTMERE

namespace sonic_json {
namespace internal {
namespace sse {

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
  __m128i v = _mm_loadu_si128((__m128i const *)(src));
  return {
      // static_cast<uint32_t>((v == '\\').to_bitmask()),
      static_cast<uint32_t>(
          _mm_movemask_epi8(_mm_cmpeq_epi8(v, _mm_set1_epi8('\\')))),
      static_cast<uint32_t>(
          _mm_movemask_epi8(_mm_cmpeq_epi8(v, _mm_set1_epi8('"')))),
      static_cast<uint32_t>(_mm_movemask_epi8(
          _mm_and_si128(_mm_cmplt_epi8(v, _mm_set1_epi8('\x20')),
                        _mm_cmpgt_epi8(v, _mm_set1_epi8(-1))))),
  };
}

sonic_force_inline uint64_t GetNonSpaceBits(const uint8_t *data) {
  // const simd::simd8x64<uint8_t> v(data);
  // const auto whitespace_table =
  //     simd128<uint8_t>::repeat_16(' ', 100, 100, 100, 17, 100, 113, 2, 100,
  //                                 '\t', '\n', 112, 100, '\r', 100, 100);
  __m128i whitespace_table =
      _mm_setr_epi8(' ', 100, 100, 100, 17, 100, 113, 2, 100, '\t', '\n', 112,
                    100, '\r', 100, 100);
  __m128i v0 = _mm_loadu_si128((__m128i const *)data);
  __m128i v1 = _mm_loadu_si128((__m128i const *)(data + 16));
  __m128i v2 = _mm_loadu_si128((__m128i const *)(data + 32));
  __m128i v3 = _mm_loadu_si128((__m128i const *)(data + 48));
  v0 = _mm_cmpeq_epi8(v0, _mm_shuffle_epi8(whitespace_table, v0));
  v1 = _mm_cmpeq_epi8(v1, _mm_shuffle_epi8(whitespace_table, v1));
  v2 = _mm_cmpeq_epi8(v2, _mm_shuffle_epi8(whitespace_table, v2));
  v3 = _mm_cmpeq_epi8(v3, _mm_shuffle_epi8(whitespace_table, v3));

  uint64_t m0 = uint32_t(_mm_movemask_epi8(v0));
  uint64_t m1 = _mm_movemask_epi8(v1);
  uint64_t m2 = _mm_movemask_epi8(v2);
  uint64_t m3 = _mm_movemask_epi8(v3);
  uint64_t space = m0 | (m1 << 16) | (m2 << 32) | (m3 << 48);

  return ~space;
}

}  // namespace sse
}  // namespace internal
}  // namespace sonic_json

SONIC_POP_TARGET
