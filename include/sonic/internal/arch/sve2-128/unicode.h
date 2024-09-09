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
namespace sve2_128 {

using sonic_json::internal::common::handle_unicode_codepoint;

struct StringBlock {
 public:
  sonic_force_inline static StringBlock Find(const uint8_t *src);
  sonic_force_inline static StringBlock Find(uint8x16_t &v);
  // has quote, and no backslash or unescaped before it
  sonic_force_inline bool HasQuoteFirst() const {
    return (bs_index > quote_index) && !HasUnescaped();
  }
  // has backslash, and no quote before it
  sonic_force_inline bool HasBackslash() const {
    return quote_index > bs_index;
  }
  // has unescaped, and no quote before it
  sonic_force_inline bool HasUnescaped() const {
    return quote_index > unescaped_index;
  }
  sonic_force_inline int QuoteIndex() const {
    sonic_assert(quote_index < 16);
    return quote_index;
  }
  sonic_force_inline int BsIndex() const {
    sonic_assert(bs_index < 16);
    return bs_index;
  }
  sonic_force_inline int UnescapedIndex() const {
    sonic_assert(unescaped_index < 16);
    return unescaped_index;
  }

  // 0 ~ 15: bit position of first token, 16 - token not found
  unsigned bs_index;
  unsigned quote_index;
  unsigned unescaped_index;
};

// return bit position of the first occurrence of the token in a vector
// return 16 if token does not exist in the vector
sonic_force_inline unsigned locate_token(const svuint8x16_t v, char token) {
  const svbool_t ptrue = svptrue_b8();
  // same as svcmpeq, but with higher performance
  const svbool_t pmatch =
      svmatch(ptrue, v, svdup_n_u8(static_cast<uint8_t>(token)));
  // count trailing zeros of a predicate, return 16 if all zeros
  return static_cast<unsigned>(svcntp_b8(ptrue, svbrkb_z(ptrue, pmatch)));
}

sonic_force_inline StringBlock StringBlock::Find(const uint8_t *src) {
  svuint8x16_t v = svld1(svptrue_b8(), src);
  return {
      locate_token(v, '\\'),
      locate_token(v, '"'),
      locate_token(v, '\x1f'),
  };
}

sonic_force_inline StringBlock StringBlock::Find(uint8x16_t &v) {
  return {
      locate_token(v, '\\'),
      locate_token(v, '"'),
      locate_token(v, '\x1f'),
  };
}

// return a predicate indicating occurrence of four tokens
sonic_force_inline svbool_t GetNonSpaceBits(const uint8_t *data) {
  const svbool_t ptrue = svptrue_b8();
  const svuint8x16_t v = svld1_u8(ptrue, data);
  // load four tokens: tab(09), LF(0a), CR(0d), space(20)
  const svuint8x16_t tokens = svreinterpret_u8_u32(svdup_n_u32(0x090a0d20U));
  return svnmatch_u8(ptrue, v, tokens);
}

}  // namespace sve2_128
}  // namespace internal
}  // namespace sonic_json
