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

#include <sonic/error.h>
#include <sonic/macro.h>

#include <cstring>
#include <vector>

#include "../common/quote_common.h"
#include "../common/quote_tables.h"
#include "base.h"
#include "simd.h"
#include "unicode.h"

#ifndef PAGE_SIZE
#define PAGE_SIZE 4096
#endif

#ifdef __GNUC__
#if defined(__SANITIZE_THREAD__) || defined(__SANITIZE_ADDRESS__) || \
    defined(__SANITIZE_LEAK__) || defined(__SANITIZE_UNDEFINED__)
#ifndef SONIC_USE_SANITIZE
#define SONIC_USE_SANITIZE
#endif
#endif
#endif

#if defined(__clang__)
#if defined(__has_feature)
#if __has_feature(address_sanitizer) || __has_feature(thread_sanitizer) || \
    __has_feature(memory_sanitizer) ||                                     \
    __has_feature(undefined_behavior_sanitizer) ||                         \
    __has_feature(leak_sanitizer)
#ifndef SONIC_USE_SANITIZE
#define SONIC_USE_SANITIZE
#endif
#endif
#endif
#endif

#define MOVE_N_CHARS(src, N) \
  {                          \
    (src) += (N);            \
    nb -= (N);               \
    dst += (N);              \
  }

namespace sonic_json {
namespace internal {
namespace riscv {

using sonic_json::internal::common::handle_unicode_codepoint;

static sonic_force_inline uint64_t CopyAndGetEscapMask128(const char* src,
                                                          char* dst,
                                                          bool escape_emoji) {
  simd8<uint8_t> v =
      simd8<uint8_t>::load(reinterpret_cast<const uint8_t*>(src));
  v.store(reinterpret_cast<uint8_t*>(dst));

  simd8<uint8_t> bs = simd8<uint8_t>::splat('\\');
  simd8<uint8_t> quote = simd8<uint8_t>::splat('"');
  simd8<uint8_t> ctrl = simd8<uint8_t>::splat('\x20');

  simd8<bool> m1 = (v == bs);
  simd8<bool> m2 = (v == quote);
  simd8<bool> m3 = (v < ctrl);

  simd8<bool> mask = m1 | m2;
  mask = mask | m3;
  if (escape_emoji) {
    simd8<uint8_t> emoji = simd8<uint8_t>::splat(0xF0);
    simd8<bool> m_emoji = (v >= emoji);
    mask = mask | m_emoji;
  }

  return mask.to_bitmask();
}

sonic_static_inline char* Quote(const char* src, size_t nb, char* dst,
                                bool escape_emoji) {
  *dst++ = '"';
  sonic_assert(nb < (1ULL << 32));
  uint64_t mm;
  int cn;

  /* VEC_LEN byte loop */
  while (nb >= VEC_LEN) {
    /* check for matches */
    if ((mm = CopyAndGetEscapMask128(src, dst, escape_emoji)) != 0) {
      cn = TrailingZeroes(mm) >> 2;
      MOVE_N_CHARS(src, cn);
      DoEscape(src, dst, nb, escape_emoji);
    } else {
      /* move to next block */
      MOVE_N_CHARS(src, VEC_LEN);
    }
  }

  if (nb > 0) {
    char tmp_src[64];
    const char* src_r;
#ifdef SONIC_USE_SANITIZE
    if (0) {
#else
    /* This code would cause address sanitizer report heap-buffer-overflow. */
    if (((size_t)(src) & (PAGE_SIZE - 1)) <= (PAGE_SIZE - 64)) {
      src_r = src;
#endif
    } else {
      std::memcpy(tmp_src, src, nb);
      src_r = tmp_src;
    }
    while (nb > 0) {
      mm = CopyAndGetEscapMask128(src_r, dst, escape_emoji) &
           (0xFFFFFFFFFFFFFFFF >> ((VEC_LEN - nb) << 2));
      if (mm) {
        cn = TrailingZeroes(mm) >> 2;
        MOVE_N_CHARS(src_r, cn);
        DoEscape(src_r, dst, nb, escape_emoji);
      } else {
        dst += nb;
        nb = 0;
      }
    }
  }

  *dst++ = '"';
  return dst;
}

sonic_force_inline size_t
parseStringInplace(uint8_t*& src, SonicError& err,
                   bool allow_unescaped_control_chars = false) {
#define SONIC_REPEAT8(v) {v v v v v v v v}
  uint8_t* dst = src;
  uint8_t* sdst = src;
  while (1) {
  find:
    auto block = StringBlock::Find(src);
    if (block.HasQuoteFirst(allow_unescaped_control_chars)) {
      int idx = block.QuoteIndex();
      src += idx;
      *src++ = '\0';
      return src - sdst - 1;
    }
    if (!allow_unescaped_control_chars && block.HasUnescaped()) {
      err = kParseErrorUnEscaped;
      return 0;
    }
    if (!block.HasBackslash()) {
      src += VEC_LEN;
      goto find;
    }

    /* find out where the backspace is */
    auto bs_dist = block.BsIndex();
    src += bs_dist;
    dst = src;
  cont:
    uint8_t escape_char = src[1];
    if (sonic_unlikely(escape_char == 'u')) {
      if (!handle_unicode_codepoint(const_cast<const uint8_t**>(&src), &dst)) {
        err = kParseErrorEscapedUnicode;
        return 0;
      }
    } else {
      *dst = kEscapedMap[escape_char];
      if (sonic_unlikely(*dst == 0u)) {
        err = kParseErrorEscapedFormat;
        return 0;
      }
      src += 2;
      dst += 1;
    }
    // fast path for continuous escaped chars
    if (*src == '\\') {
      bs_dist = 0;
      goto cont;
    }

  find_and_move:
    // Copy the next n bytes, and find the backslash and quote in them.
    simd8<uint8_t> v = simd8<uint8_t>::load(src);
    block = StringBlock::Find(v);
    // If the next thing is the end quote, copy and return
    if (block.HasQuoteFirst(allow_unescaped_control_chars)) {
      // we encountered quotes first. Move dst to point to quotes and exit
      while (1) {
        SONIC_REPEAT8(if (sonic_unlikely(*src == '"')) break;
                      else { *dst++ = *src++; });
      }
      *dst = '\0';
      src++;
      return dst - sdst;
    }
    if (!allow_unescaped_control_chars && block.HasUnescaped()) {
      err = kParseErrorUnEscaped;
      return 0;
    }
    if (!block.HasBackslash()) {
      /* they are the same. Since they can't co-occur, it means we
       * encountered neither. */
      v.store(dst);
      src += VEC_LEN;
      dst += VEC_LEN;
      goto find_and_move;
    }
    while (1) {
      SONIC_REPEAT8(if (sonic_unlikely(*src == '\\')) break;
                    else { *dst++ = *src++; });
    }
    goto cont;
  }
  sonic_assert(false);
#undef SONIC_REPEAT8
}

}  // namespace riscv
}  // namespace internal
}  // namespace sonic_json

#undef VEC_LEN
#undef MOVE_N_CHARS
