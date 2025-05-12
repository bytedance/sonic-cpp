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

#define VEC_LEN 16

#include <sonic/internal/arch/common/skip_common.h>
#include <sonic/internal/utils.h>

#include "../neon/simd.h"
#include "base.h"
#include "simd.h"

namespace sonic_json {
namespace internal {
namespace sve2_128 {

using sonic_json::internal::common::EqBytes4;
using sonic_json::internal::common::SkipLiteral;

#include "../common/arm_common/skip.inc.h"

// Requires clang vx or GCC>=14
#if (defined(__clang__) && (__clang_major__ >= 14)) || \
    (defined(__GNUC__) && !defined(__clang__) && (__GNUC__ >= 14))

#define USE_SVE_HIST 1

#include <arm_neon_sve_bridge.h>

template <typename T>
sonic_force_inline uint64_t GetStringBits(const T &v, uint64_t &prev_instring,
                                          uint64_t &prev_escaped,
                                          int backslash_count,
                                          int quotes_count) {
  // const T v(data);
  uint64_t escaped = 0;
  uint64_t bs_bits = 0;
  if (backslash_count) {
    uint64_t bs_bits = v.eq('\\');
    escaped = common::GetEscaped<64>(prev_escaped, bs_bits);
  } else {
    escaped = prev_escaped;
    prev_escaped = 0;
  }
  uint64_t in_string = prev_instring;
  if (quotes_count) {
    uint64_t quote_bits = v.eq('"') & ~escaped;
    in_string = PrefixXor(quote_bits) ^ prev_instring;
    prev_instring = uint64_t(static_cast<int64_t>(in_string) >> 63);
  }
  return in_string;
}

sonic_force_inline uint32_t count_chars(const uint8x16_t &data,
                                        svuint8_t &tokens, uint8_t left,
                                        uint8_t right) {
  svuint8_t v = svundef_u8();
  v = svset_neonq_u8(v, data);
  svuint32_t vec32 = svreinterpret_u32(svhistseg_u8(tokens, v));
  return vgetq_lane_u32(svget_neonq_u32(vec32), 0);
}

template <typename T>
sonic_force_inline bool skip_container_sve(const uint8_t *data, size_t &pos,
                                           size_t len, uint8_t left,
                                           uint8_t right) {
  uint64_t prev_instring = 0, prev_escaped = 0, instring = 0;
  int rbrace_num = 0, lbrace_num = 0, last_lbrace_num;
  const uint8_t *p;
  svuint8_t tokens =
      svreinterpret_u8_u32(svdup_n_u32(0x5C22 | (left << 24) | (right << 16)));

  while (pos + 64 <= len) {
    p = data + pos;

    T v(p);
    uint32_t counts = count_chars(v.chunks[0], tokens, left, right);
    // We know they don't overflow, max is 16*4, so we can directly accomulate
    counts += count_chars(v.chunks[1], tokens, left, right);
    counts += count_chars(v.chunks[2], tokens, left, right);
    counts += count_chars(v.chunks[3], tokens, left, right);

#define SKIP_LOOP()                                                        \
  {                                                                        \
    int q_c = counts & 0xff;                                               \
    int b_c = (counts >> 8) & 0xff;                                        \
    int r_c = (counts >> 16) & 0xff;                                       \
    int l_c = (counts >> 24) & 0xff;                                       \
    last_lbrace_num = lbrace_num;                                          \
    instring = GetStringBits<T>(v, prev_instring, prev_escaped, b_c, q_c); \
    uint64_t lbrace = 0;                                                   \
    if (l_c) {                                                             \
      lbrace = v.eq(left) & ~instring;                                     \
    }                                                                      \
    if (r_c) {                                                             \
      uint64_t rbrace = v.eq(right) & ~instring;                           \
      /* traverse each '}' */                                              \
      while (rbrace > 0) {                                                 \
        rbrace_num++;                                                      \
        /* counts the number of {{ that happens before } */                \
        lbrace_num = last_lbrace_num + CountOnes((rbrace - 1) & lbrace);   \
        bool is_closed = lbrace_num < rbrace_num;                          \
        if (is_closed) {                                                   \
          sonic_assert(rbrace_num == lbrace_num + 1);                      \
          pos += TrailingZeroes(rbrace) + 1;                               \
          return true;                                                     \
        }                                                                  \
        rbrace &= (rbrace - 1);                                            \
      }                                                                    \
    }                                                                      \
    lbrace_num = last_lbrace_num + CountOnes(lbrace);                      \
  }
    if (!counts) {
      // Skip, no interesting characters here
      prev_escaped = 0;
    } else if ((counts < 256) && prev_escaped == 0) {
      // counts < 256 means that all the values besides the last byte
      // (quotes) are 0.
      // Only quotes, other vals are 0, need to check the number to see
      // if string is open or not
      // last byte of counts is the string number
      prev_instring ^= (0 - (int)(counts & 1));
      prev_escaped = 0;
    } else if (!(counts & 0xff) && prev_instring) {
      // only backslahes and no quotes, the whole 64 bytes are inside a string
      // so we dont care about left & right
      // just check if the last character is a backslash
      prev_escaped = (p[63] == '\\');
    } else {
      SKIP_LOOP();
    }
    pos += 64;
  }
  uint8_t buf[64] = {0};
  std::memcpy(buf, data + pos, len - pos);
  p = buf;
  T v(p);
  uint32_t counts = count_chars(v.chunks[0], tokens, left, right);
  // We know they don't overflow, max is 16*4, so we can directly accomulate
  counts += count_chars(v.chunks[1], tokens, left, right);
  counts += count_chars(v.chunks[2], tokens, left, right);
  counts += count_chars(v.chunks[3], tokens, left, right);
  SKIP_LOOP();
#undef SKIP_LOOP
  return false;
}

#endif

sonic_force_inline bool SkipContainer(const uint8_t *data, size_t &pos,
                                      size_t len, uint8_t left, uint8_t right) {
  // We use neon for the on demand parser since it is currently faster for
  // comparisons than sve
#ifdef USE_SVE_HIST
  return skip_container_sve<sonic_json::internal::neon::simd8x64<uint8_t>>(
      data, pos, len, left, right);
#else
  return skip_container<sonic_json::internal::neon::simd8x64<uint8_t>>(
      data, pos, len, left, right);
#endif
}

// TODO: optimize by removing bound checking.
sonic_force_inline uint8_t skip_space(const uint8_t *data, size_t &pos,
                                      size_t &, uint64_t &) {
  // fast path for single space
  if (!IsSpace(data[pos++])) return data[pos - 1];
  if (!IsSpace(data[pos++])) return data[pos - 1];

  // current pos is out of block
  while (1) {
    const svbool_t pmatch = GetNonSpaceBits(data + pos);
    const svbool_t ptrue = svptrue_b8();
    if (svptest_any(ptrue, pmatch)) {
      // nonspace = bit position of first non-space token
      const uint64_t nonspace = svcntp_b8(ptrue, svbrkb_z(ptrue, pmatch));
      pos += nonspace;
      return data[pos++];
    } else {
      pos += 16;
    }
  }
  sonic_assert(false && "!should not happen");
}

}  // namespace sve2_128
}  // namespace internal
}  // namespace sonic_json

#undef VEC_LEN
