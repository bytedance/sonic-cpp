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

#include "../avx2/skip.h"
#include "../sse/skip.h"

namespace sonic_json {
namespace internal {

using common::EqBytes4;
using common::SkipLiteral;
using sse::GetNextToken;  // !!!Not efficency

__attribute__((target("default"))) inline int SkipString(const uint8_t*,
                                                         size_t&, size_t) {
  // TODO static_assert(!!!"Not Implemented!");
  return 0;
}

__attribute__((target("default"))) inline bool SkipContainer(const uint8_t*,
                                                             size_t&, size_t,
                                                             uint8_t, uint8_t) {
  // TODO static_assert(!!!"Not Implemented!");
  return 0;
}

__attribute__((target("default"))) inline uint8_t skip_space(const uint8_t*,
                                                             size_t&, size_t&,
                                                             uint64_t&) {
  // TODO static_assert(!!!"Not Implemented!");
  return 0;
}

__attribute__((target("default"))) inline uint8_t skip_space_safe(
    const uint8_t*, size_t&, size_t, size_t&, uint64_t&) {
  // TODO static_assert(!!!"Not Implemented!");
  return 0;
}

__attribute__((target(SONIC_WESTMERE))) inline int SkipString(
    const uint8_t* data, size_t& pos, size_t len) {
  return sse::SkipString(data, pos, len);
}

__attribute__((target(SONIC_WESTMERE))) inline bool SkipContainer(
    const uint8_t* data, size_t& pos, size_t len, uint8_t left, uint8_t right) {
  return sse::SkipContainer(data, pos, len, left, right);
}

__attribute__((target(SONIC_WESTMERE))) inline uint8_t skip_space(
    const uint8_t* data, size_t& pos, size_t& nonspace_bits_end,
    uint64_t& nonspace_bits) {
  return sse::skip_space(data, pos, nonspace_bits_end, nonspace_bits);
}

__attribute__((target(SONIC_WESTMERE))) inline uint8_t skip_space_safe(
    const uint8_t* data, size_t& pos, size_t len, size_t& nonspace_bits_end,
    uint64_t& nonspace_bits) {
  return sse::skip_space_safe(data, pos, len, nonspace_bits_end, nonspace_bits);
}

__attribute__((target(SONIC_HASWELL))) inline int SkipString(
    const uint8_t* data, size_t& pos, size_t len) {
  return avx2::SkipString(data, pos, len);
}

__attribute__((target(SONIC_HASWELL))) inline bool SkipContainer(
    const uint8_t* data, size_t& pos, size_t len, uint8_t left, uint8_t right) {
  return avx2::SkipContainer(data, pos, len, left, right);
}

__attribute__((target(SONIC_HASWELL))) inline uint8_t skip_space(
    const uint8_t* data, size_t& pos, size_t& nonspace_bits_end,
    uint64_t& nonspace_bits) {
  return avx2::skip_space(data, pos, nonspace_bits_end, nonspace_bits);
}

__attribute__((target(SONIC_HASWELL))) inline uint8_t skip_space_safe(
    const uint8_t* data, size_t& pos, size_t len, size_t& nonspace_bits_end,
    uint64_t& nonspace_bits) {
  return avx2::skip_space_safe(data, pos, len, nonspace_bits_end,
                               nonspace_bits);
}

}  // namespace internal
}  // namespace sonic_json
