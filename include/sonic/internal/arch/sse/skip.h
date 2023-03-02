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

#include <immintrin.h>
#include <sonic/dom/json_pointer.h>
#include <sonic/error.h>
#include <sonic/internal/utils.h>
#include <sonic/macro.h>

#include "../common/skip_common.h"
#include "base.h"
#include "quote.h"
#include "simd.h"
#include "unicode.h"

#if defined(__clang__)
#pragma clang attribute push( \
    __attribute__((target("pclmul,sse,sse2,sse4.1"))), apply_to = function)
#elif defined(__GNUG__)
#pragma GCC push_options
#pragma GCC target("pclmul,sse,sse2,sse4.1")
#else
#error "Only g++ and clang is supported!"
#endif

#define VEC_LEN 16

namespace sonic_json {
namespace internal {
namespace sse {

using VecUint8Type = simd::simd128<uint8_t>;
using VecBoolType = simd::simd128<bool>;
using sonic_json::internal::common::EqBytes4;
using sonic_json::internal::common::SkipLiteral;

#include "../common/x86_common/skip.inc.h"

}  // namespace sse
}  // namespace internal
}  // namespace sonic_json

#undef VEC_LEN

#if defined(__clang__)
#pragma clang attribute pop
#elif defined(__GNUG__)
#pragma GCC pop_options
#endif
