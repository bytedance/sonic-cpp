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

#include <cstring>

#include "sonic/error.h"
#include "sonic/internal/haswell.h"
#include "sonic/internal/simd.h"
#include "sonic/macro.h"

namespace sonic_json {
namespace internal {

using namespace simd;

// kEscapedMap maps the escaped char into origin char, as follows:
// ['/' ] = '/',
// ['"' ] = '"',
// ['b' ] = '\b',
// ['f' ] = '\f',
// ['n' ] = '\n',
// ['r' ] = '\r',
// ['t' ] = '\t',
// ['u' ] = -1,
// ['\\'] = '\\',
static const uint8_t kEscapedMap[256] = {
    0, 0, 0,    0, 0,    0, 0,    0, 0, 0, 0, 0, 0,    0, 0,    0,
    0, 0, 0,    0, 0,    0, 0,    0, 0, 0, 0, 0, 0,    0, 0,    0,
    0, 0, '"',  0, 0,    0, 0,    0, 0, 0, 0, 0, 0,    0, 0,    '/',
    0, 0, 0,    0, 0,    0, 0,    0, 0, 0, 0, 0, 0,    0, 0,    0,

    0, 0, 0,    0, 0,    0, 0,    0, 0, 0, 0, 0, 0,    0, 0,    0,
    0, 0, 0,    0, 0,    0, 0,    0, 0, 0, 0, 0, '\\', 0, 0,    0,
    0, 0, '\b', 0, 0,    0, '\f', 0, 0, 0, 0, 0, 0,    0, '\n', 0,
    0, 0, '\r', 0, '\t', 0, 0,    0, 0, 0, 0, 0, 0,    0, 0,    0,

    0, 0, 0,    0, 0,    0, 0,    0, 0, 0, 0, 0, 0,    0, 0,    0,
    0, 0, 0,    0, 0,    0, 0,    0, 0, 0, 0, 0, 0,    0, 0,    0,
    0, 0, 0,    0, 0,    0, 0,    0, 0, 0, 0, 0, 0,    0, 0,    0,
    0, 0, 0,    0, 0,    0, 0,    0, 0, 0, 0, 0, 0,    0, 0,    0,

    0, 0, 0,    0, 0,    0, 0,    0, 0, 0, 0, 0, 0,    0, 0,    0,
    0, 0, 0,    0, 0,    0, 0,    0, 0, 0, 0, 0, 0,    0, 0,    0,
    0, 0, 0,    0, 0,    0, 0,    0, 0, 0, 0, 0, 0,    0, 0,    0,
    0, 0, 0,    0, 0,    0, 0,    0, 0, 0, 0, 0, 0,    0, 0,    0,
};

// GCC didn't support non-trivial designated initializers C99 extension
struct QuotedChar {
  long n;
  const char *s;
};

static const struct QuotedChar kQuoteTab[256] = {
    // 0x00 ~ 0x1f
    {.n = 6, .s = "\\u0000\0\0"},
    {.n = 6, .s = "\\u0001\0\0"},
    {.n = 6, .s = "\\u0002\0\0"},
    {.n = 6, .s = "\\u0003\0\0"},
    {.n = 6, .s = "\\u0004\0\0"},
    {.n = 6, .s = "\\u0005\0\0"},
    {.n = 6, .s = "\\u0006\0\0"},
    {.n = 6, .s = "\\u0007\0\0"},
    {.n = 2, .s = "\\b\0\0\0\0\0\0"},
    {.n = 2, .s = "\\t\0\0\0\0\0\0"},
    {.n = 2, .s = "\\n\0\0\0\0\0\0"},
    {.n = 6, .s = "\\u000b\0\0"},
    {.n = 2, .s = "\\f\0\0\0\0\0\0"},
    {.n = 2, .s = "\\r\0\0\0\0\0\0"},
    {.n = 6, .s = "\\u000e\0\0"},
    {.n = 6, .s = "\\u000f\0\0"},
    {.n = 6, .s = "\\u0010\0\0"},
    {.n = 6, .s = "\\u0011\0\0"},
    {.n = 6, .s = "\\u0012\0\0"},
    {.n = 6, .s = "\\u0013\0\0"},
    {.n = 6, .s = "\\u0014\0\0"},
    {.n = 6, .s = "\\u0015\0\0"},
    {.n = 6, .s = "\\u0016\0\0"},
    {.n = 6, .s = "\\u0017\0\0"},
    {.n = 6, .s = "\\u0018\0\0"},
    {.n = 6, .s = "\\u0019\0\0"},
    {.n = 6, .s = "\\u001a\0\0"},
    {.n = 6, .s = "\\u001b\0\0"},
    {.n = 6, .s = "\\u001c\0\0"},
    {.n = 6, .s = "\\u001d\0\0"},
    {.n = 6, .s = "\\u001e\0\0"},
    {.n = 6, .s = "\\u001f\0\0"},
    // 0x20 ~ 0x2f
    {0, 0},
    {0, 0},
    {.n = 2, .s = "\\\"\0\0\0\0\0\0"},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    // 0x30 ~ 0x4f
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    // 0x50 ~ 0x5f
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {.n = 2, .s = "\\\\\0\0\0\0\0\0"},
    {0, 0},
    {0, 0},
    {0, 0},
    // 0x60 ~ 0xff
};

static const bool kNeedEscaped[256] = {
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

static sonic_force_inline uint8_t GetEscapeMask4(const char *src) {
  return kNeedEscaped[*(uint8_t *)(src)] |
         (kNeedEscaped[*(uint8_t *)(src + 1)] << 1) |
         (kNeedEscaped[*(uint8_t *)(src + 2)] << 2) |
         (kNeedEscaped[*(uint8_t *)(src + 3)] << 3);
}

static sonic_force_inline int CopyAndGetEscapMask128(const char *src,
                                                     char *dst) {
  simd128<uint8_t> v(reinterpret_cast<const uint8_t *>(src));
  v.store(reinterpret_cast<uint8_t *>(dst));
  return ((v < '\x20') | (v == '\\') | (v == '"')).to_bitmask();
}

static sonic_force_inline int CopyAndGetEscapMask256(const char *src,
                                                     char *dst) {
  simd256<uint8_t> v(reinterpret_cast<const uint8_t *>(src));
  v.store(reinterpret_cast<uint8_t *>(dst));
  return ((v < '\x20') | (v == '\\') | (v == '"')).to_bitmask();
}

sonic_static_inline void DoEscape(const char *&src, char *&dst, size_t &nb) {
  /* get the escape entry, handle consecutive quotes */
  do {
    uint8_t ch = *(uint8_t *)src;
    int nc = kQuoteTab[ch].n;
    std::memcpy(dst, kQuoteTab[ch].s, 8);
    src++;
    nb--;
    dst += nc;
    if (nb <= 0) return;
    /* copy and find escape chars */
    if (kNeedEscaped[*(uint8_t *)(src)] == 0) {
      return;
    }
  } while (true);
}

// Not check the buffer size of dst, src must be a valid UTF-8 string with
// null-terminator.
#define MOVE_N_CHARS(src, N) \
  {                          \
    (src) += (N);            \
    nb -= (N);               \
    dst += (N);              \
  }

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

sonic_static_inline char *Quote(const char *src, size_t nb, char *dst) {
  *dst++ = '"';
  sonic_assert(nb < (1ULL << 32));
  uint32_t mm;
  int cn;

  /* 32-byte loop */
  while (nb >= 32) {
    /* check for matches */
    // TODO: optimize: exploit the simd bitmask in the escape block.
    if ((mm = CopyAndGetEscapMask256(src, dst)) != 0) {
      cn = __builtin_ctz(mm);
      MOVE_N_CHARS(src, cn);
      DoEscape(src, dst, nb);
    } else {
      /* move to next block */
      MOVE_N_CHARS(src, 32);
    }
  }

  if (nb > 0) {
    char tmp_src[64];
    const char *src_r;
#ifdef SONIC_USE_SANITIZE
    if (0) {
#else
    /* This code would cause address sanitizer report heap-buffer-overflow. */
    if (((size_t)(src)&4095) <= (4096 - 64)) {
      src_r = src;
#endif
    } else {
      std::memcpy(tmp_src, src, nb);
      src_r = tmp_src;
    }
    while (nb > 0) {
      mm = CopyAndGetEscapMask256(src_r, dst) & (0xFFFFFFFF >> (32 - nb));
      if (mm) {
        cn = __builtin_ctz(mm);
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

}  // namespace internal
}  // namespace sonic_json
