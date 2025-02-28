// Copyright 2018-2019 The simdjson authors

// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0

// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// This file may have been modified by ByteDance authors. All ByteDance
// Modifications are Copyright 2022 ByteDance Authors.

#pragma once

#include "../common/arm_common/base.h"

namespace sonic_json {
namespace internal {
namespace sve2_128 {

using sonic_json::internal::arm_common::ClearLowestBit;
using sonic_json::internal::arm_common::CountOnes;
using sonic_json::internal::arm_common::InlinedMemcmp;
using sonic_json::internal::arm_common::InlinedMemcmpEq;
using sonic_json::internal::arm_common::LeadingZeroes;
using sonic_json::internal::arm_common::PrefixXor;
using sonic_json::internal::arm_common::TrailingZeroes;
using sonic_json::internal::arm_common::Xmemcpy;

}  // namespace sve2_128
}  // namespace internal
}  // namespace sonic_json
