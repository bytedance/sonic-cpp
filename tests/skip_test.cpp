/*
 * Copyright 2023 ByteDance Inc.
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

#include <limits>
#include <string>

#include "gtest/gtest.h"
#include "sonic/dom/parser.h"

namespace {

using namespace sonic_json;

void TestGetOnDemand(StringView json, const JsonPointer& path,
                     StringView expect) {
  StringView target;
  auto result = GetOnDemand(json, path, target);
  EXPECT_EQ(result.Error(), sonic_json::kErrorNone) << json;
  EXPECT_EQ(target, expect) << json;
}

void TestGetOnDemandFailed(StringView json, const JsonPointer& path,
                           ParseResult expect) {
  StringView target;
  auto result = GetOnDemand(json, path, target);
  EXPECT_EQ(result.Error(), expect.Error()) << json << expect.Error();
  EXPECT_EQ(result.Offset(), expect.Offset()) << json << expect.Error();
  EXPECT_EQ(target, "");
}

void TestGetOnDemandError(StringView json, const JsonPointer& path,
                          SonicError expect) {
  StringView target;
  auto result = GetOnDemand(json, path, target);
  EXPECT_EQ(result.Error(), expect) << json << result.Offset();
  EXPECT_EQ(target, "");
}

TEST(GetOnDemand, SuccessBasic) {
  TestGetOnDemand("{}", {}, "{}");
  TestGetOnDemand("1", {}, "1");
  TestGetOnDemand(R"({"a":1})", {"a"}, "1");
  TestGetOnDemand(R"({"a":"1"})", {"a"}, "\"1\"");
  TestGetOnDemand("[1, 2, 3, null]", {3}, "null");
}

TEST(GetOnDemand, SuccessNestedObject) {
  TestGetOnDemand(R"({"a":{"b":{"c":1}}})", {"a", "b", "c"}, "1");
  TestGetOnDemand(R"({"a":{"b":{"c":true}}})", {"a", "b", "c"}, "true");
  TestGetOnDemand(R"({"a":{"b":{"c":"hello, world!"}}})", {"a", "b", "c"},
                  R"("hello, world!")");
}

TEST(GetOnDemand, SuccessNestedArray) {
  TestGetOnDemand(R"([[1], [2, 3], [4, 5, 6]])", {1, 1}, "3");
  TestGetOnDemand(R"([[1], [2, 3], [4, 5, 6]])", {2, 2}, "6");
}

TEST(GetOnDemand, SuccessUnicode) {
  TestGetOnDemand(
      R"("未来简史-从智人到智神hello: world, \\ {\" / \b \f \n \r \t } [景] 测试中文 😀")",
      {},
      R"("未来简史-从智人到智神hello: world, \\ {\" / \b \f \n \r \t } [景] 测试中文 😀")");
  TestGetOnDemand(R"({"a":"你好，世界！"})", {"a"}, R"("你好，世界！")");
  TestGetOnDemand(R"({"a":"こんにちは、世界！"})", {"a"},
                  R"("こんにちは、世界！")");
  TestGetOnDemand(R"({"a":"안녕하세요, 세상!"})", {"a"},
                  R"("안녕하세요, 세상!")");
}

TEST(GetOnDemand, SuccessEscapeCharacters) {
  TestGetOnDemand(R"({"a":"\n\tHello,\nworld!\n"})", {"a"},
                  R"("\n\tHello,\nworld!\n")");
  TestGetOnDemand(R"({"a":"\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\"})", {"a"},
                  R"("\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\")");
  TestGetOnDemand(R"({"a":"\"\\\\\\\\\\\\\\\\\\\\\\\\\\\\\""})", {"a"},
                  R"("\"\\\\\\\\\\\\\\\\\\\\\\\\\\\\\"")");
  TestGetOnDemand(R"({"\"a\"":"\n\tHello,\nworld!\n"})", {"\"a\""},
                  R"("\n\tHello,\nworld!\n")");
  TestGetOnDemand(R"({"123456789012345\"123":"sse_string",
                      "1234567890123456789012345678901\"123":"avx2_string",
                      "obj\n\t\\":{"name\\\\\\\\":"string\\\\"},
                      "array\"\t\n\b\r":["\n\tHello,\nworld!\n",
                        "{\" / \b \f \n \r \t } [景] 测试中文 😀"],
                      "\"a\"":"\n\tHello,\nworld!\n"})",
                  {"\"a\""}, R"("\n\tHello,\nworld!\n")");
}

TEST(GetOnDemand, Failed) {
  TestGetOnDemandFailed("{}", {1}, ParseResult(kParseErrorMismatchType, 0));
  TestGetOnDemandFailed("{}", {"a"}, ParseResult(kParseErrorUnknownObjKey, 1));

  TestGetOnDemandFailed("{123}", {"a"}, ParseResult(kParseErrorInvalidChar, 2));

  TestGetOnDemandFailed("[]", {1},
                        ParseResult(kParseErrorArrIndexOutOfRange, 2));
  TestGetOnDemandFailed("[]", {0},
                        ParseResult(kParseErrorArrIndexOutOfRange, 2));

  TestGetOnDemandError(R"("\")", {}, kParseErrorInvalidChar);
}

TEST(GetOnDemand, EmptyInputDoesNotReadBeforeBuffer) {
  TestGetOnDemandFailed("", {}, ParseResult(kParseErrorInvalidChar, 0));
}

TEST(GetOnDemand, LargeIntegralPointerDoesNotWrapToZero) {
  JsonPointer path{JsonPointerNode(uint64_t{1} << 32)};
  TestGetOnDemandFailed("[10,20]", path,
                        ParseResult(kParseErrorArrIndexOutOfRange, 6));
}

TEST(GetOnDemand, HugeUnsignedPointerDoesNotWrapThroughSignedStorage) {
  JsonPointer path{JsonPointerNode(std::numeric_limits<uint64_t>::max())};
  TestGetOnDemandFailed("[10,20]", path,
                        ParseResult(kParseErrorArrIndexOutOfRange, 6));
}

TEST(GetOnDemand, NegativePointerIndexReportsOutOfRange) {
  JsonPointer path{JsonPointerNode(-1)};
  TestGetOnDemandFailed("[10,20]", path,
                        ParseResult(kParseErrorArrIndexOutOfRange, 1));
}

TEST(GetOnDemand, RejectsInvalidSkippedValuesAndTrailingTargetGarbage) {
  TestGetOnDemandFailed(R"({"x":1abc "a":2})", {"a"},
                        ParseResult(kParseErrorInvalidChar, 6));
  TestGetOnDemandFailed(R"([1abc,2])", {1},
                        ParseResult(kParseErrorInvalidChar, 2));
  TestGetOnDemandFailed(R"({"a":[1] 2})", {"a"},
                        ParseResult(kParseErrorInvalidChar, 9));
  TestGetOnDemandFailed(R"({"a":"x" 2})", {"a"},
                        ParseResult(kParseErrorInvalidChar, 9));
  TestGetOnDemandError(R"({"x":{]},"a":2})", {"a"}, kParseErrorInvalidChar);
  TestGetOnDemandError(R"({"x":[}],"a":2})", {"a"}, kParseErrorInvalidChar);
  TestGetOnDemandError("{]}", {}, kParseErrorInvalidChar);
  TestGetOnDemandError("[}]", {}, kParseErrorInvalidChar);
  TestGetOnDemandError(R"({"a":1]})", {"a"}, kParseErrorInvalidChar);
  TestGetOnDemandError("1]", {}, kParseErrorInvalidChar);
  TestGetOnDemandError(R"({"a":1,"bad":"\q"})", {"missing"},
                       kParseErrorEscapedFormat);
}

TEST(GetOnDemand, RejectsInvalidMatchedPathContainerSuffix) {
  TestGetOnDemandError(R"({"a":[1] 2})", {"a", 0}, kParseErrorInvalidChar);
  TestGetOnDemandError(R"({"a":{"b":1} 2})", {"a", "b"},
                       kParseErrorInvalidChar);
  TestGetOnDemandError(R"([[1] 2])", {0, 0}, kParseErrorInvalidChar);
  TestGetOnDemandError(R"({"a":[{"b":1} 2]})", {"a", 0, "b"},
                       kParseErrorInvalidChar);
  TestGetOnDemandError(R"({"a":[1,]})", {"a", 0}, kParseErrorInvalidChar);
  TestGetOnDemandError(R"({"a":{"b":1,}})", {"a", "b"}, kParseErrorInvalidChar);
  TestGetOnDemandError(R"({"a":1,})", {"a"}, kParseErrorInvalidChar);
}

TEST(GetOnDemand, FastModeDoesNotValidateUnvisitedSuffix) {
  TestGetOnDemand(R"({"a":1,"bad":"\q"})", {"a"}, "1");
  TestGetOnDemand(R"({"a":1} garbage)", {"a"}, "1");
  TestGetOnDemand(R"({"a":[1,2] 3})", {"a", 0}, "1");
}

TEST(GetOnDemand, FullValidationModeRejectsUnvisitedSuffix) {
  StringView target;
  auto escaped = GetOnDemand<ParseFlags::kParseValidateOnDemandFull>(
      R"({"a":1,"bad":"\q"})", JsonPointer{"a"}, target);
  EXPECT_EQ(escaped.Error(), kParseErrorEscapedFormat);
  EXPECT_EQ(target, "");

  auto infinity = GetOnDemand<ParseFlags::kParseValidateOnDemandFull>(
      R"({"a":1,"bad":1e309})", JsonPointer{"a"}, target);
  EXPECT_EQ(infinity.Error(), kParseErrorInfinity);
  EXPECT_EQ(target, "");

  auto trailing_match = GetOnDemand<ParseFlags::kParseValidateOnDemandFull>(
      R"({"a":1} garbage)", JsonPointer{"a"}, target);
  EXPECT_EQ(trailing_match.Error(), kParseErrorInvalidChar);
  EXPECT_EQ(target, "");

  auto trailing_no_match = GetOnDemand<ParseFlags::kParseValidateOnDemandFull>(
      R"({"a":1} garbage)", JsonPointer{"missing"}, target);
  EXPECT_EQ(trailing_no_match.Error(), kParseErrorInvalidChar);
  EXPECT_EQ(target, "");
}
}  // namespace
