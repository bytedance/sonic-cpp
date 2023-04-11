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

#include <immintrin.h>
#include <sonic/macro.h>

#include <cstdint>

SONIC_PUSH_WESTMERE

namespace sonic_json {
namespace internal {
namespace simd {

#define REPEAT16_ARGS(typ)                                                \
  typ v0, typ v1, typ v2, typ v3, typ v4, typ v5, typ v6, typ v7, typ v8, \
      typ v9, typ v10, typ v11, typ v12, typ v13, typ v14, typ v15

template <typename T>
struct simd128;

template <typename T, typename Mask = simd128<bool>>
struct base128 {
 public:
  using Child = simd128<T>;
  __m128i value;
  sonic_force_inline base128() : value{__m128i()} {}
  sonic_force_inline base128(const __m128i _value) : value(_value) {}
  sonic_force_inline base128(const T _value) : value(splat(_value)) {}
  sonic_force_inline base128(const T values[16]) : value(load(values)) {}
  sonic_force_inline base128(REPEAT16_ARGS(T))
      : value(_mm_setr_epi8(REPEAT16_ARGS())) {}

  // Conversion to SIMD register
  sonic_force_inline operator const __m128i&() const { return this->value; }
  sonic_force_inline operator __m128i&() { return this->value; }

  // Bit operations
  sonic_force_inline Child operator|(const Child other) const {
    return _mm_or_si128(*this, other);
  }
  sonic_force_inline Child operator&(const Child other) const {
    return _mm_and_si128(*this, other);
  }
  sonic_force_inline Child operator^(const Child other) const {
    return _mm_xor_si128(*this, other);
  }
  sonic_force_inline Child bit_andnot(const Child other) const {
    return _mm_andnot_si128(other, *this);
  }
  sonic_force_inline Child operator~() const {
    return *this ^ _mm_set1_epi8(uint8_t(0xFFu));
  }
  sonic_force_inline Child& operator|=(const Child other) {
    auto this_cast = static_cast<Child*>(this);
    *this_cast = *this_cast | other;
    return *this_cast;
  }
  sonic_force_inline Child& operator&=(const Child other) {
    auto this_cast = static_cast<Child*>(this);
    *this_cast = *this_cast & other;
    return *this_cast;
  }
  sonic_force_inline Child& operator^=(const Child other) {
    auto this_cast = static_cast<Child*>(this);
    *this_cast = *this_cast ^ other;
    return *this_cast;
  }

  // Compare operations
  friend sonic_force_inline Mask operator==(const Child lhs, const Child rhs) {
    return _mm_cmpeq_epi8(lhs, rhs);
  }

  // Memory Operations
  sonic_force_inline void store(T dst[16]) const {
    return _mm_storeu_si128(reinterpret_cast<__m128i*>(dst), *this);
  }
  static sonic_force_inline Child load(const T values[16]) {
    return _mm_loadu_si128(reinterpret_cast<const __m128i*>(values));
  }
  static sonic_force_inline Child splat(T _value) {
    return Child(_mm_set1_epi8(_value));
  }
  static sonic_force_inline Child repeat_16(REPEAT16_ARGS(T)) {
    return Child(REPEAT16_ARGS());
  }
  template <int N = 1>
  sonic_force_inline Child prev(const Child prev_chunk) const {
    return _mm_alignr_epi8(*this, prev_chunk, 16 - N);
  }
};

// SIMD byte mask type (returned by things like eq and gt)
template <>
struct simd128<bool> : base128<bool> {
  sonic_force_inline simd128<bool>() : base128<bool>() {}
  sonic_force_inline simd128<bool>(const __m128i _value)
      : base128<bool>(_value) {}
  // Splat constructor
  sonic_force_inline simd128<bool>(bool _value)
      : base128<bool>(splat(_value)) {}
  sonic_force_inline int to_bitmask() const { return _mm_movemask_epi8(*this); }
  // Override splat bool
  static sonic_force_inline simd128<bool> splat(bool _value) {
    return _mm_set1_epi8(uint8_t(-(!!_value)));
  }
};

template <typename T>
struct num128 : base128<T> {
  using Base = base128<T>;
  using Base::Base;
  static sonic_force_inline simd128<T> zero() { return _mm_setzero_si128(); }

  // Addition/subtraction are the same for signed and unsigned
  sonic_force_inline simd128<T> operator+(const simd128<T> other) const {
    return _mm_add_epi8(*this, other);
  }
  sonic_force_inline simd128<T> operator-(const simd128<T> other) const {
    return _mm_sub_epi8(*this, other);
  }
  sonic_force_inline simd128<T>& operator+=(const simd128<T> other) {
    *this = *this + other;
    return *static_cast<simd128<T>*>(this);
  }
  sonic_force_inline simd128<T>& operator-=(const simd128<T> other) {
    *this = *this - other;
    return *static_cast<simd128<T>*>(this);
  }
};

// Signed bytes
template <>
struct simd128<int8_t> : num128<int8_t> {
  using Base = num128<int8_t>;
  using Base::Base;
  // Order-sensitive comparisons
  sonic_force_inline simd128<int8_t> max_val(
      const simd128<int8_t> other) const {
    return _mm_max_epi8(*this, other);
  }
  sonic_force_inline simd128<int8_t> min_val(
      const simd128<int8_t> other) const {
    return _mm_min_epi8(*this, other);
  }
  sonic_force_inline simd128<bool> operator>(
      const simd128<int8_t> other) const {
    return _mm_cmpgt_epi8(*this, other);
  }
  sonic_force_inline simd128<bool> operator<(
      const simd128<int8_t> other) const {
    return _mm_cmpgt_epi8(other, *this);
  }
};

// Unsigned bytes
template <>
struct simd128<uint8_t> : num128<uint8_t> {
  using Base = num128<uint8_t>;
  using Base::Base;

  // Saturated math
  sonic_force_inline simd128<uint8_t> saturating_add(
      const simd128<uint8_t> other) const {
    return _mm_adds_epu8(*this, other);
  }
  sonic_force_inline simd128<uint8_t> saturating_sub(
      const simd128<uint8_t> other) const {
    return _mm_subs_epu8(*this, other);
  }

  // Order-specific operations
  sonic_force_inline simd128<uint8_t> max_val(
      const simd128<uint8_t> other) const {
    return _mm_max_epu8(*this, other);
  }
  sonic_force_inline simd128<uint8_t> min_val(
      const simd128<uint8_t> other) const {
    return _mm_min_epu8(other, *this);
  }
  // Same as >, but only guarantees true is nonzero (< guarantees true = -1)
  sonic_force_inline simd128<uint8_t> gt_bits(
      const simd128<uint8_t> other) const {
    return this->saturating_sub(other);
  }
  // Same as <, but only guarantees true is nonzero (< guarantees true = -1)
  sonic_force_inline simd128<uint8_t> lt_bits(
      const simd128<uint8_t> other) const {
    return other.saturating_sub(*this);
  }
  sonic_force_inline simd128<bool> operator<=(
      const simd128<uint8_t> other) const {
    return other.max_val(*this) == other;
  }
  sonic_force_inline simd128<bool> operator>=(
      const simd128<uint8_t> other) const {
    return other.min_val(*this) == other;
  }
  sonic_force_inline simd128<bool> operator>(
      const simd128<uint8_t> other) const {
    return this->gt_bits(other).any_bits_set();
  }
  sonic_force_inline simd128<bool> operator<(
      const simd128<uint8_t> other) const {
    return this->lt_bits(other).any_bits_set();
  }

  // Bit-specific operations
  sonic_force_inline simd128<bool> bits_not_set() const {
    return *this == uint8_t(0);
  }
  sonic_force_inline simd128<bool> bits_not_set(simd128<uint8_t> bits) const {
    return (*this & bits).bits_not_set();
  }
  sonic_force_inline simd128<bool> any_bits_set() const {
    return ~this->bits_not_set();
  }
  sonic_force_inline simd128<bool> any_bits_set(simd128<uint8_t> bits) const {
    return ~this->bits_not_set(bits);
  }
  sonic_force_inline bool is_ascii() const {
    return _mm_movemask_epi8(*this) == 0;
  }
  sonic_force_inline bool bits_not_set_anywhere() const {
    return _mm_testz_si128(*this, *this);
  }
  sonic_force_inline bool any_bits_set_anywhere() const {
    return !bits_not_set_anywhere();
  }
  sonic_force_inline bool bits_not_set_anywhere(simd128<uint8_t> bits) const {
    return _mm_testz_si128(*this, bits);
  }
  sonic_force_inline bool any_bits_set_anywhere(simd128<uint8_t> bits) const {
    return !bits_not_set_anywhere(bits);
  }
  template <int N>
  sonic_force_inline simd128<uint8_t> shr() const {
    return simd128<uint8_t>(_mm_srli_epi16(*this, N)) & uint8_t(0xFFu >> N);
  }
  template <int N>
  sonic_force_inline simd128<uint8_t> shl() const {
    return simd128<uint8_t>(_mm_slli_epi16(*this, N)) & uint8_t(0xFFu << N);
  }
  // Get one of the bits and make a bitmask out of it.
  // e.g. value.get_bit<7>() gets the high bit
  template <int N>
  sonic_force_inline int get_bit() const {
    return _mm_movemask_epi8(_mm_slli_epi16(*this, 7 - N));
  }
};

template <typename T>
struct simd8x16 {
  static constexpr int NUM_CHUNKS = 16 / sizeof(simd128<T>);
  static_assert(
      NUM_CHUNKS == 1,
      "Haswell kernel should use one sse registers per 16-byte block.");
  const simd128<T> chunks[NUM_CHUNKS];

  simd8x16(const simd8x16<T>& o) = delete;  // no copy allowed
  simd8x16<T>& operator=(const simd128<T>& other) =
      delete;           // no assignment allowed
  simd8x16() = delete;  // no default constructor allowed

  sonic_force_inline simd8x16(const simd128<T> chunk0) : chunks{chunk0} {}
  sonic_force_inline simd8x16(const T ptr[16])
      : chunks{simd128<T>::load(ptr)} {}

  sonic_force_inline void store(T ptr[16]) const {
    this->chunks[0].store(ptr + sizeof(simd128<T>) * 0);
  }

  sonic_force_inline uint64_t to_bitmask() const {
    return this->chunks[0].to_bitmask();
  }

  sonic_force_inline simd128<T> reduce_or() const { return this->chunks[0]; }

  sonic_force_inline simd8x16<T> bit_or(const T m) const {
    const simd128<T> mask = simd128<T>::splat(m);
    return simd8x16<T>(this->chunks[0] | mask);
  }

  sonic_force_inline uint64_t eq(const T m) const {
    const simd128<T> mask = simd128<T>::splat(m);
    return simd8x16<bool>(this->chunks[0] == mask).to_bitmask();
  }

  sonic_force_inline uint64_t eq(const simd8x16<uint8_t>& other) const {
    return simd8x16<bool>(this->chunks[0] == other.chunks[0]).to_bitmask();
  }

  sonic_force_inline uint64_t lteq(const T m) const {
    const simd128<T> mask = simd128<T>::splat(m);
    return simd8x16<bool>(this->chunks[0] <= mask).to_bitmask();
  }
};  // struct simd8x16<T>

template <typename T>
struct simd8x64 {
  static constexpr int NUM_CHUNKS = 64 / sizeof(simd128<T>);
  static_assert(NUM_CHUNKS == 4,
                "Westmere kernel should use four registers per 64-byte block.");
  const simd128<T> chunks[NUM_CHUNKS];

  simd8x64(const simd8x64<T>& o) = delete;  // no copy allowed
  simd8x64<T>& operator=(const simd128<T>& other) =
      delete;           // no assignment allowed
  simd8x64() = delete;  // no default constructor allowed

  sonic_force_inline simd8x64(const simd128<T> chunk0, const simd128<T> chunk1,
                              const simd128<T> chunk2, const simd128<T> chunk3)
      : chunks{chunk0, chunk1, chunk2, chunk3} {}
  sonic_force_inline simd8x64(const T ptr[64])
      : chunks{simd128<T>::load(ptr), simd128<T>::load(ptr + 16),
               simd128<T>::load(ptr + 32), simd128<T>::load(ptr + 48)} {}

  sonic_force_inline void store(T ptr[64]) const {
    this->chunks[0].store(ptr + sizeof(simd128<T>) * 0);
    this->chunks[1].store(ptr + sizeof(simd128<T>) * 1);
    this->chunks[2].store(ptr + sizeof(simd128<T>) * 2);
    this->chunks[3].store(ptr + sizeof(simd128<T>) * 3);
  }

  sonic_force_inline simd128<T> reduce_or() const {
    return (this->chunks[0] | this->chunks[1]) |
           (this->chunks[2] | this->chunks[3]);
  }

  sonic_force_inline uint64_t to_bitmask() const {
    uint64_t r0 = uint32_t(this->chunks[0].to_bitmask());
    uint64_t r1 = this->chunks[1].to_bitmask();
    uint64_t r2 = this->chunks[2].to_bitmask();
    uint64_t r3 = this->chunks[3].to_bitmask();
    return r0 | (r1 << 16) | (r2 << 32) | (r3 << 48);
  }

  sonic_force_inline uint64_t eq(const T m) const {
    const simd128<T> mask = simd128<T>::splat(m);
    return simd8x64<bool>(this->chunks[0] == mask, this->chunks[1] == mask,
                          this->chunks[2] == mask, this->chunks[3] == mask)
        .to_bitmask();
  }

  sonic_force_inline uint64_t eq(const simd8x64<uint8_t>& other) const {
    return simd8x64<bool>(this->chunks[0] == other.chunks[0],
                          this->chunks[1] == other.chunks[1],
                          this->chunks[2] == other.chunks[2],
                          this->chunks[3] == other.chunks[3])
        .to_bitmask();
  }

  sonic_force_inline uint64_t lteq(const T m) const {
    const simd128<T> mask = simd128<T>::splat(m);
    return simd8x64<bool>(this->chunks[0] <= mask, this->chunks[1] <= mask,
                          this->chunks[2] <= mask, this->chunks[3] <= mask)
        .to_bitmask();
  }
};  // struct simd8x64<T>

#undef REPEAT16_ARGS
}  // namespace simd
}  // namespace internal
}  // namespace sonic_json

SONIC_POP_TARGET
