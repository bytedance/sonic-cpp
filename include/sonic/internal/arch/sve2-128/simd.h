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

#include <arm_sve.h>

#include "../common/arm_common/simd.h"

static_assert(__ARM_FEATURE_SVE_BITS == 128, "SVE vector size must be 128");
typedef svuint8_t svuint8x16_t __attribute__((arm_sve_vector_bits(128)));

namespace sonic_json {
namespace internal {
namespace sve2_128 {

using sonic_json::internal::arm_common::to_bitmask;

}  // namespace sve2_128
}  // namespace internal
}  // namespace sonic_json
