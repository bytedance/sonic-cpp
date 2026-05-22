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
#include <sonic/macro.h>

#include <cstring>

namespace sonic_json {
namespace internal {
namespace riscv_common {

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
  return input_num & (input_num - 1);
}

/* result might be undefined when input_num is zero */
sonic_force_inline int LeadingZeroes(uint64_t input_num) {
  return __builtin_clzll(input_num);
}

sonic_force_inline long long int CountOnes(uint64_t input_num) {
  return __builtin_popcountll(input_num);
}

sonic_force_inline uint64_t PrefixXor(uint64_t bitmask) {
  bitmask ^= bitmask << 1;
  bitmask ^= bitmask << 2;
  bitmask ^= bitmask << 4;
  bitmask ^= bitmask << 8;
  bitmask ^= bitmask << 16;
  bitmask ^= bitmask << 32;
  return bitmask;
}

template <size_t ChunkSize>
sonic_force_inline void Xmemcpy(void* dst_, const void* src_, size_t chunks) {
  std::memcpy(dst_, src_, chunks * ChunkSize);
}

}  // namespace riscv_common
}  // namespace internal
}  // namespace sonic_json