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

#include <arm_neon.h>

#include "sonic/macro.h"

namespace sonic_json {
namespace internal {
namespace neon {

#ifndef VEC_LEN
#define VEC_LEN 16
#endif

sonic_force_inline uint64_t to_bitmask(uint8x16_t v) {
  return vget_lane_u64(
      vreinterpret_u64_u8(vshrn_n_u16(vreinterpretq_u16_u8(v), 4)), 0);
}

}  // namespace neon
}  // namespace internal
}  // namespace sonic_json
