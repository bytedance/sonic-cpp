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

#include "../avx2/base.h"
#include "../sse/base.h"

namespace sonic_json {
namespace internal {
using sse::ClearLowestBit;
using sse::CountOnes;
using sse::LeadingZeroes;
using sse::PrefixXor;
using sse::TrailingZeroes;

__attribute__((target("default"))) inline void Xmemcpy_32(void*, const void*,
                                                          size_t) {
  // TODO static_assert(!!!"Not Implemented!");
  return;
}

__attribute__((target("default"))) inline void Xmemcpy_16(void*, const void*,
                                                          size_t) {
  // TODO static_assert(!!!"Not Implemented!");
  return;
}

__attribute__((target(SONIC_WESTMERE))) inline void Xmemcpy_32(void* dst,
                                                               const void* src,
                                                               size_t chunks) {
  return sse::Xmemcpy<32>(dst, src, chunks);
}

__attribute__((target(SONIC_WESTMERE))) inline void Xmemcpy_16(void* dst,
                                                               const void* src,
                                                               size_t chunks) {
  return sse::Xmemcpy<16>(dst, src, chunks);
}

__attribute__((target(SONIC_HASWELL))) inline void Xmemcpy_32(void* dst,
                                                              const void* src,
                                                              size_t chunks) {
  return avx2::Xmemcpy<32>(dst, src, chunks);
}

__attribute__((target(SONIC_HASWELL))) inline void Xmemcpy_16(void* dst,
                                                              const void* src,
                                                              size_t chunks) {
  return avx2::Xmemcpy<16>(dst, src, chunks);
}

template <size_t ChunkSize>
sonic_force_inline void Xmemcpy(void* dst, const void* src, size_t chunks) {
  std::memcpy(dst, src, chunks * ChunkSize);
  return;
}

template <>
sonic_force_inline void Xmemcpy<32>(void* dst, const void* src, size_t chunks) {
  return Xmemcpy_32(dst, src, chunks);
}

template <>
sonic_force_inline void Xmemcpy<16>(void* dst, const void* src, size_t chunks) {
  return Xmemcpy_16(dst, src, chunks);
}

}  // namespace internal
}  // namespace sonic_json
