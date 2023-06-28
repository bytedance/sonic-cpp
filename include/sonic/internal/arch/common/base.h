
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

#include <cstdint>

#include "sonic/macro.h"

namespace sonic_json {
namespace internal {
namespace common {

sonic_force_inline bool AddOverflow64(uint64_t value1, uint64_t value2,
                                      uint64_t* result) {
  return __builtin_uaddll_overflow(
      value1, value2, reinterpret_cast<unsigned long long*>(result));
}

sonic_force_inline bool AddOverflow32(uint32_t value1, uint32_t value2,
                                      uint64_t* result) {
  unsigned ret = 0;
  bool is_over = __builtin_uadd_overflow(value1, value2, &ret);
  *result = ret;
  return is_over;
}

}  // namespace common
}  // namespace internal
}  // namespace sonic_json
