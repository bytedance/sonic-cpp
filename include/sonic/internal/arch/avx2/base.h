// Copyright 2018-2019 The simdjson authors

// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0

// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// This file may have been modified by ByteDance authors. All ByteDance
// Modifications are Copyright 2022 ByteDance Authors.

#pragma once

#include <sonic/macro.h>

#include <cstring>

#include "simd.h"

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

SONIC_PUSH_HASWELL

namespace sonic_json {
namespace internal {
namespace avx2 {

using namespace simd;

// We sometimes call trailing_zero on inputs that are zero,
// but the algorithms do not end up using the returned value.
// Sadly, sanitizers are not smart enough to figure it out.

sonic_force_inline int TrailingZeroes(uint64_t input_num) {
  ////////
  // You might expect the next line to be equivalent to
  // return (int)_tzcnt_u64(input_num);
  // but the generated code differs and might be less efficient?
  ////////
  return __builtin_ctzll(input_num);
}

/* result might be undefined when input_num is zero */
sonic_force_inline uint64_t ClearLowestBit(uint64_t input_num) {
#if __BMI__
  return _blsr_u64(input_num);
#else
  return input_num & (input_num - 1);
#endif
}

/* result might be undefined when input_num is zero */
sonic_force_inline int LeadingZeroes(uint64_t input_num) {
  return __builtin_clzll(input_num);
}

sonic_force_inline long long int CountOnes(uint64_t input_num) {
  return __builtin_popcountll(input_num);
}

sonic_force_inline uint64_t PrefixXor(const uint64_t bitmask) {
  // There should be no such thing with a processor supporting avx2
  // but not clmul.
#if __PCLMUL__
  __m128i all_ones = _mm_set1_epi8('\xFF');
  __m128i result =
      _mm_clmulepi64_si128(_mm_set_epi64x(0ULL, bitmask), all_ones, 0);
  return _mm_cvtsi128_si64(result);
#else
#error "PCLMUL instruction set required. Missing option -mpclmul ?"
  return 0;
#endif
}

sonic_force_inline bool IsAscii(const simd8x64<uint8_t>& input) {
  return input.reduce_or().is_ascii();
}

template <size_t ChunkSize>
sonic_force_inline void Xmemcpy(void* dst_, const void* src_, size_t chunks) {
  std::memcpy(dst_, src_, chunks * ChunkSize);
}

template <>
sonic_force_inline void Xmemcpy<32>(void* dst_, const void* src_,
                                    size_t chunks) {
  uint8_t* dst = reinterpret_cast<uint8_t*>(dst_);
  const uint8_t* src = reinterpret_cast<const uint8_t*>(src_);
  size_t blocks = chunks / 4;
  for (size_t i = 0; i < blocks; i++) {
    for (size_t j = 0; j < 4; j++) {
      simd256<uint8_t> s(src);
      s.store(dst);
      src += 32;
      dst += 32;
    }
  }
  // has remained 1, 2, 3 * 32-bytes
  switch (chunks & 3) {
    case 3: {
      simd256<uint8_t> s(src);
      s.store(dst);
      src += 32;
      dst += 32;
    }
    /* fall through */
    case 2: {
      simd256<uint8_t> s(src);
      s.store(dst);
      src += 32;
      dst += 32;
    }
    /* fall through */
    case 1: {
      simd256<uint8_t> s(src);
      s.store(dst);
    }
  }
}

template <>
sonic_force_inline void Xmemcpy<16>(void* dst_, const void* src_,
                                    size_t chunks) {
  uint8_t* dst = reinterpret_cast<uint8_t*>(dst_);
  const uint8_t* src = reinterpret_cast<const uint8_t*>(src_);
  size_t blocks = chunks / 8;
  for (size_t i = 0; i < blocks; i++) {
    for (size_t j = 0; j < 4; j++) {
      simd256<uint8_t> s(src);
      s.store(dst);
      src += 32;
      dst += 32;
    }
  }
  // has remained 1, 2, 3 * 32-bytes
  switch ((chunks / 2) & 3) {
    case 3: {
      simd256<uint8_t> s(src);
      s.store(dst);
      src += 32;
      dst += 32;
    }
    /* fall through */
    case 2: {
      simd256<uint8_t> s(src);
      s.store(dst);
      src += 32;
      dst += 32;
    }
    /* fall through */
    case 1: {
      simd256<uint8_t> s(src);
      s.store(dst);
      src += 32;
      dst += 32;
    }
  }
  // has remained 16 bytes
  if (chunks & 1) {
    simd128<uint8_t> s(src);
    s.store(dst);
  }
}

namespace {
static sonic_force_inline bool in_page_32(const void* a, const void* b) {
#ifdef SONIC_USE_SANITIZE
  (void)a;
  (void)b;
  return false;
#else
  static constexpr size_t VecLen = 32;
  static constexpr size_t PageSize = 4096;
  size_t addr = (size_t)(a) | (size_t)(b);
  return ((addr) & (PageSize - 1)) <= (PageSize - VecLen);
#endif
}

static sonic_force_inline int cmp_lt_32(const void* _l, const void* _r,
                                        size_t s) {
  auto lhs = static_cast<const uint8_t*>(_l);
  auto rhs = static_cast<const uint8_t*>(_r);
  if (in_page_32(lhs, rhs)) {
#if defined(__GNUC__) && __GNUC__ >= 11
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Warray-bounds"
#endif
    __m256i vec_l = _mm256_loadu_si256((__m256i const*)rhs);
    __m256i vec_r = _mm256_loadu_si256((__m256i const*)lhs);
#if defined(__GNUC__) && __GNUC__ >= 11
#pragma GCC diagnostic pop
#endif
    __m256i ans = _mm256_cmpeq_epi8(vec_l, vec_r);
    int mask = _mm256_movemask_epi8(ans) + 1;
    // mask = mask << (32 -s);
    __asm__("bzhil  %1, %2, %[result]\n\t"
            : [result] "=r"(mask)
            : "r"((int)s), "r"(mask));
    if (mask) {
      int ne_idx = __builtin_ctz(mask);
      // if (lhs[ne_idx] < rhs[ne_idx]) return -1;
      // else return 1;
      return lhs[ne_idx] - rhs[ne_idx];
    } else {
      return 0;
    }
  }
  return std::memcmp(lhs, rhs, s);
}

// slow path
static inline bool is_eq_lt_32_cross_page(const void* _a, const void* _b,
                                          unsigned int s) {
  auto a = static_cast<const uint8_t*>(_a);
  auto b = static_cast<const uint8_t*>(_b);
  if (s >= 16) {
    __m128i vec_a = _mm_loadu_si128((__m128i const*)a);
    __m128i vec_b = _mm_loadu_si128((__m128i const*)b);
    __m128i ans1 = _mm_cmpeq_epi8(vec_a, vec_b);

    vec_a = _mm_loadu_si128((__m128i const*)(a + s - 16));
    vec_b = _mm_loadu_si128((__m128i const*)(b + s - 16));
    __m128i ans2 = _mm_cmpeq_epi8(vec_a, vec_b);

    __m128i ans = _mm_and_si128(ans1, ans2);
    int mask = _mm_movemask_epi8(ans);
    return mask == 0xFFFF;
  }
  // cross page
  if (s >= 8) {
    return __builtin_memcmp(a, b, 8) == 0 &&
           __builtin_memcmp(a + s - 8, b + s - 8, 8) == 0;
  } else if (s >= 4) {
    return __builtin_memcmp(a, b, 4) == 0 &&
           __builtin_memcmp(a + s - 4, b + s - 4, 4) == 0;
  } else if (s >= 2) {
    return __builtin_memcmp(a, b, 2) == 0 &&
           __builtin_memcmp(a + s - 2, b + s - 2, 2) == 0;
  } else {
    return *a == *b;
  }
  return true;
}

static sonic_force_inline bool is_eq_lt_32(const void* _a, const void* _b,
                                           size_t s) {
  auto a = static_cast<const uint8_t*>(_a);
  auto b = static_cast<const uint8_t*>(_b);
  if (in_page_32(a, b)) {
    __m256i vec_a = _mm256_loadu_si256((__m256i const*)a);
    __m256i vec_b = _mm256_loadu_si256((__m256i const*)b);
    __m256i ans = _mm256_cmpeq_epi8(vec_a, vec_b);
    int mask = _mm256_movemask_epi8(ans) + 1;
    // mask = mask << (32 -s);
    __asm__("bzhil  %1, %2, %[result]\n\t"
            : [result] "=r"(mask)
            : "r"((int)s), "r"(mask));
    return mask == 0;
  }
  return is_eq_lt_32_cross_page(a, b, s);
}
}  // namespace

sonic_force_inline bool InlinedMemcmpEq(const void* _a, const void* _b,
                                        size_t s) {
  auto a = static_cast<const uint8_t*>(_a);
  auto b = static_cast<const uint8_t*>(_b);
  if (s == 0) return true;
  if (s < 32) return is_eq_lt_32(a, b, s);
  size_t avx2_end = (s & (~31ULL));

  __m256i vec_a = _mm256_loadu_si256((__m256i const*)(a));
  __m256i vec_b = _mm256_loadu_si256((__m256i const*)(b));
  __m256i ans_1 = _mm256_cmpeq_epi8(vec_a, vec_b);
  // unsigned int mask = _mm256_movemask_epi8(ans_1) + 1;
  // if (mask) return false;

  for (size_t i = 32; i < avx2_end; i += 32) {
    vec_a = _mm256_loadu_si256((__m256i const*)(a + i));
    vec_b = _mm256_loadu_si256((__m256i const*)(b + i));
    __m256i ans = _mm256_cmpeq_epi8(vec_a, vec_b);
    unsigned int mask = _mm256_movemask_epi8(ans) + 1;
    if (mask) return false;
  }
  // no branch for s = x32
  // if (avx2_end == s) return true;
  // s >= 32 overlap
  {
    vec_a = _mm256_loadu_si256((__m256i const*)(a + s - 32));
    vec_b = _mm256_loadu_si256((__m256i const*)(b + s - 32));
    __m256i ans = _mm256_cmpeq_epi8(vec_a, vec_b);
    ans = _mm256_and_si256(ans, ans_1);
    unsigned int mask = _mm256_movemask_epi8(ans) + 1;
    if (mask) return false;
  }
  return true;
}

sonic_force_inline int InlinedMemcmp(const void* _l, const void* _r, size_t s) {
  auto lhs = static_cast<const uint8_t*>(_l);
  auto rhs = static_cast<const uint8_t*>(_r);
  if (s == 0) return 0;
  if (s < 32) return cmp_lt_32(lhs, rhs, s);
  size_t avx2_end = (s & (~31ULL));

  __m256i vec_l = _mm256_loadu_si256((__m256i const*)(lhs));
  __m256i vec_r = _mm256_loadu_si256((__m256i const*)(rhs));
  __m256i ans_1 = _mm256_cmpeq_epi8(vec_l, vec_r);
  uint32_t mask = static_cast<uint32_t>(_mm256_movemask_epi8(ans_1)) + 1;
  if (mask) {
    int ne_idx = __builtin_ctz(mask);
    // if (lhs[ne_idx] < rhs[ne_idx]) return -1;
    // else return 1;
    return lhs[ne_idx] - rhs[ne_idx];
  }

  for (size_t i = 32; i < avx2_end; i += 32) {
    vec_l = _mm256_loadu_si256((__m256i const*)(lhs + i));
    vec_r = _mm256_loadu_si256((__m256i const*)(rhs + i));
    __m256i ans = _mm256_cmpeq_epi8(vec_l, vec_r);
    mask = static_cast<uint32_t>(_mm256_movemask_epi8(ans)) + 1;
    if (mask) {
      int ne_idx = __builtin_ctz(mask);
      // if (lhs[i + ne_idx] < rhs[i + ne_idx]) return -1;
      // else return 1;
      return lhs[i + ne_idx] - rhs[i + ne_idx];
    }
  }
  // no branch for s = x32
  // if (avx2_end == s) return true;
  // s >= 32 overlap
  {
    size_t offset = s - 32;
    vec_l = _mm256_loadu_si256((__m256i const*)(lhs + offset));
    vec_r = _mm256_loadu_si256((__m256i const*)(rhs + offset));
    __m256i ans = _mm256_cmpeq_epi8(vec_l, vec_r);
    // ans = _mm256_and_si256(ans, ans_1);
    unsigned int mask = static_cast<uint32_t>(_mm256_movemask_epi8(ans)) + 1;
    if (mask) {
      int ne_idx = __builtin_ctz(mask);
      return lhs[offset + ne_idx] - rhs[offset + ne_idx];
    }
  }
  return 0;
}

}  // namespace avx2
}  // namespace internal
}  // namespace sonic_json

SONIC_POP_TARGET
