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

#include "../avx2/quote.h"
#include "../sse/quote.h"

namespace sonic_json {
namespace internal {
__attribute__((target("default"))) inline size_t parseStringInplace(
    uint8_t *&, SonicError &) {
  // TODO static_assert(!!!"Not Implemented!");
  return 0;
}

__attribute__((target("default"))) inline char *Quote(const char *, size_t,
                                                      char *) {
  // TODO static_assert(!!!"Not Implemented!");
  return 0;
}

__attribute__((target(SONIC_WESTMERE))) inline size_t parseStringInplace(
    uint8_t *&src, SonicError &err) {
  return sse::parseStringInplace(src, err);
}

__attribute__((target(SONIC_WESTMERE))) inline char *Quote(const char *src,
                                                           size_t nb,
                                                           char *dst) {
  return sse::Quote(src, nb, dst);
}

__attribute__((target(SONIC_HASWELL))) inline size_t parseStringInplace(
    uint8_t *&src, SonicError &err) {
  return avx2::parseStringInplace(src, err);
}

__attribute__((target(SONIC_HASWELL))) inline char *Quote(const char *src,
                                                          size_t nb,
                                                          char *dst) {
  return avx2::Quote(src, nb, dst);
}

}  // namespace internal
}  // namespace sonic_json
