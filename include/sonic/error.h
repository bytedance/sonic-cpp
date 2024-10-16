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

#include <string>

#include "sonic/macro.h"

namespace sonic_json {

enum SonicError {
  kErrorNone = 0,              ///< No errors.
  kParseErrorEof = 1,          ///< Parse: JSON is empty or truncated.
  kParseErrorInvalidChar = 2,  ///< Parse: JSON has invalid chars, e.g. 1.2x.
  kParseErrorInfinity = 3,     ///< Parse: JSON number is infinity.
  kParseErrorUnEscaped = 4,  ///< Parse: JSON string has unescaped control chars
                             ///< (\x00 ~ \x1f)
  kParseErrorEscapedFormat = 5,   ///< Parse: JSON string has wrong escaped
                                  ///< format, e.g.
                                  ///< "\\g"
  kParseErrorEscapedUnicode = 6,  ///< Parse: JSON string has wrong escaped
                                  ///< unicode, e.g.
                                  ///< "\\uD800"
  kParseErrorInvalidUTF8 = 7,     ///< Parse: JSON string has wrong escaped
                                  ///< unicode, e.g.
                                  ///< "\xff\xff"
  kParseErrorUnknownObjKey = 8,   ///< ParseOnDemand: Not found the target keys
                                  ///< in object.
  kParseErrorArrIndexOutOfRange = 9,  ///< ParseOnDemand: the target array index
                                      ///< out of range.
  kParseErrorMismatchType =
      10,  ///< ParseOnDemand: the target type is not matched.
  kSerErrorUnsupportedType = 11,  ///< Serialize: DOM has invalid node type.
  kSerErrorInfinity = 12,         ///< Serialize: DOM has inifinity number node.
  kSerErrorInvalidObjKey = 13,  ///< Serialize: The type of object's key is not
                                ///< string.
  kErrorNoMem = 14,             ///< Memory is not enough to allocate.
  kParseErrorUnexpect = 15,     ///< Unexpected Errors

  kSaxTermination = 16,       ///< Parse: SAX handler return false to
                              ///< terminate parsing.
  kUnsupportedJsonPath = 17,  ///< JsonPath: Unsupported json path.
  kNotFoundByJsonPath = 18,   ///< JsonPath: Not found the target by json path.
  kUnmatchedTypeInJsonPath = 19, ///< JsonPath: The type of node is not matched.
  kErrorNums,
};

inline const char* ErrorMsg(SonicError error) noexcept {
  struct SonicErrorInfo {
    SonicError err;
    const char* msg;
  };
  static const SonicErrorInfo kErrorMsg[kErrorNums] = {
      {kErrorNone, "No errors"},
      {kParseErrorEof, "Parse: JSON is empty or truncated."},
      {kParseErrorInvalidChar, "Parse: JSON has invalid chars, e.g. 1.2x."},
      {kParseErrorInfinity, "Parse: JSON number is infinity."},
      {kParseErrorUnEscaped,
       "Parse: JSON string has unescaped control chars (\\x00 ~ \\x1f)."},
      {kParseErrorEscapedFormat,
       "Parse: JSON string has wrong escaped format, e.g.\"\\g\""},
      {kParseErrorEscapedUnicode,
       "Parse: JSON string has wrong escaped unicode, e.g. \"\\uD800\""},
      {kParseErrorInvalidUTF8,
       "Parse: JSON string has wrong escaped unicode, e.g. \"\\xff\\xff\""},
      {kParseErrorUnknownObjKey,
       "ParseOnDemand: Not found the target keys in object."},
      {kParseErrorArrIndexOutOfRange,
       "ParseOnDemand: the target array index out of range."},
      {kParseErrorMismatchType,
       "ParseOnDemand: the target type is not matched."},
      {kSerErrorUnsupportedType, "Serialize: DOM has invalid node type."},
      {kSerErrorInfinity, "Serialize: DOM has inifinity number node."},
      {kSerErrorInvalidObjKey,
       "Serialize: The type of object's key is not string."},
      {kErrorNoMem, "Memory is not enough to allocate."},
      {kParseErrorUnexpect, "Unexpected Errors"},
      {kSaxTermination,
       "Parse: SAX handler return false to terminate parsing."},
      {kUnsupportedJsonPath, "JsonPath: Unsupported json path."},
      {kNotFoundByJsonPath, "JsonPath: Not found the target by json path."}

  };
  return kErrorMsg[error].msg;
};

struct ParseResult {
 public:
  ParseResult() noexcept {}
  ParseResult(SonicError err, size_t off) noexcept : err_(err), off_(off) {}
  ParseResult(SonicError err) noexcept : err_(err) {}
  sonic_force_inline SonicError Error() const noexcept { return err_; }
  sonic_force_inline size_t Offset() const noexcept { return off_; }

 private:
  SonicError err_{kErrorNone};
  size_t off_{0};
};

}  // namespace sonic_json
