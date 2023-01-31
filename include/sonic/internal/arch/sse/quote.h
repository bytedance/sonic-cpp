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

#include <sonic/error.h>
#include <sonic/macro.h>

#include <cstring>
#include <vector>

#include "../common/quote_common.h"
#include "../common/quote_tables.h"
#include "base.h"
#include "simd.h"
#include "unicode.h"

#ifndef VEC_FULL_MASK
#define VEC_FULL_MASK 0xFFFF
#endif

namespace sonic_json {
namespace internal {
namespace x86_common {

using StringBlock = sse::StringBlock;
using VecType = simd::simd128<uint8_t>;

static sonic_force_inline int CopyAndGetEscapMask(const char *src, char *dst) {
  simd::simd128<uint8_t> v(reinterpret_cast<const uint8_t *>(src));
  v.store(reinterpret_cast<uint8_t *>(dst));
  return ((v < '\x20') | (v == '\\') | (v == '"')).to_bitmask();
}

}  // namespace x86_common
}  // namespace internal
}  // namespace sonic_json

#include "../common/x86_common/quote.h"

namespace sonic_json {
namespace internal {
namespace sse {

using sonic_json::internal::x86_common::parseStringInplace;
using sonic_json::internal::x86_common::Quote;

}  // namespace sse
}  // namespace internal
}  // namespace sonic_json
