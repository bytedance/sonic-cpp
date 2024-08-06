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
#include <sonic/macro.h>

#include <cstring>
#include <vector>

#include "../quote_common.h"
#include "../quote_tables.h"
#include "base.h"
#include "simd.h"

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

#ifndef VEC_LEN
#error "You should define VEC_LEN before including quote.h!"
#endif

#define MOVE_N_CHARS(src, N) \
  {                          \
    (src) += (N);            \
    nb -= (N);               \
    dst += (N);              \
  }

namespace sonic_json {
namespace internal {
namespace arm_common {

static sonic_force_inline uint64_t CopyAndGetEscapMask128(const char *src,
                                                          char *dst) {
  uint8x16_t v = vld1q_u8(reinterpret_cast<const uint8_t *>(src));
  vst1q_u8(reinterpret_cast<uint8_t *>(dst), v);

  uint8x16_t m1 = vceqq_u8(v, vdupq_n_u8('\\'));
  uint8x16_t m2 = vceqq_u8(v, vdupq_n_u8('"'));
  uint8x16_t m3 = vcltq_u8(v, vdupq_n_u8('\x20'));

  uint8x16_t m4 = vorrq_u8(m1, m2);
  uint8x16_t m5 = vorrq_u8(m3, m4);

  return to_bitmask(m5);
}

sonic_static_inline char *Quote(const char *src, size_t nb, char *dst) {
  *dst++ = '"';
  sonic_assert(nb < (1ULL << 32));
  uint64_t mm;
  int cn;

  /* VEC_LEN byte loop */
  while (nb >= VEC_LEN) {
    /* check for matches */
    // TODO: optimize: exploit the simd bitmask in the escape block.
    if ((mm = CopyAndGetEscapMask128(src, dst)) != 0) {
      // cn = __builtin_ctz(mm);
      cn = TrailingZeroes(mm) >> 2;
      MOVE_N_CHARS(src, cn);
      DoEscape(src, dst, nb);
    } else {
      /* move to next block */
      MOVE_N_CHARS(src, VEC_LEN);
    }
  }

  if (nb > 0) {
    char tmp_src[64];
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
    while (nb > 0) {
      mm = CopyAndGetEscapMask128(src_r, dst) &
           (0xFFFFFFFFFFFFFFFF >> ((VEC_LEN - nb) << 2));
      if (mm) {
        cn = TrailingZeroes(mm) >> 2;
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

}  // namespace arm_common
}  // namespace internal
}  // namespace sonic_json

#undef MOVE_N_CHARS
