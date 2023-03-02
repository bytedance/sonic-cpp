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
#include <sonic/macro.h>
#include <stdint.h>

#include <cstddef>

#if defined(__clang__)
#pragma clang attribute push(__attribute__((target("sse,sse2,sse3,sse4.1"))), \
                             apply_to = function)
#elif defined(__GNUG__)
#pragma GCC push_options
#pragma GCC target("sse,sse2,sse3,sse4.1")
#else
#error "Only g++ and clang is supported!"
#endif

namespace sonic_json {

namespace internal {

namespace x86_common {

#define as_m128p(v) ((__m128i *)(v))
#define as_m128c(v) ((const __m128i *)(v))
#define as_m128v(v) (*(const __m128i *)(v))
#define as_uint64v(p) (*(uint64_t *)(p))

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

}  // namespace x86_common

}  // namespace internal

}  // namespace sonic_json

#if defined(__clang__)
#pragma clang attribute pop
#elif defined(__GNUG__)
#pragma GCC pop_options
#endif
