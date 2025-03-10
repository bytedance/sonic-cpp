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

#include <riscv_vector.h>
#include <sonic/macro.h>

namespace sonic_json {
namespace internal {
namespace rvv_128 {
// for rvv-128
sonic_force_inline uint16_t to_bitmask(vbool8_t v) {
  return __riscv_vmv_x_s_u16m1_u16(__riscv_vreinterpret_v_b8_u16m1(v));
}

typedef vuint8m1_t vuint8x16_t
    __attribute__((riscv_rvv_vector_bits(__riscv_v_fixed_vlen)));
typedef vint8m1_t vint8x16_t
    __attribute__((riscv_rvv_vector_bits(__riscv_v_fixed_vlen)));

template <typename T>
struct simd8;

//
// Base class of simd8<uint8_t> and simd8<bool>, both of which use vuint8x16_t
// internally.
//
template <typename T, typename Mask = uint16_t>
struct base_u8 {
  vuint8x16_t value;
  static const int SIZE = sizeof(value);

  // Conversion from/to SIMD register
  sonic_force_inline base_u8(const vuint8x16_t _value) : value(_value) {}
  sonic_force_inline operator const vuint8x16_t&() const { return this->value; }
  sonic_force_inline operator vuint8x16_t&() { return this->value; }

  // Bit operations
  sonic_force_inline simd8<T> operator|(const simd8<T> other) const {
    return __riscv_vor_vv_u8m1(*this, other, 16);
  }
  sonic_force_inline simd8<T> operator&(const simd8<T> other) const {
    return __riscv_vand_vv_u8m1(*this, other, 16);
  }
  sonic_force_inline simd8<T> operator^(const simd8<T> other) const {
    return __riscv_vxor_vv_u8m1(*this, other, 16);
  }
  sonic_force_inline simd8<T> bit_andnot(const simd8<T> other) const {
    return __riscv_vand_vv_u8m1(*this, __riscv_vnot_v_u8m1(other, 16), 16);
  }
  sonic_force_inline simd8<T> operator~() const { return *this ^ 0xFFu; }
  sonic_force_inline simd8<T>& operator|=(const simd8<T> other) {
    auto this_cast = static_cast<simd8<T>*>(this);
    *this_cast = *this_cast | other;
    return *this_cast;
  }
  sonic_force_inline simd8<T>& operator&=(const simd8<T> other) {
    auto this_cast = static_cast<simd8<T>*>(this);
    *this_cast = *this_cast & other;
    return *this_cast;
  }
  sonic_force_inline simd8<T>& operator^=(const simd8<T> other) {
    auto this_cast = static_cast<simd8<T>*>(this);
    *this_cast = *this_cast ^ other;
    return *this_cast;
  }

  friend sonic_force_inline Mask operator==(const simd8<T> lhs,
                                            const simd8<T> rhs) {
    return to_bitmask(__riscv_vmseq_vv_u8m1_b8(lhs, rhs, 16));
  }

  template <int N = 1>
  sonic_force_inline simd8<T> prev(const simd8<T> prev_chunk) const {
    vuint8x16_t prev_chunk_slidedown =
        __riscv_vslidedown_vx_u8m1(prev_chunk, 16 - N, 16);
    return __riscv_vslideup_vx_u8m1(prev_chunk_slidedown, *this, N, 16);
  }
};

// Unsigned bytes
template <>
struct simd8<uint8_t> : base_u8<uint8_t> {
  static sonic_force_inline vuint8x16_t splat(uint8_t _value) {
    return __riscv_vmv_v_x_u8m1(_value, 16);
  }
  static sonic_force_inline vuint8x16_t zero() {
    return __riscv_vmv_v_x_u8m1(0, 16);
  }
  static sonic_force_inline vuint8x16_t load(const uint8_t* values) {
    return __riscv_vle8_v_u8m1(values, 16);
  }

  sonic_force_inline simd8(const vuint8x16_t _value)
      : base_u8<uint8_t>(_value) {}
  // Zero constructor
  sonic_force_inline simd8() : simd8(zero()) {}
  // Array constructor
  sonic_force_inline simd8(const uint8_t values[16]) : simd8(load(values)) {}
  // Splat constructor
  sonic_force_inline simd8(uint8_t _value) : simd8(splat(_value)) {}
  // Member-by-member initialization
  sonic_force_inline simd8(uint8_t v0, uint8_t v1, uint8_t v2, uint8_t v3,
                           uint8_t v4, uint8_t v5, uint8_t v6, uint8_t v7,
                           uint8_t v8, uint8_t v9, uint8_t v10, uint8_t v11,
                           uint8_t v12, uint8_t v13, uint8_t v14, uint8_t v15)
      : simd8(vuint8x16_t{v0, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12,
                          v13, v14, v15}) {}

  // Repeat 16 values as many times as necessary (usually for lookup tables)
  sonic_force_inline static simd8<uint8_t> repeat_16(
      uint8_t v0, uint8_t v1, uint8_t v2, uint8_t v3, uint8_t v4, uint8_t v5,
      uint8_t v6, uint8_t v7, uint8_t v8, uint8_t v9, uint8_t v10, uint8_t v11,
      uint8_t v12, uint8_t v13, uint8_t v14, uint8_t v15) {
    return simd8<uint8_t>(v0, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12,
                          v13, v14, v15);
  }

  // Store to array
  sonic_force_inline void store(uint8_t dst[16]) const {
    return __riscv_vse8_v_u8m1(dst, *this, 16);
  }

  // Saturated math
  sonic_force_inline simd8<uint8_t> saturating_add(
      const simd8<uint8_t> other) const {
    return __riscv_vsaddu_vv_u8m1(*this, other, 16);
  }
  sonic_force_inline simd8<uint8_t> saturating_sub(
      const simd8<uint8_t> other) const {
    return __riscv_vssubu_vv_u8m1(*this, other, 16);
  }

  // Addition/subtraction are the same for signed and unsigned
  sonic_force_inline simd8<uint8_t> operator+(
      const simd8<uint8_t> other) const {
    return __riscv_vadd_vv_u8m1(*this, other, 16);
  }
  sonic_force_inline simd8<uint8_t> operator-(
      const simd8<uint8_t> other) const {
    return __riscv_vsub_vv_u8m1(*this, other, 16);
  }
  sonic_force_inline simd8<uint8_t>& operator+=(const simd8<uint8_t> other) {
    *this = *this + other;
    return *this;
  }
  sonic_force_inline simd8<uint8_t>& operator-=(const simd8<uint8_t> other) {
    *this = *this - other;
    return *this;
  }

  // Order-specific operations
  sonic_force_inline uint8_t max_val() const {
    return __riscv_vmv_x_s_u8m1_u8(
        __riscv_vredmaxu_vs_u8m1_u8m1(*this, __riscv_vmv_v_x_u8m1(0, 16), 16));
  }
  sonic_force_inline uint8_t min_val() const {
    return __riscv_vmv_x_s_u8m1_u8(__riscv_vredminu_vs_u8m1_u8m1(
        *this, __riscv_vmv_v_x_u8m1(UINT8_MAX, 16), 16));
  }
  sonic_force_inline simd8<uint8_t> max_val(const simd8<uint8_t> other) const {
    return __riscv_vmaxu_vv_u8m1(*this, other, 16);
  }
  sonic_force_inline simd8<uint8_t> min_val(const simd8<uint8_t> other) const {
    return __riscv_vminu_vv_u8m1(*this, other, 16);
  }
  sonic_force_inline uint16_t operator<=(const simd8<uint8_t> other) const {
    return to_bitmask(__riscv_vmsleu_vv_u8m1_b8(*this, other, 16));
  }
  sonic_force_inline uint16_t operator>=(const simd8<uint8_t> other) const {
    return to_bitmask(__riscv_vmsgeu_vv_u8m1_b8(*this, other, 16));
  }
  sonic_force_inline uint16_t operator<(const simd8<uint8_t> other) const {
    return to_bitmask(__riscv_vmsltu_vv_u8m1_b8(*this, other, 16));
  }
  sonic_force_inline uint16_t operator>(const simd8<uint8_t> other) const {
    return to_bitmask(__riscv_vmsgtu_vv_u8m1_b8(*this, other, 16));
  }
  // Same as >, but instead of guaranteeing all 1's == true, false = 0 and true
  // = nonzero. For ARM, returns all 1's.
  sonic_force_inline simd8<uint8_t> gt_bits(const simd8<uint8_t> other) const {
    return simd8<uint8_t>(*this > other);
  }
  // Same as <, but instead of guaranteeing all 1's == true, false = 0 and true
  // = nonzero. For ARM, returns all 1's.
  sonic_force_inline simd8<uint8_t> lt_bits(const simd8<uint8_t> other) const {
    return simd8<uint8_t>(*this < other);
  }

  // Bit-specific operations
  sonic_force_inline uint16_t any_bits_set(simd8<uint8_t> bits) const {
    return to_bitmask(__riscv_vmsgtu_vx_u8m1_b8(
        __riscv_vand_vv_u8m1(*this, bits, 16), 0, 16));
  }
  sonic_force_inline bool any_bits_set_anywhere() const {
    return this->max_val() != 0;
  }
  sonic_force_inline bool any_bits_set_anywhere(simd8<uint8_t> bits) const {
    return (*this & bits).any_bits_set_anywhere();
  }
  template <int N>
  sonic_force_inline simd8<uint8_t> shr() const {
    const int b_half = N >> 1;
    vuint8m1_t srl1 = __riscv_vsrl_vx_u8m1(*this, b_half, 16);
    return __riscv_vsrl_vx_u8m1(srl1, b_half + (N & 0x1), 16);

    // return vshrq_n_u8(*this, N);
  }
  template <int N>
  sonic_force_inline simd8<uint8_t> shl() const {
    return __riscv_vsll_vx_u8m1(*this, N, 16);
  }

  // Perform a lookup assuming the value is between 0 and 16 (undefined behavior
  // for out of range values)
  template <typename L>
  sonic_force_inline simd8<L> lookup_16(simd8<L> lookup_table) const {
    return lookup_table.apply_lookup_16_to(*this);
  }

  template <typename L>
  sonic_force_inline simd8<L> lookup_16(L replace0, L replace1, L replace2,
                                        L replace3, L replace4, L replace5,
                                        L replace6, L replace7, L replace8,
                                        L replace9, L replace10, L replace11,
                                        L replace12, L replace13, L replace14,
                                        L replace15) const {
    return lookup_16(simd8<L>::repeat_16(
        replace0, replace1, replace2, replace3, replace4, replace5, replace6,
        replace7, replace8, replace9, replace10, replace11, replace12,
        replace13, replace14, replace15));
  }

  template <typename T>
  sonic_force_inline simd8<uint8_t> apply_lookup_16_to(
      const simd8<T> original) {
    vbool8_t mask = __riscv_vmsgeu_vx_u8m1_b8(simd8<uint8_t>(original), 16, 16);
    return __riscv_vmerge_vxm_u8m1(
        __riscv_vrgather_vv_u8m1(*this, simd8<uint8_t>(original), 16), 0, mask,
        16);
    // return vqtbl1q_u8(*this, simd8<uint8_t>(original));
  }
};

// Signed bytes
template <>
struct simd8<int8_t> {
  vint8x16_t value;

  static sonic_force_inline simd8<int8_t> splat(int8_t _value) {
    return __riscv_vmv_v_x_i8m1(_value, 16);
  }
  static sonic_force_inline simd8<int8_t> zero() {
    return __riscv_vmv_v_x_i8m1(0, 16);
  }
  static sonic_force_inline simd8<int8_t> load(const int8_t values[16]) {
    return __riscv_vle8_v_i8m1(values, 16);
  }

  // Conversion from/to SIMD register
  sonic_force_inline simd8(const vint8x16_t _value) : value{_value} {}
  sonic_force_inline operator const vint8x16_t&() const { return this->value; }
  sonic_force_inline operator vint8x16_t&() { return this->value; }

  // Zero constructor
  sonic_force_inline simd8() : simd8(zero()) {}
  // Splat constructor
  sonic_force_inline simd8(int8_t _value) : simd8(splat(_value)) {}
  // Array constructor
  sonic_force_inline simd8(const int8_t* values) : simd8(load(values)) {}
  // Member-by-member initialization
  sonic_force_inline simd8(int8_t v0, int8_t v1, int8_t v2, int8_t v3,
                           int8_t v4, int8_t v5, int8_t v6, int8_t v7,
                           int8_t v8, int8_t v9, int8_t v10, int8_t v11,
                           int8_t v12, int8_t v13, int8_t v14, int8_t v15)
      : simd8(vint8x16_t{v0, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12,
                         v13, v14, v15}) {}
  // Repeat 16 values as many times as necessary (usually for lookup tables)
  sonic_force_inline static simd8<int8_t> repeat_16(
      int8_t v0, int8_t v1, int8_t v2, int8_t v3, int8_t v4, int8_t v5,
      int8_t v6, int8_t v7, int8_t v8, int8_t v9, int8_t v10, int8_t v11,
      int8_t v12, int8_t v13, int8_t v14, int8_t v15) {
    return simd8<int8_t>(v0, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12,
                         v13, v14, v15);
  }

  // Store to array
  sonic_force_inline void store(int8_t dst[16]) const {
    return __riscv_vse8_v_i8m1(dst, *this, 16);
  }

  sonic_force_inline explicit operator simd8<uint8_t>() const {
    return __riscv_vreinterpret_v_i8m1_u8m1(this->value);
  }

  // Math
  sonic_force_inline simd8<int8_t> operator+(const simd8<int8_t> other) const {
    return __riscv_vadd_vv_i8m1(*this, other, 16);
  }
  sonic_force_inline simd8<int8_t> operator-(const simd8<int8_t> other) const {
    return __riscv_vsub_vv_i8m1(*this, other, 16);
  }
  sonic_force_inline simd8<int8_t>& operator+=(const simd8<int8_t> other) {
    *this = *this + other;
    return *this;
  }
  sonic_force_inline simd8<int8_t>& operator-=(const simd8<int8_t> other) {
    *this = *this - other;
    return *this;
  }

  // Order-sensitive comparisons
  sonic_force_inline simd8<int8_t> max_val(const simd8<int8_t> other) const {
    return __riscv_vmax_vv_i8m1(*this, other, 16);
  }
  sonic_force_inline simd8<int8_t> min_val(const simd8<int8_t> other) const {
    return __riscv_vmin_vv_i8m1(*this, other, 16);
  }
  sonic_force_inline uint16_t operator>(const simd8<int8_t> other) const {
    return to_bitmask(__riscv_vmsgt_vv_i8m1_b8(*this, other, 16));
  }
  sonic_force_inline uint16_t operator<(const simd8<int8_t> other) const {
    return to_bitmask(__riscv_vmslt_vv_i8m1_b8(*this, other, 16));
  }
  sonic_force_inline uint16_t operator==(const simd8<int8_t> other) const {
    return to_bitmask(__riscv_vmseq_vv_i8m1_b8(*this, other, 16));
  }

  template <int N = 1>
  sonic_force_inline simd8<int8_t> prev(const simd8<int8_t> prev_chunk) const {
    vint8m1_t prev_chunk_slidedown =
        __riscv_vslidedown_vx_i8m1(prev_chunk, 16 - N, 16);
    return __riscv_vslideup_vx_i8m1(prev_chunk_slidedown, *this, N, 16);
  }

  // Perform a lookup assuming no value is larger than 16
  template <typename L>
  sonic_force_inline simd8<L> lookup_16(simd8<L> lookup_table) const {
    return lookup_table.apply_lookup_16_to(*this);
  }
  template <typename L>
  sonic_force_inline simd8<L> lookup_16(L replace0, L replace1, L replace2,
                                        L replace3, L replace4, L replace5,
                                        L replace6, L replace7, L replace8,
                                        L replace9, L replace10, L replace11,
                                        L replace12, L replace13, L replace14,
                                        L replace15) const {
    return lookup_16(simd8<L>::repeat_16(
        replace0, replace1, replace2, replace3, replace4, replace5, replace6,
        replace7, replace8, replace9, replace10, replace11, replace12,
        replace13, replace14, replace15));
  }

  template <typename T>
  sonic_force_inline simd8<int8_t> apply_lookup_16_to(const simd8<T> original) {
    vbool8_t mask = __riscv_vmsgeu_vx_u8m1_b8(simd8<uint8_t>(original), 16, 16);
    return __riscv_vmerge_vxm_i8m1(
        __riscv_vrgather_vv_i8m1(*this, simd8<uint8_t>(original), 16), 0, mask,
        16);
  }
};

sonic_force_inline uint64_t merge_bitmask(uint16_t mask1, uint16_t mask2,
                                          uint16_t mask3, uint16_t mask4) {
  return (uint64_t)mask1 | ((uint64_t)mask2 << 16) | ((uint64_t)mask3 << 32) |
         ((uint64_t)mask4 << 48);
}

template <typename T>
struct simd8x64 {
  static constexpr int NUM_CHUNKS = 64 / sizeof(simd8<T>);
  static_assert(NUM_CHUNKS == 4,
                "ARM kernel should use four registers per 64-byte block.");
  const simd8<T> chunks[NUM_CHUNKS];

  simd8x64(const simd8x64<T>& o) = delete;  // no copy allowed
  simd8x64<T>& operator=(const simd8<T>& other) =
      delete;           // no assignment allowed
  simd8x64() = delete;  // no default constructor allowed

  sonic_force_inline simd8x64(const simd8<T> chunk0, const simd8<T> chunk1,
                              const simd8<T> chunk2, const simd8<T> chunk3)
      : chunks{chunk0, chunk1, chunk2, chunk3} {}
  sonic_force_inline simd8x64(const T ptr[64])
      : chunks{simd8<T>::load(ptr), simd8<T>::load(ptr + 16),
               simd8<T>::load(ptr + 32), simd8<T>::load(ptr + 48)} {}

  sonic_force_inline void store(T ptr[64]) const {
    this->chunks[0].store(ptr + sizeof(simd8<T>) * 0);
    this->chunks[1].store(ptr + sizeof(simd8<T>) * 1);
    this->chunks[2].store(ptr + sizeof(simd8<T>) * 2);
    this->chunks[3].store(ptr + sizeof(simd8<T>) * 3);
  }

  sonic_force_inline simd8<T> reduce_or() const {
    return (this->chunks[0] | this->chunks[1]) |
           (this->chunks[2] | this->chunks[3]);
  }

  sonic_force_inline uint64_t eq(const T m) const {
    const simd8<T> mask = simd8<T>::splat(m);
    return merge_bitmask(this->chunks[0] == mask, this->chunks[1] == mask,
                         this->chunks[2] == mask, this->chunks[3] == mask);
  }

  sonic_force_inline uint64_t lteq(const T m) const {
    const simd8<T> mask = simd8<T>::splat(m);
    return merge_bitmask(this->chunks[0] <= mask, this->chunks[1] <= mask,
                         this->chunks[2] <= mask, this->chunks[3] <= mask);
  }
};  // struct simd8x64<T>

}  // namespace rvv_128
}  // namespace internal
}  // namespace sonic_json
