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

#include <cstddef>
#include <cstdint>

namespace sonic_json {
namespace internal {
namespace riscv {

// Scalar implementation of Utoa_4_helper: convert num {abcd} to digits
static sonic_force_inline void Utoa_4_helper_scalar(uint16_t num,
                                                    uint8_t digits[4]) {
  digits[0] = static_cast<uint8_t>((num / 1000) % 10);
  digits[1] = static_cast<uint8_t>((num / 100) % 10);
  digits[2] = static_cast<uint8_t>((num / 10) % 10);
  digits[3] = static_cast<uint8_t>(num % 10);
}

// Convert num's each digit to bytes.
// num's digits as abcdefgh (high bits is 0 if not enough)
// The converted digits are { a, b, c, d, e, f, g, h }
static sonic_force_inline void Utoa_Scalar(uint32_t num, uint8_t digits[8]) {
  uint16_t hi = static_cast<uint16_t>(num % 10000);  // {efgh}
  uint16_t lo = static_cast<uint16_t>(num / 10000);  // {abcd}

  uint8_t lo_digits[4];
  uint8_t hi_digits[4];
  Utoa_4_helper_scalar(lo, lo_digits);
  Utoa_4_helper_scalar(hi, hi_digits);

  digits[0] = lo_digits[0];
  digits[1] = lo_digits[1];
  digits[2] = lo_digits[2];
  digits[3] = lo_digits[3];
  digits[4] = hi_digits[0];
  digits[5] = hi_digits[1];
  digits[6] = hi_digits[2];
  digits[7] = hi_digits[3];
}

static sonic_force_inline char *Utoa_8(uint32_t val, char *out) {
  uint8_t digits[8];
  Utoa_Scalar(val, digits);
  for (int i = 0; i < 8; i++) {
    out[i] = static_cast<char>('0' + digits[i]);
  }
  return out + 8;
}

static sonic_force_inline char *Utoa_16(uint64_t val, char *out) {
  uint8_t digits_hi[8];
  uint8_t digits_lo[8];
  Utoa_Scalar(static_cast<uint32_t>(val / 100000000), digits_hi);
  Utoa_Scalar(static_cast<uint32_t>(val % 100000000), digits_lo);

  for (int i = 0; i < 8; i++) {
    out[i] = static_cast<char>('0' + digits_hi[i]);
  }
  for (int i = 0; i < 8; i++) {
    out[i + 8] = static_cast<char>('0' + digits_lo[i]);
  }
  return out + 16;
}

}  // namespace riscv
}  // namespace internal
}  // namespace sonic_json
