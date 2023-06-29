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

#include "../common/skip_common.h"
#include "base.h"
#include "quote.h"
#include "simd.h"
#include "sonic/dom/json_pointer.h"
#include "sonic/error.h"
#include "sonic/internal/utils.h"
#include "sonic/macro.h"
#include "unicode.h"

namespace sonic_json {
namespace internal {
namespace neon {

using sonic_json::internal::common::EqBytes4;
using sonic_json::internal::common::SkipLiteral;

#ifndef VEC_LEN
#error "Define vector length firstly!"
#endif

// GetNextToken find the next characters in tokens and update the position to
// it.
template <size_t N>
sonic_force_inline uint8_t GetNextToken(const uint8_t *data, size_t &pos,
                                        size_t len, const char (&tokens)[N]) {
  while (pos + VEC_LEN <= len) {
    uint8x16_t v = vld1q_u8(data + pos);
    // simd256<uint8_t> v(data + pos);
    // simd256<bool> vor(false);
    uint8x16_t vor = vdupq_n_u8(0);
    for (size_t i = 0; i < N - 1; i++) {
      uint8x16_t vtmp = vceqq_u8(v, vdupq_n_u8((uint8_t)(tokens[i])));
      vor = vorrq_u8(vor, vtmp);
    }

    // neon doesn't have instrution same as movemask, to_bitmask uses shrn to
    // reduce 128bits -> 64bits. If a 128bits bool vector in x86 can convert
    // as 0101, neon shrn will convert it as 0000111100001111.
    uint64_t next = to_bitmask(vor);
    if (next) {
      pos += (TrailingZeroes(next) >> 2);
      return data[pos];
    }
    pos += VEC_LEN;
  }
  while (pos < len) {
    for (size_t i = 0; i < N - 1; i++) {
      if (data[pos] == tokens[i]) {
        return tokens[i];
      }
    }
    pos++;
  }
  return '\0';
}

// pos is the after the ending quote
sonic_force_inline int SkipString(const uint8_t *data, size_t &pos,
                                  size_t len) {
  const static int kEscaped = 2;
  const static int kNormal = 1;
  const static int kUnclosed = 0;
  uint64_t quote_bits = 0;
  uint64_t bs_bits = 0;
  int ret = kNormal;
  while (pos + VEC_LEN <= len) {
    // const simd::simd256<uint8_t> v(data + pos);
    uint8x16_t v = vld1q_u8(data + pos);
    bs_bits = to_bitmask(vceqq_u8(v, vdupq_n_u8('\\')));
    quote_bits = to_bitmask(vceqq_u8(v, vdupq_n_u8('"')));
    if (((bs_bits - 1) & quote_bits) != 0) {
      pos += (TrailingZeroes(quote_bits) >> 2) + 1;
      return ret;
    }
    if (bs_bits) {
      ret = kEscaped;
      pos += ((TrailingZeroes(bs_bits) >> 2) + 2);
      while (pos < len) {
        if (data[pos] == '\\') {
          pos += 2;
        } else {
          break;
        }
      }
    } else {
      pos += VEC_LEN;
    }
  }
  while (pos < len) {
    if (data[pos] == '\\') {
      if (pos + 1 >= len) {
        return kUnclosed;
      }
      ret = kEscaped;
      pos += 2;
      continue;
    }
    if (data[pos++] == '"') {
      return ret;
    }
  };
  return kUnclosed;
}

// return true if container is closed.
sonic_force_inline bool SkipContainer(const uint8_t *data, size_t &pos,
                                      size_t len, uint8_t left, uint8_t right) {
  int rbrace_num = 0, lbrace_num = 0, last_lbrace_num = 0;
  while (pos + VEC_LEN <= len) {
    const uint8_t *p = data + pos;
    last_lbrace_num = lbrace_num;

    uint8x16_t v = vld1q_u8(p);
    uint64_t quote_bits = to_bitmask(vceqq_u8(v, vdupq_n_u8('"')));
    uint64_t not_in_str_mask = 0xFFFFFFFFFFFFFFFF;
    int quote_idx = VEC_LEN;
    if (quote_bits) {
      quote_idx = TrailingZeroes(quote_bits);
      not_in_str_mask =
          quote_idx == 0 ? 0 : not_in_str_mask >> (64 - quote_idx);
      quote_idx = (quote_idx >> 2) + 1;  // point to next char after '"'
    }
    uint64_t to_one_mask = 0x8888888888888888ull;
    uint64_t rbrace = to_bitmask(vceqq_u8(v, vdupq_n_u8(right))) & to_one_mask &
                      not_in_str_mask;
    uint64_t lbrace = to_bitmask(vceqq_u8(v, vdupq_n_u8(left))) & to_one_mask &
                      not_in_str_mask;

    /* traverse each `right` */
    while (rbrace > 0) {
      rbrace_num++;
      lbrace_num = last_lbrace_num + CountOnes((rbrace - 1) & lbrace);
      if (lbrace_num < rbrace_num) { /* closed */
        pos += (TrailingZeroes(rbrace) >> 2) + 1;
        return true;
      }
      rbrace &= (rbrace - 1);
    }
    lbrace_num = last_lbrace_num + CountOnes(lbrace);
    pos += quote_idx;
    if (quote_bits) {
      SkipString(data, pos, len);
    }
  }

  while (pos < len) {
    uint8_t c = data[pos++];
    if (c == left) {
      lbrace_num++;
    } else if (c == right) {
      rbrace_num++;
    } else if (c == '"') {
      SkipString(data, pos, len);
    } /* else { do nothing } */

    if (lbrace_num < rbrace_num) { /* closed */
      return true;
    }
  }
  /* attach the end of string, but not closed */
  return false;
}

// TODO: optimize by removing bound checking.
sonic_force_inline uint8_t skip_space(const uint8_t *data, size_t &pos,
                                      size_t &, uint64_t &) {
  // fast path for single space
  if (!IsSpace(data[pos++])) return data[pos - 1];
  if (!IsSpace(data[pos++])) return data[pos - 1];

  // current pos is out of block
  while (1) {
    uint64_t nonspace = GetNonSpaceBits(data + pos);
    if (nonspace) {
      pos += TrailingZeroes(nonspace) >> 2;
      return data[pos++];
    } else {
      pos += 16;
    }
  }
  sonic_assert(false && "!should not happen");
}

sonic_force_inline uint8_t skip_space_safe(const uint8_t *data, size_t &pos,
                                           size_t len, size_t &, uint64_t &) {
  while (pos < len && IsSpace(data[pos++]))
    ;
  // if not found, still return the space chars
  return data[pos - 1];
}

}  // namespace neon
}  // namespace internal
}  // namespace sonic_json
