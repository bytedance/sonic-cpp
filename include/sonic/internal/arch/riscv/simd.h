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

// RISC-V scalar simd facade with optional Zbb byte-search helpers.

#pragma once

#include <sonic/macro.h>

#include <cstdint>
#include <cstring>

namespace sonic_json {
namespace internal {
namespace riscv {

#if defined(__riscv) && defined(__riscv_xlen) && (__riscv_xlen == 64) && \
    defined(__riscv_zbb)
#define SONIC_RISCV_SIMD_HAS_ZBB 1

namespace riscv_zbb_detail {

static constexpr uint64_t kByteMsb = 0x8080808080808080ULL;
static constexpr uint64_t kByteLsb = 0x0101010101010101ULL;
static constexpr uint64_t kMovemaskMultiplier = 0x0002040810204081ULL;

sonic_force_inline uint64_t load_u64(const uint8_t* ptr) {
  uint64_t value;
  std::memcpy(&value, ptr, sizeof(value));
  return value;
}

sonic_force_inline uint64_t orc_b(uint64_t value) {
  uint64_t result;
  __asm__ volatile("orc.b %0, %1" : "=r"(result) : "r"(value));
  return result;
}

sonic_force_inline uint32_t byte_msb_to_mask(uint64_t value) {
  return static_cast<uint32_t>(((value & kByteMsb) * kMovemaskMultiplier) >>
                               56);
}

sonic_force_inline uint32_t nonzero_byte_mask(uint64_t value) {
  return byte_msb_to_mask(orc_b(value));
}

sonic_force_inline uint32_t zero_byte_mask(uint64_t value) {
  return byte_msb_to_mask(~orc_b(value));
}

sonic_force_inline uint32_t bool_mask16(const uint8_t* ptr) {
  uint32_t low = nonzero_byte_mask(load_u64(ptr));
  uint32_t high = nonzero_byte_mask(load_u64(ptr + 8));
  return low | (high << 8);
}

sonic_force_inline uint32_t eq_mask16(const uint8_t* ptr, uint8_t value) {
  uint64_t repeated = kByteLsb * value;
  uint32_t low = zero_byte_mask(load_u64(ptr) ^ repeated);
  uint32_t high = zero_byte_mask(load_u64(ptr + 8) ^ repeated);
  return low | (high << 8);
}

sonic_force_inline uint32_t lteq_mask16(const uint8_t* ptr, uint8_t value) {
  if (value == 0xFFu) {
    return 0xFFFFu;
  }
  if (value == 0x1Fu) {
    constexpr uint64_t high_three_bits = 0xE0E0E0E0E0E0E0E0ULL;
    uint32_t low = zero_byte_mask(load_u64(ptr) & high_three_bits);
    uint32_t high = zero_byte_mask(load_u64(ptr + 8) & high_three_bits);
    return low | (high << 8);
  }
  if (value == 0x7Fu) {
    uint32_t low = byte_msb_to_mask(~load_u64(ptr));
    uint32_t high = byte_msb_to_mask(~load_u64(ptr + 8));
    return low | (high << 8);
  }

  uint32_t mask = 0;
  for (int i = 0; i < 16; i++) {
    if (ptr[i] <= value) mask |= (1u << i);
  }
  return mask;
}

}  // namespace riscv_zbb_detail

#else
#define SONIC_RISCV_SIMD_HAS_ZBB 0
#endif

template <typename T>
struct simd8;

// SIMD byte mask type (returned by things like eq and gt)
template <>
struct simd8<bool> {
  typedef uint16_t bitmask_t;
  typedef uint32_t bitmask2_t;

  uint8_t data[16];

  static sonic_force_inline simd8<bool> splat(bool _value) {
    simd8<bool> r;
    std::memset(r.data, _value ? 0xFF : 0x00, 16);
    return r;
  }

  sonic_force_inline simd8() { std::memset(data, 0, 16); }
  sonic_force_inline simd8(bool _value) {
    std::memset(data, _value ? 0xFF : 0x00, 16);
  }

  // Construct from raw array
  sonic_force_inline explicit simd8(const uint8_t d[16]) {
    std::memcpy(data, d, 16);
  }

  // We return uint64_t with 4 bits per byte (same as NEON movemask format)
  // so that TrailingZeroes(x) >> 2 gives the correct byte index.
  sonic_force_inline uint64_t to_bitmask() const {
#if SONIC_RISCV_SIMD_HAS_ZBB
    uint32_t bits = riscv_zbb_detail::bool_mask16(data);
    uint64_t mask = 0;
    for (int i = 0; i < 16; i++) {
      if (bits & (1u << i)) {
        mask |= (0xFULL << (i * 4));
      }
    }
    return mask;
#else
    uint64_t mask = 0;
    for (int i = 0; i < 16; i++) {
      if (data[i]) {
        mask |= (0xFULL << (i * 4));
      }
    }
    return mask;
#endif
  }

  sonic_force_inline bool any() const {
#if SONIC_RISCV_SIMD_HAS_ZBB
    return (riscv_zbb_detail::load_u64(data) |
            riscv_zbb_detail::load_u64(data + 8)) != 0;
#else
    for (int i = 0; i < 16; i++) {
      if (data[i]) return true;
    }
    return false;
#endif
  }

  sonic_force_inline simd8<bool> operator~() const {
    simd8<bool> r;
    for (int i = 0; i < 16; i++) r.data[i] = ~data[i];
    return r;
  }

  sonic_force_inline simd8<bool> operator|(const simd8<bool> other) const {
    simd8<bool> r;
    for (int i = 0; i < 16; i++) r.data[i] = data[i] | other.data[i];
    return r;
  }
  sonic_force_inline simd8<bool> operator&(const simd8<bool> other) const {
    simd8<bool> r;
    for (int i = 0; i < 16; i++) r.data[i] = data[i] & other.data[i];
    return r;
  }
  sonic_force_inline simd8<bool> operator^(const simd8<bool> other) const {
    simd8<bool> r;
    for (int i = 0; i < 16; i++) r.data[i] = data[i] ^ other.data[i];
    return r;
  }
  sonic_force_inline simd8<bool> bit_andnot(const simd8<bool> other) const {
    simd8<bool> r;
    for (int i = 0; i < 16; i++) r.data[i] = data[i] & ~other.data[i];
    return r;
  }
  sonic_force_inline simd8<bool>& operator|=(const simd8<bool> other) {
    for (int i = 0; i < 16; i++) data[i] |= other.data[i];
    return *this;
  }
  sonic_force_inline simd8<bool>& operator&=(const simd8<bool> other) {
    for (int i = 0; i < 16; i++) data[i] &= other.data[i];
    return *this;
  }
  sonic_force_inline simd8<bool>& operator^=(const simd8<bool> other) {
    for (int i = 0; i < 16; i++) data[i] ^= other.data[i];
    return *this;
  }
};

// Free function for skip.inc.h compatibility (same as NEON's to_bitmask)
sonic_force_inline uint64_t to_bitmask(const simd8<bool>& v) {
  return v.to_bitmask();
}

// Unsigned bytes
template <>
struct simd8<uint8_t> {
  static const int SIZE = 16;
  uint8_t data[16];

  static sonic_force_inline simd8<uint8_t> splat(uint8_t _value) {
    simd8<uint8_t> r;
    std::memset(r.data, _value, 16);
    return r;
  }
  static sonic_force_inline simd8<uint8_t> zero() {
    simd8<uint8_t> r;
    std::memset(r.data, 0, 16);
    return r;
  }
  static sonic_force_inline simd8<uint8_t> load(const uint8_t* values) {
    simd8<uint8_t> r;
    std::memcpy(r.data, values, 16);
    return r;
  }

  sonic_force_inline simd8() { std::memset(data, 0, 16); }
  sonic_force_inline simd8(const uint8_t values[16]) {
    std::memcpy(data, values, 16);
  }
  sonic_force_inline simd8(uint8_t _value) { std::memset(data, _value, 16); }
  // Conversion from bool mask
  sonic_force_inline simd8(const simd8<bool>& other) {
    std::memcpy(data, other.data, 16);
  }
  sonic_force_inline simd8(uint8_t v0, uint8_t v1, uint8_t v2, uint8_t v3,
                           uint8_t v4, uint8_t v5, uint8_t v6, uint8_t v7,
                           uint8_t v8, uint8_t v9, uint8_t v10, uint8_t v11,
                           uint8_t v12, uint8_t v13, uint8_t v14, uint8_t v15) {
    data[0] = v0;
    data[1] = v1;
    data[2] = v2;
    data[3] = v3;
    data[4] = v4;
    data[5] = v5;
    data[6] = v6;
    data[7] = v7;
    data[8] = v8;
    data[9] = v9;
    data[10] = v10;
    data[11] = v11;
    data[12] = v12;
    data[13] = v13;
    data[14] = v14;
    data[15] = v15;
  }

  // Repeat 16 values as many times as necessary (usually for lookup tables)
  static sonic_force_inline simd8<uint8_t> repeat_16(
      uint8_t v0, uint8_t v1, uint8_t v2, uint8_t v3, uint8_t v4, uint8_t v5,
      uint8_t v6, uint8_t v7, uint8_t v8, uint8_t v9, uint8_t v10, uint8_t v11,
      uint8_t v12, uint8_t v13, uint8_t v14, uint8_t v15) {
    return simd8<uint8_t>(v0, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12,
                          v13, v14, v15);
  }

  sonic_force_inline void store(uint8_t dst[16]) const {
    std::memcpy(dst, data, 16);
  }

  // Saturated math
  sonic_force_inline simd8<uint8_t> saturating_add(
      const simd8<uint8_t> other) const {
    simd8<uint8_t> r;
    for (int i = 0; i < 16; i++) {
      uint16_t sum = (uint16_t)data[i] + other.data[i];
      r.data[i] = sum > 255 ? 255 : (uint8_t)sum;
    }
    return r;
  }
  sonic_force_inline simd8<uint8_t> saturating_sub(
      const simd8<uint8_t> other) const {
    simd8<uint8_t> r;
    for (int i = 0; i < 16; i++) {
      r.data[i] = data[i] > other.data[i] ? data[i] - other.data[i] : 0;
    }
    return r;
  }

  // Arithmetic
  sonic_force_inline simd8<uint8_t> operator+(
      const simd8<uint8_t> other) const {
    simd8<uint8_t> r;
    for (int i = 0; i < 16; i++) r.data[i] = data[i] + other.data[i];
    return r;
  }
  sonic_force_inline simd8<uint8_t> operator-(
      const simd8<uint8_t> other) const {
    simd8<uint8_t> r;
    for (int i = 0; i < 16; i++) r.data[i] = data[i] - other.data[i];
    return r;
  }
  sonic_force_inline simd8<uint8_t>& operator+=(const simd8<uint8_t> other) {
    for (int i = 0; i < 16; i++) data[i] += other.data[i];
    return *this;
  }
  sonic_force_inline simd8<uint8_t>& operator-=(const simd8<uint8_t> other) {
    for (int i = 0; i < 16; i++) data[i] -= other.data[i];
    return *this;
  }

  // Bitwise
  sonic_force_inline simd8<uint8_t> operator|(
      const simd8<uint8_t> other) const {
    simd8<uint8_t> r;
    for (int i = 0; i < 16; i++) r.data[i] = data[i] | other.data[i];
    return r;
  }
  sonic_force_inline simd8<uint8_t> operator&(
      const simd8<uint8_t> other) const {
    simd8<uint8_t> r;
    for (int i = 0; i < 16; i++) r.data[i] = data[i] & other.data[i];
    return r;
  }
  sonic_force_inline simd8<uint8_t> operator^(
      const simd8<uint8_t> other) const {
    simd8<uint8_t> r;
    for (int i = 0; i < 16; i++) r.data[i] = data[i] ^ other.data[i];
    return r;
  }
  sonic_force_inline simd8<uint8_t> bit_andnot(
      const simd8<uint8_t> other) const {
    simd8<uint8_t> r;
    for (int i = 0; i < 16; i++) r.data[i] = data[i] & ~other.data[i];
    return r;
  }
  sonic_force_inline simd8<uint8_t> operator~() const {
    simd8<uint8_t> r;
    for (int i = 0; i < 16; i++) r.data[i] = ~data[i];
    return r;
  }
  sonic_force_inline simd8<uint8_t>& operator|=(const simd8<uint8_t> other) {
    for (int i = 0; i < 16; i++) data[i] |= other.data[i];
    return *this;
  }
  sonic_force_inline simd8<uint8_t>& operator&=(const simd8<uint8_t> other) {
    for (int i = 0; i < 16; i++) data[i] &= other.data[i];
    return *this;
  }
  sonic_force_inline simd8<uint8_t>& operator^=(const simd8<uint8_t> other) {
    for (int i = 0; i < 16; i++) data[i] ^= other.data[i];
    return *this;
  }

  // Comparison
  friend sonic_force_inline simd8<bool> operator==(const simd8<uint8_t> lhs,
                                                   const simd8<uint8_t> rhs) {
    simd8<bool> r;
    for (int i = 0; i < 16; i++)
      r.data[i] = (lhs.data[i] == rhs.data[i]) ? 0xFF : 0x00;
    return r;
  }
  sonic_force_inline simd8<bool> operator<=(const simd8<uint8_t> other) const {
    simd8<bool> r;
    for (int i = 0; i < 16; i++)
      r.data[i] = (data[i] <= other.data[i]) ? 0xFF : 0x00;
    return r;
  }
  sonic_force_inline simd8<bool> operator>=(const simd8<uint8_t> other) const {
    simd8<bool> r;
    for (int i = 0; i < 16; i++)
      r.data[i] = (data[i] >= other.data[i]) ? 0xFF : 0x00;
    return r;
  }
  sonic_force_inline simd8<bool> operator>(const simd8<uint8_t> other) const {
    simd8<bool> r;
    for (int i = 0; i < 16; i++)
      r.data[i] = (data[i] > other.data[i]) ? 0xFF : 0x00;
    return r;
  }
  sonic_force_inline simd8<bool> operator<(const simd8<uint8_t> other) const {
    simd8<bool> r;
    for (int i = 0; i < 16; i++)
      r.data[i] = (data[i] < other.data[i]) ? 0xFF : 0x00;
    return r;
  }

  // gt_bits / lt_bits (non-zero instead of all-1s)
  sonic_force_inline simd8<uint8_t> gt_bits(const simd8<uint8_t> other) const {
    return this->saturating_sub(other);
  }
  sonic_force_inline simd8<uint8_t> lt_bits(const simd8<uint8_t> other) const {
    return other.saturating_sub(*this);
  }

  // Max / min
  sonic_force_inline uint8_t max_val() const {
    uint8_t m = 0;
    for (int i = 0; i < 16; i++)
      if (data[i] > m) m = data[i];
    return m;
  }
  sonic_force_inline uint8_t min_val() const {
    uint8_t m = 255;
    for (int i = 0; i < 16; i++)
      if (data[i] < m) m = data[i];
    return m;
  }
  sonic_force_inline simd8<uint8_t> max_val(const simd8<uint8_t> other) const {
    simd8<uint8_t> r;
    for (int i = 0; i < 16; i++)
      r.data[i] = data[i] > other.data[i] ? data[i] : other.data[i];
    return r;
  }
  sonic_force_inline simd8<uint8_t> min_val(const simd8<uint8_t> other) const {
    simd8<uint8_t> r;
    for (int i = 0; i < 16; i++)
      r.data[i] = data[i] < other.data[i] ? data[i] : other.data[i];
    return r;
  }

  // Bit-specific operations
  sonic_force_inline simd8<bool> any_bits_set(simd8<uint8_t> bits) const {
    simd8<bool> r;
    for (int i = 0; i < 16; i++)
      r.data[i] = (data[i] & bits.data[i]) ? 0xFF : 0x00;
    return r;
  }
  sonic_force_inline bool any_bits_set_anywhere() const {
    return max_val() != 0;
  }
  sonic_force_inline bool any_bits_set_anywhere(simd8<uint8_t> bits) const {
    return (*this & bits).any_bits_set_anywhere();
  }

  template <int N>
  sonic_force_inline simd8<uint8_t> shr() const {
    simd8<uint8_t> r;
    for (int i = 0; i < 16; i++) r.data[i] = data[i] >> N;
    return r;
  }
  template <int N>
  sonic_force_inline simd8<uint8_t> shl() const {
    simd8<uint8_t> r;
    for (int i = 0; i < 16; i++) r.data[i] = data[i] << N;
    return r;
  }

  // prev<N>: shift in bytes from prev_chunk
  template <int N = 1>
  sonic_force_inline simd8<uint8_t> prev(
      const simd8<uint8_t> prev_chunk) const {
    simd8<uint8_t> r;
    for (int i = 0; i < N; i++) r.data[i] = prev_chunk.data[16 - N + i];
    for (int i = N; i < 16; i++) r.data[i] = data[i - N];
    return r;
  }

  // lookup_16: table lookup, byte by byte
  template <typename L>
  sonic_force_inline simd8<L> lookup_16(simd8<L> lookup_table) const {
    simd8<L> r;
    for (int i = 0; i < 16; i++) {
      uint8_t idx = data[i] & 0x0F;
      r.data[i] = lookup_table.data[idx];
    }
    return r;
  }

  template <typename L>
  sonic_force_inline simd8<L> lookup_16(L r0, L r1, L r2, L r3, L r4, L r5,
                                        L r6, L r7, L r8, L r9, L r10, L r11,
                                        L r12, L r13, L r14, L r15) const {
    return lookup_16(simd8<L>::repeat_16(r0, r1, r2, r3, r4, r5, r6, r7, r8, r9,
                                         r10, r11, r12, r13, r14, r15));
  }

  template <typename T>
  sonic_force_inline simd8<uint8_t> apply_lookup_16_to(
      const simd8<T> original) {
    simd8<uint8_t> r;
    for (int i = 0; i < 16; i++) {
      uint8_t idx = (uint8_t)original.data[i];
      r.data[i] = idx < 16 ? data[idx] : 0;
    }
    return r;
  }
};

// Signed bytes
template <>
struct simd8<int8_t> {
  int8_t data[16];

  static sonic_force_inline simd8<int8_t> splat(int8_t _value) {
    simd8<int8_t> r;
    std::memset(r.data, (uint8_t)_value, 16);
    return r;
  }
  static sonic_force_inline simd8<int8_t> zero() {
    simd8<int8_t> r;
    std::memset(r.data, 0, 16);
    return r;
  }
  static sonic_force_inline simd8<int8_t> load(const int8_t values[16]) {
    simd8<int8_t> r;
    std::memcpy(r.data, values, 16);
    return r;
  }

  sonic_force_inline simd8() { std::memset(data, 0, 16); }
  sonic_force_inline simd8(int8_t _value) {
    std::memset(data, (uint8_t)_value, 16);
  }
  sonic_force_inline simd8(const int8_t* values) {
    std::memcpy(data, values, 16);
  }
  sonic_force_inline simd8(int8_t v0, int8_t v1, int8_t v2, int8_t v3,
                           int8_t v4, int8_t v5, int8_t v6, int8_t v7,
                           int8_t v8, int8_t v9, int8_t v10, int8_t v11,
                           int8_t v12, int8_t v13, int8_t v14, int8_t v15) {
    data[0] = v0;
    data[1] = v1;
    data[2] = v2;
    data[3] = v3;
    data[4] = v4;
    data[5] = v5;
    data[6] = v6;
    data[7] = v7;
    data[8] = v8;
    data[9] = v9;
    data[10] = v10;
    data[11] = v11;
    data[12] = v12;
    data[13] = v13;
    data[14] = v14;
    data[15] = v15;
  }

  static sonic_force_inline simd8<int8_t> repeat_16(
      int8_t v0, int8_t v1, int8_t v2, int8_t v3, int8_t v4, int8_t v5,
      int8_t v6, int8_t v7, int8_t v8, int8_t v9, int8_t v10, int8_t v11,
      int8_t v12, int8_t v13, int8_t v14, int8_t v15) {
    return simd8<int8_t>(v0, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12,
                         v13, v14, v15);
  }

  sonic_force_inline void store(int8_t dst[16]) const {
    std::memcpy(dst, data, 16);
  }

  // Explicit conversion to uint8_t
  sonic_force_inline explicit simd8(const simd8<uint8_t>& other) {
    std::memcpy(data, other.data, 16);
  }
  sonic_force_inline explicit operator simd8<uint8_t>() const {
    simd8<uint8_t> r;
    std::memcpy(r.data, data, 16);
    return r;
  }

  // Bitwise
  sonic_force_inline simd8<int8_t> operator|(const simd8<int8_t> other) const {
    simd8<int8_t> r;
    for (int i = 0; i < 16; i++) r.data[i] = data[i] | other.data[i];
    return r;
  }

  // Arithmetic
  sonic_force_inline simd8<int8_t> operator+(const simd8<int8_t> other) const {
    simd8<int8_t> r;
    for (int i = 0; i < 16; i++) r.data[i] = data[i] + other.data[i];
    return r;
  }
  sonic_force_inline simd8<int8_t> operator-(const simd8<int8_t> other) const {
    simd8<int8_t> r;
    for (int i = 0; i < 16; i++) r.data[i] = data[i] - other.data[i];
    return r;
  }
  sonic_force_inline simd8<int8_t>& operator+=(const simd8<int8_t> other) {
    for (int i = 0; i < 16; i++) data[i] += other.data[i];
    return *this;
  }
  sonic_force_inline simd8<int8_t>& operator-=(const simd8<int8_t> other) {
    for (int i = 0; i < 16; i++) data[i] -= other.data[i];
    return *this;
  }

  // Comparison
  sonic_force_inline simd8<int8_t> max_val(const simd8<int8_t> other) const {
    simd8<int8_t> r;
    for (int i = 0; i < 16; i++)
      r.data[i] = data[i] > other.data[i] ? data[i] : other.data[i];
    return r;
  }
  sonic_force_inline simd8<int8_t> min_val(const simd8<int8_t> other) const {
    simd8<int8_t> r;
    for (int i = 0; i < 16; i++)
      r.data[i] = data[i] < other.data[i] ? data[i] : other.data[i];
    return r;
  }
  sonic_force_inline simd8<bool> operator>(const simd8<int8_t> other) const {
    simd8<bool> r;
    for (int i = 0; i < 16; i++)
      r.data[i] = (data[i] > other.data[i]) ? 0xFF : 0x00;
    return r;
  }
  sonic_force_inline simd8<bool> operator<(const simd8<int8_t> other) const {
    simd8<bool> r;
    for (int i = 0; i < 16; i++)
      r.data[i] = (data[i] < other.data[i]) ? 0xFF : 0x00;
    return r;
  }
  friend sonic_force_inline simd8<bool> operator==(const simd8<int8_t> lhs,
                                                   const simd8<int8_t> rhs) {
    simd8<bool> r;
    for (int i = 0; i < 16; i++)
      r.data[i] = (lhs.data[i] == rhs.data[i]) ? 0xFF : 0x00;
    return r;
  }

  template <int N = 1>
  sonic_force_inline simd8<int8_t> prev(const simd8<int8_t> prev_chunk) const {
    simd8<int8_t> r;
    for (int i = 0; i < N; i++) r.data[i] = prev_chunk.data[16 - N + i];
    for (int i = N; i < 16; i++) r.data[i] = data[i - N];
    return r;
  }

  template <typename L>
  sonic_force_inline simd8<L> lookup_16(simd8<L> lookup_table) const {
    return lookup_table.apply_lookup_16_to(*this);
  }
  template <typename L>
  sonic_force_inline simd8<L> lookup_16(L r0, L r1, L r2, L r3, L r4, L r5,
                                        L r6, L r7, L r8, L r9, L r10, L r11,
                                        L r12, L r13, L r14, L r15) const {
    return lookup_16(simd8<L>::repeat_16(r0, r1, r2, r3, r4, r5, r6, r7, r8, r9,
                                         r10, r11, r12, r13, r14, r15));
  }

  template <typename T>
  sonic_force_inline simd8<int8_t> apply_lookup_16_to(const simd8<T> original) {
    simd8<int8_t> r;
    for (int i = 0; i < 16; i++) {
      uint8_t idx = (uint8_t)original.data[i];
      r.data[i] = idx < 16 ? data[idx] : 0;
    }
    return r;
  }
};

template <typename T>
struct simd8x64 {
  static constexpr int NUM_CHUNKS = 64 / sizeof(simd8<T>);
  static_assert(NUM_CHUNKS == 4,
                "RISC-V kernel should use four registers per 64-byte block.");
  const simd8<T> chunks[NUM_CHUNKS];

  simd8x64(const simd8x64<T>& o) = delete;  // no copy allowed
  simd8x64<T>& operator=(const simd8<T>& other) =
      delete;           // no assignment allowed
  simd8x64() = delete;  // no default constructor allowed

  sonic_force_inline simd8x64(const simd8<T> chunk0, const simd8<T> chunk1,
                              const simd8<T> chunk2, const simd8<T> chunk3)
      : chunks{chunk0, chunk1, chunk2, chunk3} {}
  sonic_force_inline simd8x64(const T ptr[64])
      : chunks{simd8<T>::load((const uint8_t*)ptr),
               simd8<T>::load((const uint8_t*)(ptr + 16)),
               simd8<T>::load((const uint8_t*)(ptr + 32)),
               simd8<T>::load((const uint8_t*)(ptr + 48))} {}

  sonic_force_inline void store(T ptr[64]) const {
    this->chunks[0].store((uint8_t*)(ptr));
    this->chunks[1].store((uint8_t*)(ptr + 16));
    this->chunks[2].store((uint8_t*)(ptr + 32));
    this->chunks[3].store((uint8_t*)(ptr + 48));
  }

  sonic_force_inline simd8<T> reduce_or() const {
    return (this->chunks[0] | this->chunks[1]) |
           (this->chunks[2] | this->chunks[3]);
  }

  sonic_force_inline uint64_t to_bitmask() const {
    // Each chunk's to_bitmask() returns 64 bits with 4 bits per byte.
    // Compress to 1 bit per byte (16 bits per chunk) before combining.
    auto compress = [](uint64_t mask) -> uint64_t {
      uint64_t result = 0;
      for (int i = 0; i < 16; i++) {
        if (mask & (0xFULL << (i * 4))) {
          result |= (1ULL << i);
        }
      }
      return result;
    };
    uint64_t r0 = compress(this->chunks[0].to_bitmask());
    uint64_t r1 = compress(this->chunks[1].to_bitmask());
    uint64_t r2 = compress(this->chunks[2].to_bitmask());
    uint64_t r3 = compress(this->chunks[3].to_bitmask());
    return r0 | (r1 << 16) | (r2 << 32) | (r3 << 48);
  }

  sonic_force_inline uint64_t eq(const T m) const {
#if SONIC_RISCV_SIMD_HAS_ZBB
    const uint8_t byte = static_cast<uint8_t>(m);
    uint64_t r0 = riscv_zbb_detail::eq_mask16(
        reinterpret_cast<const uint8_t*>(this->chunks[0].data), byte);
    uint64_t r1 = riscv_zbb_detail::eq_mask16(
        reinterpret_cast<const uint8_t*>(this->chunks[1].data), byte);
    uint64_t r2 = riscv_zbb_detail::eq_mask16(
        reinterpret_cast<const uint8_t*>(this->chunks[2].data), byte);
    uint64_t r3 = riscv_zbb_detail::eq_mask16(
        reinterpret_cast<const uint8_t*>(this->chunks[3].data), byte);
    return r0 | (r1 << 16) | (r2 << 32) | (r3 << 48);
#else
    const simd8<T> mask = simd8<T>::splat(m);
    return simd8x64<bool>(this->chunks[0] == mask, this->chunks[1] == mask,
                          this->chunks[2] == mask, this->chunks[3] == mask)
        .to_bitmask();
#endif
  }

  sonic_force_inline uint64_t lteq(const T m) const {
#if SONIC_RISCV_SIMD_HAS_ZBB
    const uint8_t byte = static_cast<uint8_t>(m);
    uint64_t r0 = riscv_zbb_detail::lteq_mask16(
        reinterpret_cast<const uint8_t*>(this->chunks[0].data), byte);
    uint64_t r1 = riscv_zbb_detail::lteq_mask16(
        reinterpret_cast<const uint8_t*>(this->chunks[1].data), byte);
    uint64_t r2 = riscv_zbb_detail::lteq_mask16(
        reinterpret_cast<const uint8_t*>(this->chunks[2].data), byte);
    uint64_t r3 = riscv_zbb_detail::lteq_mask16(
        reinterpret_cast<const uint8_t*>(this->chunks[3].data), byte);
    return r0 | (r1 << 16) | (r2 << 32) | (r3 << 48);
#else
    const simd8<T> mask = simd8<T>::splat(m);
    return simd8x64<bool>(this->chunks[0] <= mask, this->chunks[1] <= mask,
                          this->chunks[2] <= mask, this->chunks[3] <= mask)
        .to_bitmask();
#endif
  }
};  // struct simd8x64<T>

}  // namespace riscv
}  // namespace internal
}  // namespace sonic_json

#undef SONIC_RISCV_SIMD_HAS_ZBB
