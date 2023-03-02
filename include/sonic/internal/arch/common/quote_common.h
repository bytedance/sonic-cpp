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

// Not check the buffer size of dst, src must be a valid UTF-8 string with
// null-terminator.

namespace sonic_json {
namespace internal {

static sonic_force_inline uint8_t GetEscapeMask4(const char *src) {
  return kNeedEscaped[*(uint8_t *)(src)] |
         (kNeedEscaped[*(uint8_t *)(src + 1)] << 1) |
         (kNeedEscaped[*(uint8_t *)(src + 2)] << 2) |
         (kNeedEscaped[*(uint8_t *)(src + 3)] << 3);
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

}  // namespace internal
}  // namespace sonic_json
