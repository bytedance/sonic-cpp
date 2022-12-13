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

#include "sonic/internal/simd.h"
#include "sonic/macro.h"

namespace sonic_json {
namespace internal {
namespace haswell {

using namespace simd;

// We sometimes call trailing_zero on inputs that are zero,
// but the algorithms do not end up using the returned value.
// Sadly, sanitizers are not smart enough to figure it out.

sonic_force_inline int trailing_zeroes(uint64_t input_num) {
  ////////
  // You might expect the next line to be equivalent to
  // return (int)_tzcnt_u64(input_num);
  // but the generated code differs and might be less efficient?
  ////////
  return __builtin_ctzll(input_num);
}

/* result might be undefined when input_num is zero */
sonic_force_inline uint64_t clear_lowest_bit(uint64_t input_num) {
#if __BMI__
  return _blsr_u64(input_num);
#else
  return input_num & (input_num - 1);
#endif
}

/* result might be undefined when input_num is zero */
sonic_force_inline int leading_zeroes(uint64_t input_num) {
  return __builtin_clzll(input_num);
}

sonic_force_inline long long int count_ones(uint64_t input_num) {
  return __builtin_popcountll(input_num);
}

sonic_force_inline bool add_overflow(uint64_t value1, uint64_t value2,
                                     uint64_t* result) {
  return __builtin_uaddll_overflow(
      value1, value2, reinterpret_cast<unsigned long long*>(result));
}

sonic_force_inline uint64_t prefix_xor(const uint64_t bitmask) {
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

sonic_force_inline bool is_ascii(const simd8x64<uint8_t>& input) {
  return input.reduce_or().is_ascii();
}

template <size_t ChunkSize>
sonic_force_inline void xmemcpy(void* dst_, const void* src_, size_t chunks) {
  std::memcpy(dst_, src_, chunks * ChunkSize);
}

template <>
sonic_force_inline void xmemcpy<32>(void* dst_, const void* src_,
                                    size_t chunks) {
  uint8_t* dst = reinterpret_cast<uint8_t*>(dst_);
  const uint8_t* src = reinterpret_cast<const uint8_t*>(src_);
  size_t blocks = chunks / 4;
  for (size_t i = 0; i < blocks; i++) {
    for (size_t j = 0; j < 4; j++) {
      simd256<uint8_t> s(src);
      s.store(dst);
      src += 32, dst += 32;
    }
  }
  // has remained 1, 2, 3 * 32-bytes
  switch (chunks & 3) {
    case 3: {
      simd256<uint8_t> s(src);
      s.store(dst);
      src += 32, dst += 32;
    }
    /* fall through */
    case 2: {
      simd256<uint8_t> s(src);
      s.store(dst);
      src += 32, dst += 32;
    }
    /* fall through */
    case 1: {
      simd256<uint8_t> s(src);
      s.store(dst);
    }
  }
}

template <>
sonic_force_inline void xmemcpy<16>(void* dst_, const void* src_,
                                    size_t chunks) {
  uint8_t* dst = reinterpret_cast<uint8_t*>(dst_);
  const uint8_t* src = reinterpret_cast<const uint8_t*>(src_);
  size_t blocks = chunks / 8;
  for (size_t i = 0; i < blocks; i++) {
    for (size_t j = 0; j < 4; j++) {
      simd256<uint8_t> s(src);
      s.store(dst);
      src += 32, dst += 32;
    }
  }
  // has remained 1, 2, 3 * 32-bytes
  switch ((chunks / 2) & 3) {
    case 3: {
      simd256<uint8_t> s(src);
      s.store(dst);
      src += 32, dst += 32;
    }
    /* fall through */
    case 2: {
      simd256<uint8_t> s(src);
      s.store(dst);
      src += 32, dst += 32;
    }
    /* fall through */
    case 1: {
      simd256<uint8_t> s(src);
      s.store(dst);
      src += 32, dst += 32;
    }
  }
  // has remained 16 bytes
  if (chunks & 1) {
    simd128<uint8_t> s(src);
    s.store(dst);
  }
}

}  // namespace haswell
}  // namespace internal
}  // namespace sonic_json
