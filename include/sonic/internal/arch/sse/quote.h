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
#define VEC_LEN 16

SONIC_PUSH_WESTMERE

namespace sonic_json {
namespace internal {
namespace sse {

using VecType = simd::simd128<uint8_t>;
#include "../common/x86_common/quote.inc.h"

}  // namespace sse
}  // namespace internal
}  // namespace sonic_json

#undef VEC_LEN
#undef VEC_FULL_MASK

SONIC_POP_TARGET
