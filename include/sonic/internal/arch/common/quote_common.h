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

#include <cstdint>
#include <cstring>

#include "quote_tables.h"
#include "sonic/dom/flags.h"
#include "sonic/error.h"
#include "unicode_common.h"

// Not check the buffer size of dst, src must be a valid UTF-8 string with
// null-terminator.

namespace sonic_json {
namespace internal {

template <SerializeFlags serializeFlags>
sonic_static_inline void DoEscape(const char*& src, char*& dst, size_t& nb);

namespace common {

// Scalar fallback implementations for x86 dynamic dispatch.
// These are used by x86_ifuncs when CPU lacks AVX2 / (SSE4.2 + PCLMUL).

template <ParseFlags parseFlags = ParseFlags::kParseDefault>
sonic_force_inline size_t parseStringInplace(uint8_t*& src, SonicError& err) {
  constexpr bool kAllowUnescapedControlChars =
      (parseFlags & ParseFlags::kParseAllowUnescapedControlChars) != 0;

  err = kErrorNone;
  uint8_t* dst = src;
  uint8_t* sdst = src;
  while (true) {
    const uint8_t c = *src;
    if (c == '"') {
      *dst = '\0';
      ++src;
      return static_cast<size_t>(dst - sdst);
    }
    if (sonic_unlikely(!kAllowUnescapedControlChars && c <= 0x1f)) {
      err = kParseErrorUnEscaped;
      return 0;
    }
    if (sonic_likely(c != '\\')) {
      *dst++ = *src++;
      continue;
    }

    // Escape sequence.
    const uint8_t escape_char = src[1];
    if (sonic_unlikely(escape_char == 'u')) {
      const uint8_t* src_ptr = src;
      uint8_t* dst_ptr = dst;
      if (!handle_unicode_codepoint(&src_ptr, &dst_ptr)) {
        err = kParseErrorEscapedUnicode;
        return 0;
      }
      src = const_cast<uint8_t*>(src_ptr);
      dst = dst_ptr;
    } else {
      *dst = kEscapedMap[escape_char];
      if (sonic_unlikely(*dst == 0u)) {
        err = kParseErrorEscapedFormat;
        return 0;
      }
      src += 2;
      dst += 1;
    }
  }
}

template <SerializeFlags serializeFlags>
sonic_static_inline char* Quote(const char* src, size_t nb, char* dst) {
  constexpr bool EscapeEmoji =
      (serializeFlags & SerializeFlags::kSerializeEscapeEmoji) != 0;

  *dst++ = '"';
  while (nb > 0) {
    const uint8_t ch = static_cast<uint8_t>(*src);
    const bool need_escape =
        (kNeedEscaped[ch] != 0) || (EscapeEmoji && ((ch & 0xF0) == 0xF0));
    if (sonic_likely(!need_escape)) {
      *dst++ = *src++;
      --nb;
      continue;
    }
    DoEscape<serializeFlags>(src, dst, nb);
  }
  *dst++ = '"';
  return dst;
}

}  // namespace common

static sonic_force_inline uint8_t GetEscapeMask4(const char* src) {
  return kNeedEscaped[*(uint8_t*)(src)] |
         (kNeedEscaped[*(uint8_t*)(src + 1)] << 1) |
         (kNeedEscaped[*(uint8_t*)(src + 2)] << 2) |
         (kNeedEscaped[*(uint8_t*)(src + 3)] << 3);
}

static constexpr char kHexCharsUpper[16] = {'0', '1', '2', '3', '4', '5',
                                            '6', '7', '8', '9', 'A', 'B',
                                            'C', 'D', 'E', 'F'};
static constexpr char kHexCharsLower[16] = {'0', '1', '2', '3', '4', '5',
                                            '6', '7', '8', '9', 'a', 'b',
                                            'c', 'd', 'e', 'f'};

template <bool UpperCase>
sonic_static_inline void writeHex(uint32_t value, char*& dst) {
  const char* hexChars = UpperCase ? kHexCharsUpper : kHexCharsLower;
  *dst++ = '\\';
  *dst++ = 'u';
  *dst++ = hexChars[(value >> 12) & 0xf];
  *dst++ = hexChars[(value >> 8) & 0xf];
  *dst++ = hexChars[(value >> 4) & 0xf];
  *dst++ = hexChars[value & 0xf];
}

template <SerializeFlags serializeFlags>
sonic_static_inline void DoEscape(const char*& src, char*& dst, size_t& nb) {
  constexpr bool UnicodeEscapeUpperCase =
      serializeFlags & SerializeFlags::kSerializeUnicodeEscapeUppercase;
  constexpr bool EscapeEmoji =
      serializeFlags & SerializeFlags::kSerializeEscapeEmoji;

  const auto& quote_tab =
      UnicodeEscapeUpperCase ? kQuoteTabUpperCase : kQuoteTabLowerCase;
  /* get the escape entry, handle consecutive quotes */
  do {
    uint8_t ch = *(uint8_t*)src;
    int nc = quote_tab[ch].n;
    if (nc != 0) {
      std::memcpy(dst, quote_tab[ch].s, 6);
      src++;
      nb--;
      dst += nc;
    } else {
      if constexpr (EscapeEmoji) {
        if (nb < 4) {
          // Not enough bytes for a 4-byte emoji, handle as raw char or error
          *dst++ = *src++;
          nb--;
          continue;
        }
        // TODO: validate the utf8?
        uint32_t unicode = (src[0] & 0x07) << 18 | (src[1] & 0x3f) << 12 |
                           (src[2] & 0x3f) << 6 | (src[3] & 0x3f);
        unicode -= 0x10000;
        writeHex<UnicodeEscapeUpperCase>(0xD800 | ((unicode >> 10) & 0x3FF),
                                         dst);
        writeHex<UnicodeEscapeUpperCase>(0xDC00 | (unicode & 0x3FF), dst);
        src += 4;
        nb -= 4;
      }
    }

    if (nb <= 0) {
      return;
    }

    /* next char is emoji */
    if constexpr (EscapeEmoji) {
      if ((*(uint8_t*)(src)&0xf0) == 0xf0) {
        continue;
      }
    }

    /* copy and find escape chars */
    if (kNeedEscaped[*(uint8_t*)(src)] == 0) {
      return;
    }
  } while (true);
}

}  // namespace internal
}  // namespace sonic_json
