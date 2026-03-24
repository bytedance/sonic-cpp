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

#include <cstdint>

// ParseFlag is one-hot encoded for different parsing option.
// User can define customized flags through combinations.
enum class ParseFlags : uint32_t {
  kParseDefault = 0,
  kParseAllowUnescapedControlChars = 1 << 1,
  // parse all integer as raw number
  kParseIntegerAsRaw = 1 << 2,
  // Parse numbers as number strings (NumStr) only when they overflow the
  // native numeric representation. In-range floating-point numbers stay
  // double; in-range integers stay int64/uint64.
  kParseOverflowNumAsNumStr = 1 << 3,
};

// Compatibility layer for downstream users.
// Note: ParseFlag was the previous unscoped enum type.
using ParseFlag [[deprecated("Use ParseFlags instead")]] = ParseFlags;
[[deprecated("Use ParseFlags::kParseDefault instead")]] constexpr ParseFlags
    kParseDefault = ParseFlags::kParseDefault;
[[deprecated(
    "Use ParseFlags::kParseAllowUnescapedControlChars "
    "instead")]] constexpr ParseFlags kParseAllowUnescapedControlChars =
    ParseFlags::kParseAllowUnescapedControlChars;
[[deprecated(
    "Use ParseFlags::kParseIntegerAsRaw instead")]] constexpr ParseFlags
    kParseIntegerAsRaw = ParseFlags::kParseIntegerAsRaw;
[[deprecated(
    "Use ParseFlags::kParseOverflowNumAsNumStr instead")]] constexpr ParseFlags
    kParseOverflowNumAsNumStr = ParseFlags::kParseOverflowNumAsNumStr;

constexpr ParseFlags operator|(ParseFlags lhs, ParseFlags rhs) {
  return static_cast<ParseFlags>(static_cast<uint32_t>(lhs) |
                                 static_cast<uint32_t>(rhs));
}

constexpr bool operator&(ParseFlags lhs, ParseFlags rhs) {
  return (static_cast<uint32_t>(lhs) & static_cast<uint32_t>(rhs)) != 0;
}

// SerializeFlags is one-hot encoded for different serializing option.
// User can define customized flags through combinations.
enum class SerializeFlags : uint32_t {
  kSerializeDefault = 0,
  kSerializeAppendBuffer = 1 << 1,
  kSerializeEscapeEmoji = 1 << 2,
  kSerializeInfNan = 1 << 3,
  kSerializeUnicodeEscapeUppercase = 1 << 4,
  kSerializeFloatFormatJava = 1 << 5,
};

// Compatibility layer for downstream users.
// Note: SerializeFlag was the previous unscoped enum type.
using SerializeFlag [[deprecated("Use SerializeFlags instead")]] =
    SerializeFlags;
[[deprecated(
    "Use SerializeFlags::kSerializeDefault instead")]] constexpr SerializeFlags
    kSerializeDefault = SerializeFlags::kSerializeDefault;
[[deprecated(
    "Use SerializeFlags::kSerializeAppendBuffer "
    "instead")]] constexpr SerializeFlags kSerializeAppendBuffer =
    SerializeFlags::kSerializeAppendBuffer;
[[deprecated(
    "Use SerializeFlags::kSerializeEscapeEmoji "
    "instead")]] constexpr SerializeFlags kSerializeEscapeEmoji =
    SerializeFlags::kSerializeEscapeEmoji;
[[deprecated(
    "Use SerializeFlags::kSerializeInfNan instead")]] constexpr SerializeFlags
    kSerializeInfNan = SerializeFlags::kSerializeInfNan;
[[deprecated(
    "Use SerializeFlags::kSerializeUnicodeEscapeUppercase "
    "instead")]] constexpr SerializeFlags kSerializeUnicodeEscapeUppercase =
    SerializeFlags::kSerializeUnicodeEscapeUppercase;
[[deprecated(
    "Use SerializeFlags::kSerializeFloatFormatJava "
    "instead")]] constexpr SerializeFlags kSerializeFloatFormatJava =
    SerializeFlags::kSerializeFloatFormatJava;

constexpr SerializeFlags operator|(SerializeFlags lhs, SerializeFlags rhs) {
  return static_cast<SerializeFlags>(static_cast<uint32_t>(lhs) |
                                     static_cast<uint32_t>(rhs));
}
constexpr bool operator&(SerializeFlags lhs, SerializeFlags rhs) {
  return (static_cast<uint32_t>(lhs) & static_cast<uint32_t>(rhs)) != 0;
}

constexpr static auto kSerializeJavaStyleFlag =
    SerializeFlags::kSerializeFloatFormatJava |
    SerializeFlags::kSerializeUnicodeEscapeUppercase;
