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

#include "sonic/macro.h"

namespace sonic_json {
enum TypeFlag {
  // BasicType: 3 bits
  kNull = 0,    // xxxxx000
  kBool = 2,    // xxxxx010
  kNumber = 3,  // xxxxx011
  kString = 4,  // xxxxx100
  kRaw = 5,     // xxxxx101
  // Container Mask 00000110, & Mask == Mask
  kObject = 6,  // xxxxx110
  kArray = 7,   // xxxxx111

  // SubType: 2 bits
  kFalse = ((uint8_t)(0 << 3)) | kBool,   // xxx00_010, 2
  kTrue = ((uint8_t)(1 << 3)) | kBool,    // xxx01_010, 10
  kUint = ((uint8_t)(0 << 3)) | kNumber,  // xxx00_011, 3
  kSint = ((uint8_t)(1 << 3)) | kNumber,  // xxx01_011, 11
  kReal = ((uint8_t)(2 << 3)) | kNumber,  // xxx10_011, 19
  // kStringCopy: sv.p is copied, but not need free, e.g. node's string buffer
  // is dom str_
  kStringCopy = kString,  // xxx00_100, 4
  // kStringFree: sv.p is copied and need free, e.g. SetString with allocator
  // arg
  kStringFree = ((uint8_t)(1 << 3)) | kString,  // xxx01_100, 12
  // kStringConst: sv.p is not copied, so not need free, e.g. SetString without
  // allocator arg
  kStringConst = ((uint8_t)(2 << 3)) | kString,  // xxx10_100, 20

};  // 8 bits

enum TypeInfo {
  kTotalTypeBits = 8,

  // BasicType: 3 bits
  kBasicTypeBits = 3,
  kBasicTypeMask = 0x7,

  // SubType: 5 bits (including basic 3 bits)
  kSubTypeBits = 2,
  kSubTypeMask = 0x1F,

  // Others
  kInfoBits = 8,
  kInfoMask = (1 << 8) - 1,
  kOthersBits = 56,
  kLengthMask = (0xFFFFFFFFFFFFFFFF << 8),
  kContainerMask = 0x6,  // 00000110
};

}  // namespace sonic_json
