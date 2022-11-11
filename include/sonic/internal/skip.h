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
#include <immintrin.h>

#include "sonic/error.h"
#include "sonic/internal/haswell.h"
#include "sonic/internal/simd.h"
#include "sonic/internal/unicode.h"
#include "sonic/macro.h"

namespace sonic_json {
namespace internal {

using namespace haswell;

static sonic_force_inline bool IsSpace(uint8_t ch) {
  return ch == ' ' || ch == '\r' || ch == '\n' || ch == '\t';
}

sonic_force_inline uint64_t GetStringBits(const uint8_t *data,
                                          uint64_t &prev_instring,
                                          uint64_t &prev_escaped) {
  const simd::simd8x64<uint8_t> v(data);
  uint64_t escaped = 0;
  uint64_t bs_bits = v.eq('\\');
  if (bs_bits) {
    escaped = GetEscapedBranchless(prev_escaped, bs_bits);
  } else {
    escaped = prev_escaped;
    prev_escaped = 0;
  }
  uint64_t quote_bits = v.eq('"') & ~escaped;
  uint64_t in_string = prefix_xor(quote_bits) ^ prev_instring;
  prev_instring = uint64_t(static_cast<int64_t>(in_string) >> 63);
  return in_string;
}

// SkipScanner is used to skip space and json values in json text.
// TODO: optimize by removing bound checking.
class SkipScanner {
 public:
  enum SkipStatus {
    NextObjKey,
    ObjEnd,
    NextArrElem,
    ArrEnd,
    Unknown,
  };
  sonic_force_inline uint8_t SkipSpace(const uint8_t *data, size_t &pos) {
    // fast path for single space
    if (!IsSpace(data[pos++])) return data[pos - 1];
    if (!IsSpace(data[pos++])) return data[pos - 1];

    uint64_t nonspace;
    // current pos is out of block
    if (pos >= nonspace_bits_end_) {
    found_space:
      while (1) {
        nonspace = GetNonSpaceBits(data + pos);
        if (nonspace) {
          nonspace_bits_end_ = pos + 64;
          pos += trailing_zeroes(nonspace);
          nonspace_bits_ = nonspace;
          return data[pos++];
        } else {
          pos += 64;
        }
      }
      sonic_assert(false && "!should not happen");
    }

    // current pos is in block
    sonic_assert(pos + 64 > nonspace_bits_end_);
    size_t block_start = nonspace_bits_end_ - 64;
    sonic_assert(pos >= block_start);
    size_t bit_pos = pos - block_start;
    uint64_t mask = (1ull << bit_pos) - 1;
    nonspace = nonspace_bits_ & (~mask);
    if (nonspace == 0) {
      pos = nonspace_bits_end_;
      goto found_space;
    }
    pos = block_start + trailing_zeroes(nonspace);
    return data[pos++];
  }

  // pos is the after the ending quote
  sonic_force_inline void SkipString(const uint8_t *data, size_t &pos) {
    uint64_t quote_bits;
    uint64_t escaped, bs_bits, prev_escaped = 0;
    while (true) {
      const simd::simd256<uint8_t> v(data + pos);
      bs_bits = static_cast<uint32_t>((v == '\\').to_bitmask());
      quote_bits = static_cast<uint32_t>((v == '"').to_bitmask());
      if (((bs_bits - 1) & quote_bits) != 0) {
        pos += trailing_zeroes(quote_bits) + 1;
        return;
      }
      if (bs_bits) {
        escaped = GetEscapedBranchless(prev_escaped, bs_bits);
        quote_bits &= ~escaped;
        if (quote_bits) {
          pos += trailing_zeroes(quote_bits) + 1;
          return;
        }
      }
      pos += 32;
    }
    return;
  }

  // find the first '"'(the next key) or '}'(the ending of object)
  sonic_force_inline SkipStatus SkipObjectPrimitives(const uint8_t *data,
                                                     size_t &pos, size_t len) {
    while (true) {
      if (pos >= len) {
        return Unknown;
      }
      simd256<uint8_t> v(data + pos);
      auto v_mask = (v == '\"') | (v == '}');
      uint32_t next = static_cast<uint32_t>(v_mask.to_bitmask());
      if (next) {
        pos += trailing_zeroes(next) + 1;
        uint8_t c = data[pos - 1];
        return c == '\"' ? NextObjKey : ObjEnd;
      }
      pos += 32;
    }
    return Unknown;
  }

  // find the first ','(the next elem) or '}'(the ending of array)
  sonic_force_inline SkipStatus SkipArrayPrimitives(const uint8_t *data,
                                                    size_t &pos, size_t len) {
    while (true) {
      if (pos >= len) {
        return Unknown;
      }
      simd256<uint8_t> v(data + pos);
      auto v_mask = (v == ',') | (v == ']');
      uint32_t next = static_cast<uint32_t>(v_mask.to_bitmask());
      if (next) {
        pos += trailing_zeroes(next) + 1;
        uint8_t c = data[pos - 1];
        return c == ',' ? NextArrElem : ArrEnd;
      }
      pos += 32;
    }
    return Unknown;
  }

  sonic_force_inline void SkipObject(const uint8_t *data, size_t &pos,
                                     size_t len) {
    uint64_t prev_instring = 0, prev_escaped = 0;
    int rbrace_num = 0, lbrace_num = 0, last_lbrace_num;
    while (true) {
      if (pos >= len) {
        return;
      }
      uint64_t instring =
          GetStringBits(data + pos, prev_instring, prev_escaped);
      simd::simd8x64<uint8_t> v(data + pos);
      last_lbrace_num = lbrace_num;
      uint64_t rbrace = v.eq('}') & ~instring;
      uint64_t lbrace = v.eq('{') & ~instring;
      // traverse each '}'
      while (rbrace > 0) {
        rbrace_num++;
        lbrace_num = last_lbrace_num + count_ones((rbrace - 1) & lbrace);
        bool is_closed = lbrace_num < rbrace_num;
        if (is_closed) {
          sonic_assert(rbrace_num == lbrace_num + 1);
          pos += trailing_zeroes(rbrace) + 1;
          return;
        }
        rbrace &= (rbrace - 1);
      }
      lbrace_num = last_lbrace_num + count_ones(lbrace);
      pos += 64;
    }
  }

  sonic_force_inline void SkipArray(const uint8_t *data, size_t &pos,
                                    size_t len) {
    uint64_t prev_instring = 0, prev_escaped = 0;
    int rbrace_num = 0, lbrace_num = 0, last_lbrace_num;
    while (true) {
      if (pos >= len) {
        return;
      }
      uint64_t instring =
          GetStringBits(data + pos, prev_instring, prev_escaped);
      simd::simd8x64<uint8_t> v(data + pos);
      last_lbrace_num = lbrace_num;
      uint64_t rbrace = v.eq(']') & ~instring;
      uint64_t lbrace = v.eq('[') & ~instring;
      while (rbrace > 0) {
        rbrace_num++;
        lbrace_num = last_lbrace_num + count_ones((rbrace - 1) & lbrace);
        bool is_closed = lbrace_num < rbrace_num;
        if (is_closed) {
          sonic_assert(rbrace_num == lbrace_num + 1);
          pos += trailing_zeroes(rbrace) + 1;
          return;
        }
        rbrace &= (rbrace - 1);
      }
      lbrace_num = last_lbrace_num + count_ones(lbrace);
      pos += 64;
    }
  }

  sonic_force_inline SonicError GetArrayElem(const uint8_t *data, size_t &pos,
                                             size_t len, int index) {
    while (index > 0 && pos < len) {
      index--;
      char c = SkipSpace(data, pos);
      if (c == '{') {
        SkipObject(data, pos, len);
        c = SkipSpace(data, pos);
        // array may be closed
        if (c != ',') {
          return kParseErrorArrIndexOutOfRange;
        }
        continue;
      }
      if (c == '[') {
        SkipArray(data, pos, len);
        c = SkipSpace(data, pos);
        // array may be closed
        if (c != ',') {
          return kParseErrorArrIndexOutOfRange;
        }
        continue;
      }
      if (c == ']') return kParseErrorArrIndexOutOfRange;
      if (c == '"') {
        SkipString(data, pos);
        c = SkipSpace(data, pos);
        // array may be closed
        if (c != ',') {
          return kParseErrorArrIndexOutOfRange;
        }
        continue;
      }
      // skip primitives
      if (SkipArrayPrimitives(data, pos, len) != NextArrElem) {
        return kParseErrorArrIndexOutOfRange;
      }
    }
    return index == 0 ? kErrorNone : kParseErrorInvalidChar;
  }

 private:
  size_t nonspace_bits_end_{0};
  uint64_t nonspace_bits_{0};
};

}  // namespace internal
}  // namespace sonic_json
