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

#include <sonic/error.h>

#include <cstring>

#include "../common/quote_common.h"
#include "../common/quote_tables.h"
#include "base.h"
#include "unicode.h"

// Not check the buffer size of dst, src must be a valid UTF-8 string with
// null-terminator.

#define VEC_LEN 16
#define PAGE_SIZE 4096

namespace sonic_json {
namespace internal {
namespace rvv_128 {

#define MOVE_N_CHARS(src, N) \
  {                          \
    (src) += (N);            \
    nb -= (N);               \
    dst += (N);              \
  }

static sonic_force_inline long CopyAndGetEscapMask128(const char *src,
                                                      char *dst) {
  vuint8m1_t v =
      __riscv_vle8_v_u8m1(reinterpret_cast<const uint8_t *>(src), 16);
  __riscv_vse8_v_u8m1(reinterpret_cast<uint8_t *>(dst), v, 16);

  vbool8_t m1 = __riscv_vmseq_vv_u8m1_b8(v, __riscv_vmv_v_x_u8m1('\\', 16), 16);
  vbool8_t m2 = __riscv_vmseq_vv_u8m1_b8(v, __riscv_vmv_v_x_u8m1('"', 16), 16);
  vbool8_t m3 =
      __riscv_vmsltu_vv_u8m1_b8(v, __riscv_vmv_v_x_u8m1('\x20', 16), 16);

  vbool8_t m4 = __riscv_vmor_mm_b8(m1, m2, 16);
  vbool8_t m5 = __riscv_vmor_mm_b8(m3, m4, 16);

  return __riscv_vfirst_m_b8(m5, 16);
}

sonic_static_inline char *Quote(const char *src, size_t nb, char *dst) {
  *dst++ = '"';
  sonic_assert(nb < (1ULL << 32));
  long mm;
  int cn;

  /* VEC_LEN byte loop */
  while (nb >= VEC_LEN) {
    /* check for matches */
    // TODO: optimize: exploit the simd bitmask in the escape block.
    if ((mm = cn = CopyAndGetEscapMask128(src, dst)) >= 0) {
      /* move to next block */
      MOVE_N_CHARS(src, cn);
      DoEscape(src, dst, nb);
    } else {
      /* move to next block */
      MOVE_N_CHARS(src, VEC_LEN);
    }
  }

  if (nb > 0) {
    char tmp_src[64] = {127};
    const char *src_r;
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
    while (int(nb) > 0) {
      long tmp = CopyAndGetEscapMask128(src_r, dst);
      cn = mm = (tmp >= static_cast<long>(nb) ? -1 : tmp);
      if (mm >= 0) {
        MOVE_N_CHARS(src_r, cn);
        DoEscape(src_r, dst, nb);
      } else {
        dst += nb;
        nb = 0;
      }
    }
  }

  *dst++ = '"';
  return dst;
}

sonic_force_inline size_t parseStringInplace(uint8_t *&src, SonicError &err) {
#define SONIC_REPEAT8(v) {v v v v v v v v}

  uint8_t *dst = src;
  uint8_t *sdst = src;
  while (1) {
  find:
    auto block = StringBlock::Find(src);
    if (block.HasQuoteFirst()) {
      int idx = block.QuoteIndex();
      src += idx;
      *src++ = '\0';
      return src - sdst - 1;
    }
    if (block.HasUnescaped()) {
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
      if (!handle_unicode_codepoint(const_cast<const uint8_t **>(&src), &dst)) {
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
    // fast path for continous escaped chars
    if (*src == '\\') {
      bs_dist = 0;
      goto cont;
    }

  find_and_move:
    // Copy the next n bytes, and find the backslash and quote in them.
    vuint8m1_t v = __riscv_vle8_v_u8m1(src, 16);
    block = StringBlock::Find(v);
    // If the next thing is the end quote, copy and return
    if (block.HasQuoteFirst()) {
      // we encountered quotes first. Move dst to point to quotes and exit
      while (1) {
        SONIC_REPEAT8(if (sonic_unlikely(*src == '"')) break;
                      else { *dst++ = *src++; });
      }
      *dst = '\0';
      src++;
      return dst - sdst;
    }
    if (block.HasUnescaped()) {
      err = kParseErrorUnEscaped;
      return 0;
    }
    if (!block.HasBackslash()) {
      /* they are the same. Since they can't co-occur, it means we
       * encountered neither. */
      __riscv_vse8_v_u8m1(dst, v, 16);
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

}  // namespace rvv_128
}  // namespace internal
}  // namespace sonic_json