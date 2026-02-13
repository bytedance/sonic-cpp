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

#include "quote_tables.h"
#include "sonic/dom/flags.h"

// Not check the buffer size of dst, src must be a valid UTF-8 string with
// null-terminator.

namespace sonic_json {
namespace internal {

static sonic_force_inline uint8_t GetEscapeMask4(const char* src) {
  return kNeedEscaped[*(uint8_t*)(src)] |
         (kNeedEscaped[*(uint8_t*)(src + 1)] << 1) |
         (kNeedEscaped[*(uint8_t*)(src + 2)] << 2) |
         (kNeedEscaped[*(uint8_t*)(src + 3)] << 3);
}

static char kHexChars[16] = {'0', '1', '2', '3', '4', '5', '6', '7',
                             '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};

sonic_static_inline void writeHex(uint32_t value, char*& dst) {
  *dst++ = '\\';
  *dst++ = 'u';
  *dst++ = kHexChars[(value >> 12) & 0xf];
  *dst++ = kHexChars[(value >> 8) & 0xf];
  *dst++ = kHexChars[(value >> 4) & 0xf];
  *dst++ = kHexChars[value & 0xf];
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
        writeHex(0xD800 | ((unicode >> 10) & 0x3FF), dst);
        writeHex(0xDC00 | (unicode & 0x3FF), dst);
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
