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

#include <immintrin.h>
#include <stdint.h>

#include "sonic/macro.h"

namespace sonic_json {

namespace internal {

#define as_m128p(v) ((__m128i *)(v))
#define as_m128c(v) ((const __m128i *)(v))
#define as_m256c(v) ((const __m256i *)(v))
#define as_m128v(v) (*(const __m128i *)(v))
#define as_uint64v(p) (*(uint64_t *)(p))

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

static const char kVec16xAsc0[16] sonic_align(16) = {
    '0', '0', '0', '0', '0', '0', '0', '0',
    '0', '0', '0', '0', '0', '0', '0', '0',
};

static const uint16_t kVec8x10[8] sonic_align(16) = {
    10, 10, 10, 10, 10, 10, 10, 10,
};

static const uint32_t kVec4x10k[4] sonic_align(16) = {
    10000,
    10000,
    10000,
    10000,
};

static const uint32_t kVec4xDiv10k[4] sonic_align(16) = {
    0xd1b71759,
    0xd1b71759,
    0xd1b71759,
    0xd1b71759,
};

static const uint16_t kVecDivPowers[8] sonic_align(16) = {
    0x20c5, 0x147b, 0x3334, 0x8000, 0x20c5, 0x147b, 0x3334, 0x8000,
};

static const uint16_t kVecShiftPowers[8] sonic_align(16) = {
    0x0080, 0x0800, 0x2000, 0x8000, 0x0080, 0x0800, 0x2000, 0x8000,
};

// Convert num's each digit as packed 16-bit in a vector.
// num's digits as abcdefgh (high bits is 0 if not enough)
// The converted vector is { a, b, c, d, e, f, g, h }
sonic_force_inline __m128i UtoaSSE(uint32_t num) {
  // num(abcdefgh) -> v04 = vector{abcd, efgh, 0, 0, 0, 0, 0, 0}
  __m128i v00 = _mm_cvtsi32_si128(num);
  __m128i v01 = _mm_mul_epu32(v00, as_m128v(kVec4xDiv10k));
  __m128i v02 = _mm_srli_epi64(v01, 45);
  __m128i v03 = _mm_mul_epu32(v02, as_m128v(kVec4x10k));
  __m128i v04 = _mm_sub_epi32(v00, v03);
  __m128i v05 = _mm_unpacklo_epi16(v02, v04);

  // v08 = vector{abcd * 4, abcd * 4, abcd * 4, abcd * 4, efgh * 4, efgh * 4,
  // efgh * 4, efgh * 4}
  __m128i v06 = _mm_slli_epi64(v05, 2);
  __m128i v07 = _mm_unpacklo_epi16(v06, v06);
  __m128i v08 = _mm_unpacklo_epi32(v07, v07);

  // v10 = { a, ab, abc, abcd, e, ef, efg, efgh }
  __m128i v09 = _mm_mulhi_epu16(v08, as_m128v(kVecDivPowers));
  __m128i v10 = _mm_mulhi_epu16(v09, as_m128v(kVecShiftPowers));

  // v12 = { 0, a0, ab0, abc0, 0, e0, ef0, efg0 }
  __m128i v11 = _mm_mullo_epi16(v10, as_m128v(kVec8x10));
  __m128i v12 = _mm_slli_epi64(v11, 16);

  // v13 = { a, b, c, d, e, f, g, h }
  __m128i v13 = _mm_sub_epi16(v10, v12);
  return v13;
}

sonic_force_inline void Copy2Digs(char *dst, const char *src) {
  *(dst) = *(src);
  *(dst + 1) = *(src + 1);
}

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

static sonic_force_inline char *Utoa_8(uint32_t val, char *out) {
  /* convert to digits */
  __m128i v0 = UtoaSSE(val);
  __m128i v1 = _mm_setzero_si128();

  /* convert to bytes, add '0' */
  __m128i v2 = _mm_packus_epi16(v0, v1);
  __m128i v3 = _mm_add_epi8(v2, as_m128v(kVec16xAsc0));

  /* store high 64 bits */
  _mm_storeu_si128(as_m128p(out), v3);
  return out + 8;
}

static sonic_force_inline char *Utoa_16(uint64_t val, char *out) {
  /* remaining digits */
  __m128i v0 = UtoaSSE((uint32_t)(val / 100000000));
  __m128i v1 = UtoaSSE((uint32_t)(val % 100000000));
  __m128i v2 = _mm_packus_epi16(v0, v1);
  __m128i v3 = _mm_add_epi8(v2, as_m128v(kVec16xAsc0));

  /* convert to bytes, add '0' */
  _mm_storeu_si128(as_m128p(out), v3);
  return out + 16;
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