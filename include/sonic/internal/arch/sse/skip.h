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

#include "base.h"
#include "quote.h"
#include "simd.h"
#include "unicode.h"

namespace sonic_json {
namespace internal {
namespace x86_common {

using sse::GetEscapedBranchless;
using sse::GetNonSpaceBits;
using VecUint8Type = simd::simd128<uint8_t>;
using VecBoolType = simd::simd128<bool>;

}  // namespace x86_common
}  // namespace internal
}  // namespace sonic_json

#include "../common/x86_common/skip.h"

namespace sonic_json {
namespace internal {
namespace sse {

using sonic_json::internal::common::EqBytes4;
using sonic_json::internal::common::SkipLiteral;
using sonic_json::internal::x86_common::GetNextToken;
using sonic_json::internal::x86_common::GetStringBits;
using sonic_json::internal::x86_common::SkipArray;
using sonic_json::internal::x86_common::SkipContainer;
using sonic_json::internal::x86_common::SkipNumber;
using sonic_json::internal::x86_common::SkipObject;
using sonic_json::internal::x86_common::SkipScanner;
using sonic_json::internal::x86_common::SkipString;

}  // namespace sse
}  // namespace internal
}  // namespace sonic_json
