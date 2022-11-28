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
#include <x86intrin.h>

#include "sonic/internal/simd.h"

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
#error "BMI instruction set required. Missing option -mbmi ?"
  return 0;
#endif
}

/* result might be undefined when input_num is zero */
sonic_force_inline int leading_zeroes(uint64_t input_num) {
  return int(_lzcnt_u64(input_num));
}

sonic_force_inline long long int count_ones(uint64_t input_num) {
  return _popcnt64(input_num);
}

sonic_force_inline bool add_overflow(uint64_t value1, uint64_t value2,
                                     uint64_t *result) {
  return __builtin_uaddll_overflow(
      value1, value2, reinterpret_cast<unsigned long long *>(result));
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

sonic_force_inline bool is_ascii(const simd8x64<uint8_t> &input) {
  return input.reduce_or().is_ascii();
}

}  // namespace haswell
}  // namespace internal
}  // namespace sonic_json
