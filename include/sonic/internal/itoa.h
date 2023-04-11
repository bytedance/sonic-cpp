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

#include <cstddef>
#include <cstdint>

#include "sonic/internal/arch/simd_itoa.h"
#include "sonic/macro.h"

namespace sonic_json {

namespace internal {

static const char kDigits[202] sonic_align(2) =
    "00010203040506070809"
    "10111213141516171819"
    "20212223242526272829"
    "30313233343536373839"
    "40414243444546474849"
    "50515253545556575859"
    "60616263646566676869"
    "70717273747576777879"
    "80818283848586878889"
    "90919293949596979899";

sonic_force_inline void Copy2Digs(char *dst, const char *src) {
  *(dst) = *(src);
  *(dst + 1) = *(src + 1);
}

#if defined(__GNUG__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Warray-bounds"
#endif

sonic_force_inline char *Utoa_1_8(char *out, uint32_t val) {
  uint32_t hi, lo, a, b, c, d, lz;

  if (val < 100) {  // 1 ~ 2 digits
    lz = val < 10;
    Copy2Digs(out, &kDigits[val * 2 + lz]);
    out -= lz;
    return out + 2;
  } else if (val < 10000) {  // 3 ~ 4 digits
    hi = val / 100;
    lo = val % 100;
    lz = hi < 10;
    Copy2Digs(out, &kDigits[hi * 2 + lz]);
    out -= lz;
    Copy2Digs(out + 2, &kDigits[lo * 2]);
    return out + 4;
  } else if (val < 1000000) {  // 5 ~ 6 digits
    hi = val / 10000;
    lo = val % 10000;
    lz = hi < 10;
    Copy2Digs(out, &kDigits[hi * 2 + lz]);
    out -= lz;
    a = lo / 100;
    b = lo % 100;
    Copy2Digs(out + 2, &kDigits[a * 2]);
    Copy2Digs(out + 4, &kDigits[b * 2]);
    return out + 6;
  } else {  // 7 ~ 8 digits
    hi = val / 10000;
    lo = val % 10000;
    a = hi / 100;
    b = hi % 100;
    c = lo / 100;
    d = lo % 100;
    lz = a < 10;
    Copy2Digs(out, &kDigits[a * 2 + lz]);
    out -= lz;
    Copy2Digs(out + 2, &kDigits[b * 2]);
    Copy2Digs(out + 4, &kDigits[c * 2]);
    Copy2Digs(out + 6, &kDigits[d * 2]);
    return out + 8;
  }
}

sonic_force_inline char *U64toa_17_20(char *out, uint64_t val) {
  uint64_t lo = val % 10000000000000000;
  uint32_t hi = (uint32_t)(val / 10000000000000000);

  uint32_t aa, bb, lz;
  if (hi < 100) {  // 2 digits
    lz = hi < 10;
    Copy2Digs(out, &kDigits[hi * 2 + lz]);
    out += 2 - lz;
  } else if (hi < 10000) {  // 4 digits like aabb
    aa = hi / 100;
    bb = hi % 100;
    lz = aa < 10;
    Copy2Digs(out, &kDigits[aa * 2 + lz]);
    out -= lz;
    Copy2Digs(out + 2, &kDigits[bb * 2]);
    out += 4;
  }
  return Utoa_16(lo, out);
}
#if defined(__GNUG__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif

sonic_force_inline char *U64toa(char *out, uint64_t val) {
  if (sonic_likely(val < 100000000)) {  // 1 ~ 8 digits
    return Utoa_1_8(out, (uint32_t)val);
  } else if (sonic_likely(val < 10000000000000000)) {  // 8 ~ 16 digits
    uint32_t hi, lo;
    hi = ((uint32_t)(val / 100000000));
    lo = ((uint32_t)(val % 100000000));
    return Utoa_8(lo, Utoa_1_8(out, hi));
  } else {  // 17 ~ 20 digits
    return U64toa_17_20(out, val);
  }
}

sonic_force_inline char *I64toa(char *buf, int64_t val) {
  size_t neg = val < 0;
  *buf = '-';
  return U64toa(buf + neg, neg ? (uint64_t)(-val) : (uint64_t)val);
}

}  // namespace internal

}  // namespace sonic_json
