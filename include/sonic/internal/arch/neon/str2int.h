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

namespace sonic_json {
namespace internal {
namespace neon {

sonic_force_inline uint64_t simd_str2int(const char* c, int& man_nd) {
  uint64_t sum = 0;
  int i = 0;
  while (c[i] >= '0' && c[i] <= '9' && i < man_nd) {
    sum = sum * 10 + (c[i] - '0');
    i++;
  }
  man_nd = i;
  return sum;
}

}  // namespace neon
}  // namespace internal
}  // namespace sonic_json
