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

SONIC_PUSH_HASWELL

#if defined(VEC_FULL_MASK) || defined(VEC_LEN)
#error "VEC_FULL_MASK and VEC_LEN has been defined! This may cause error."
#endif

#define VEC_FULL_MASK 0xFFFFFFFF
#define VEC_LEN 32

namespace sonic_json {
namespace internal {
namespace avx2 {

using VecType = simd::simd256<uint8_t>;

#include "../common/x86_common/quote.inc.h"

}  // namespace avx2
}  // namespace internal
}  // namespace sonic_json

#undef VEC_FULL_MASK
#undef VEC_LEN

SONIC_POP_TARGET
