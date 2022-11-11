// Copyright (C) 2019 Yaoyuan <ibireme@gmail.com>.

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

#include <stdint.h>

namespace sonic_json {
namespace internal {
inline bool ParseFloatingNormalFast(uint64_t& d_raw, int exp10, uint64_t man,
                                    int sgn) {
  // This function is copyed from
  // https://github.com/ibireme/yyjson/blob/
  // 68d08a59faff64c598a84e6cc75e9df5699db44a/src/yyjson.c#L3404
  uint64_t sig1, sig2, sig2_ext, hi, lo, hi2, lo2, add, bits;
  int32_t exp2;
  uint32_t lz;
  bool exact = false, carry, round_up;
  {
    int idx = exp10 + 348;
    sig2_ext = internal::kPow10M128Tab[idx][0];
    sig2 = internal::kPow10M128Tab[idx][1];
    // TODO:
  }
  lz = internal::haswell::leading_zeroes(man);
  sig1 = man << lz;
  exp2 = ((217706 * exp10 - 4128768) >> 16) - lz;

  internal::MulU64(sig1, sig2, hi,
                   lo);  // higher 64 bits of POW10 mantissa

  bits = hi & (((uint64_t)1 << (64 - 54 - 1)) - 1);
  if (bits - 1 < (((uint64_t)1 << (64 - 54 - 1)) - 2)) {
    exact = true;
  } else {
    internal::MulU64(sig1, sig2_ext, hi2, lo2);
    add = lo + hi2;
    if (add + 1 > (uint64_t)1) {
      carry = add < lo || add < hi2;
      hi += carry;
      exact = true;
    }
  }

  if (exact) {
    lz = hi < ((uint64_t)1 << 63);
    hi <<= lz;
    exp2 -= (int32_t)lz;
    exp2 += 64;

    round_up = (hi & ((uint64_t)1 << (64 - 54))) > (uint64_t)0;
    hi += (round_up ? ((uint64_t)1 << (64 - 54)) : (uint64_t)0);

    if (hi < ((uint64_t)1 << (64 - 54))) {
      hi = ((uint64_t)1 << 63);
      exp2 += 1;
    }

#define F64_BITS 64
#define F64_SIG_BITS 52
#define F64_SIG_FULL_BITS 53
#define F64_EXP_BIAS 1023
#define F64_SIG_MASK 0x000FFFFFFFFFFFFF
    hi >>= F64_BITS - F64_SIG_FULL_BITS;
    exp2 += F64_BITS - F64_SIG_FULL_BITS + F64_SIG_BITS;
    exp2 += F64_EXP_BIAS;
    d_raw = ((uint64_t)exp2 << F64_SIG_BITS) | (hi & F64_SIG_MASK);
    d_raw |= (((uint64_t)(sgn) >> 63) << 63);
    return true;
#undef F64_BIT
#undef F64_SIG_BITS
#undef F64_SIG_FULL_BITS
#undef F64_EXP_BIAS
#undef F64_SIG_MASK
  }  // else { // not exact, goto eisel_lemire64; }
  return false;
}

}  // namespace internal
}  // namespace sonic_json
