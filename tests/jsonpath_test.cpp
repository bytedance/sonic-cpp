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

#include <gtest/gtest.h>

#include <cstdio>
#include <cstdlib>
#include <limits>

#if !defined(_WIN32)
#include <signal.h>
#include <unistd.h>
#endif

#include "sonic/sonic.h"

namespace {

using namespace sonic_json;

#define TestOk(json, path, expect)                                         \
  do {                                                                     \
    auto got = GetByJsonPathOnDemand<kSerializeJavaStyleFlag>(json, path); \
    EXPECT_EQ(std::get<0>(got), expect)                                    \
        << "json: " << json << ", path: " << path                          \
        << ", error: " << ErrorMsg(std::get<1>(got));                      \
  } while (0)

#define TestOkWithErr(json, path, expect, expect_err)                      \
  do {                                                                     \
    auto got = GetByJsonPathOnDemand<kSerializeJavaStyleFlag>(json, path); \
    EXPECT_EQ(std::get<0>(got), expect)                                    \
        << "json: " << json << ", path: " << path                          \
        << ", error: " << ErrorMsg(std::get<1>(got));                      \
    EXPECT_EQ(std::get<1>(got), (expect_err))                              \
        << "json: " << json << ", path: " << path                          \
        << ", got: " << ErrorMsg(std::get<1>(got));                        \
  } while (0)

#define TestNoMatch(json, path)                                            \
  do {                                                                     \
    auto got = GetByJsonPathOnDemand<kSerializeJavaStyleFlag>(json, path); \
    EXPECT_EQ(std::get<0>(got), "")                                        \
        << "json: " << json << ", path: " << path                          \
        << ", error: " << ErrorMsg(std::get<1>(got));                      \
    EXPECT_EQ(std::get<1>(got), kErrorNoneNoMatch)                         \
        << "json: " << json << ", path: " << path                          \
        << ", got: " << ErrorMsg(std::get<1>(got));                        \
  } while (0)

#define TestUnsupportedPath(json, path) \
  TestOkWithErr(json, path, "", kUnsupportedJsonPath)

void TestFail(const std::string json, const std::string path) {
  auto result = GetByJsonPathOnDemand<kSerializeJavaStyleFlag>(json, path);
  ASSERT_NE(std::get<1>(result), kErrorNone)
      << "Expected error for json: " << json << ", path: " << path
      << ", but actual: " << std::get<0>(result);
}

void ValidBatchOK(const std::string json,
                  const std::vector<std::string>& paths) {
  std::vector<StringView> jsonpaths;
  for (const auto& path : paths) {
    jsonpaths.emplace_back(path);
  }
  auto batch = GetByJsonPaths(json, jsonpaths);

  ASSERT_EQ(std::get<1>(batch), kErrorNone)
      << "json: " << json << ", parse failed.";
  auto results = std::get<0>(batch);
  for (size_t i = 0; i < paths.size(); ++i) {
    auto result = GetByJsonPath(json, paths[i]);
    ASSERT_EQ(std::get<0>(results[i]), std::get<0>(result))
        << "json: " << json << ", path: " << paths[i];
    ASSERT_EQ(std::get<1>(results[i]), std::get<1>(result))
        << "json: " << json << ", path: " << paths[i];
  }
}

TEST(JsonPath, RootIdentifier) {
  TestOk("[\"[\\\",\"]", "$", "[\"[\\\",\"]");
  TestOk(" null ", "$", "null");
  TestOk("true", "$", "true");
  TestOk("false", "$", "false");
  TestOk("false ", "$", "false");
  TestOk(" false ", "$", "false");
  TestOk("true ", "$", "true");
  TestOk("true", "$", "true");
  TestOk(" true ", "$", "true");
  TestOk(" true\n", "$", "true");

  // string
  TestOk("\"123\"  ", "$", "123");
  TestOk("\"😊\"  ", "$", "😊");
  TestOk("\"null\"", "$", "null");

  // container
  TestOk(" [] ", "$", "[]");
  TestOk(" [\"😊\"] ", "$", "[\"\\uD83D\\uDE0A\"]");
  TestOk(" {\"a\":  \"😊💎\"} ", "$", "{\"a\":\"\\uD83D\\uDE0A\\uD83D\\uDC8E\"}");
  TestOk(" {} ", "$", "{}");
  TestOk(R"( {"a":null} )", "$", R"({"a":null})");
  TestOk(R"( [[], {}, []] )", "$", R"([[],{},[]])");

  // has unescaped chars
  TestOk("\"\t\n\"", "$", "\t\n");
  TestOk("\"\\t\\n\"", "$", "\t\n");
  TestOk("[\"\t\n\"]", "$", "[\"\\t\\n\"]");
  TestOk("[\"\\t\\n\"]", "$", "[\"\\t\\n\"]");

  // invalid json
  TestFail("123x  ", "$");
  TestFail(" nullx ", "$");
  TestFail("truex", "$");
  TestFail("xtrue", "$");
  TestFail("falsex", "$");
  TestFail("xfalse", "$");
  TestFail(" [} ", "$");
  TestFail(R"( {"a:null} )", "$");
  TestFail(R"( [[], {[]}, []] )", "$");
}

TEST(JsonPath, UnicodeTest) {
  // unicode control character in value
  TestOk(
      R"( {"error_msg":"X.G1x: Expected value at line 1 column 1 path $","origin_string":"=\u0007mEs\u000FA���%חk�9ded06cd author: John Doe"})",
      "$.origin_string", "=\u0007mEs\u000FA���%חk�9ded06cd author: John Doe");

  // unicode control character within quotes
  TestOk(
      R"( {"category":{"error_msg":"X.G1x: Expected value at line 1 column 1 path $","origin_string":"=\u0007mEs\u000fA���%חk�9ded06cd author: John Doe"},"server_uuid":"1234"} )",
      "$.category",
      "{\"error_msg\":\"X.G1x: Expected value at line 1 column 1 path "
      "$\",\"origin_string\":\"=\\u0007mEs\\u000FA���%חk�9ded06cd author: John "
      "Doe\"}");
}

TEST(Root, Number) {
  TestOk("123  ", "$", "123");
  TestOk("1.23  ", "$", "1.23");
  TestOk("1.0E7", "$", "1.0E7");
  TestOk("9999999.999999998", "$", "9999999.999999998");
  TestOk("0.001", "$", "0.001");
  TestOk("0.0001", "$", "1.0E-4");
  TestOk("0.0009999999999999998", "$", "9.999999999999998E-4");

  // all interges will parse as Raw
  TestOk("5555555555555555555555555555", "$", "5555555555555555555555555555");
  TestOk("[5555555555555555555555555555]", "$",
         "[5555555555555555555555555555]");
  TestOk("[0,1,-0,-1,-123,-5555555555555555555555555555]", "$",
         "[0,1,-0,-1,-123,-5555555555555555555555555555]");
  TestOk(R"({"a": 5555555555555555555555555555 })", "$",
         R"({"a":5555555555555555555555555555})");
}

TEST(JsonPath, IndexSelector) {
  auto json = R"([
        0,
        1.23,
        4e56,
        "null",
        true,
        {},
        []
    ])";

  TestOk(json, "$[0]", "0");
  TestOk(json, "$[1]", "1.23");
  TestOk(json, "$[1]", "1.23");
  TestOk(json, "$[2]", "4.0E56");
  TestOk(json, "$[3]", "null");
  TestOk(json, "$[4]", "true");
  TestOk(json, "$[5]", "{}");
  TestOk(json, "$[6]", "[]");

  TestOk("[1,2]", "$[1]", "2");

  TestFail(json, "$.a");
  TestFail(json, "$[7]");
  TestFail(json, "$[-8]");
  TestFail(json, "$[5].a");
  TestFail(json, "$[6][0]");
}

TEST(JsonPathWildcard, Basic) {
  auto json = R"([
        0,
        [1,2,3],
        {"a":1,"b":[1,2,3]},
        []
    ])";

  TestOk(json, "$.*", R"([0,[1,2,3],{"a":1,"b":[1,2,3]},[]])");
  TestOk(json, "$[1].*", "[1,2,3]");
  // spark cannot handle Object[*]
  // TestOk(json, "$[2].*", "[1,[1,2,3]]");
  TestOk(json, "$[2].b.*", "[1,2,3]");
  // spark cannot handle Object[*]
  // TestOk(json, "$[3].*", "null");

  // ignore when not found
  TestOk(R"([{"a":123}, {}])", "$[*].a", "123");
  TestOk(R"([[123, 456], []])", "$[*][1]", "456");

  // ignore when encounter the mismatched type
  TestOk(R"([{"a":123}, null])", "$[*].a", "123");
  TestOk(R"([[123, 456], [], null])", "$[*][1]", "456");
}

TEST(JsonPathWildcard, SingleStringIsUnquotedOnDemand) {
  TestOk(R"([{"a":"x"}, {}])", "$[*].a", "x");
}

TEST(JsonPathWildcard, Primitive) {
  TestOk("1", "$[*]", "");
  TestOk("null", "$[*]", "");
  TestOk("true", "$[*]", "");
  TestOk("false", "$[*]", "");
  TestOk("\"\"", "$[*]", "");
  TestOk("\"hello\"", "$[*]", "");
}

TEST(JsonPathWildcard, PrimitiveNoMatchErrorCode) {
  TestNoMatch("1", "$[*]");
  TestNoMatch("null", "$[*]");
  TestNoMatch("true", "$[*]");
  TestNoMatch("false", "$[*]");
}

TEST(JsonPath, WildcardBatch) {
  auto json = R"([
        0,
        [1,2,3],
        {"a":1,"b":[1,2,3]},
        []
    ])";
  std::vector<std::string> paths = {"$.*",    "$[1].*", "$[2].*", "$[2].b.*",
                                    "$[3].*", "$[0].*", "$[5].a", "$[6][0].*"};
  ValidBatchOK(json, paths);
}

TEST(JsonPath, BadBatch) {
  auto json = R"([
        0,
        [1,2,3],
        {"a":1,"b":[1,2,3]},
        [] bad balaba)";
  std::vector<std::string> paths = {"$.*", "$[1].*", "$[2].*", "$[2].b.*"};
  std::vector<StringView> jsonpaths;
  for (const auto& path : paths) {
    jsonpaths.emplace_back(path);
  }
  ASSERT_NE(std::get<1>(GetByJsonPaths(json, jsonpaths)), kErrorNone);
}

TEST(JsonPath, WildcardMany) {
  auto json = R"([
        [0],
        [1,2,3],
        [{"a":1,"b":[1,2,3]}],
        []
    ])";

  TestOk(json, "$.*.*", R"([0,1,2,3,{"a":1,"b":[1,2,3]}])");
}

TEST(JsonPath, WildcardArray) {
  auto json = R"({
      "a": {
        "b": [
          [
            [
              {
                "c": 1
              },
              {
                "c": 2
              }
            ]
          ]
        ]
      }
    })";
  auto path = "$.a.b[0][0][*].c";
  TestOk(json, path, "[1,2]");
  json = R"([{
    "a": 1,
    "b": 2
    }, {
    "a": 3,
    "b": 4
    }])";
  path = "$[*].a";
  TestOk(json, path, "[1,3]");
  json = R"([{
    "TaskKey": 3010,
    "Status": 3,
    "OutTaskType": 5,
    "OutTaskContent": {
            "OutTaskId": "8121973866456483870",
            "Source": 5133,
            "productId": "1738761753995320",
            "productSnapshotId": "0",
            "extra": "{"sku_name":"Caribbean Water Park Student Ticket (Jul 22-Jul 24}}"}"
    },
    {
            "TaskKey": 3001,
            "Status": 3,
            "OutTaskType": 1,
            "OutTaskContent": {
                    "ObjectID": "1738762988118075",
                    "AuditID": "NT011658213969796EDed000",
                    "Extra": "{"
                    namek_task_id ":"
                    8121974700481349645 "}}"
        }])";
  path = "$[*].TaskKey";
  TestFail(json, path);
}

TEST(JsonPath, KeySelector) {
  auto json = R"({
        "a": 1,
        "b": 2,
        "c": 3,
        "d": {
            "d1": 4,
            "d2": [
                0,
                1,
                {
                    "d21": 5
                },
                [ true],
                [],
                [[null]]
            ]
        },
        "e": "null",
        "f\"": "f key\""
    })";
  TestOk(json, "$",
         "{\"a\":1,\"b\":2,\"c\":3,\"d\":{\"d1\":4,\"d2\":[0,1,{\"d21\":5},["
         "true],[],[[null]]]},\"e\":\"null\",\"f\\\"\":\"f key\\\"\"}");
  TestOk(json, "$.a", "1");
  TestOk(json, "$.b", "2");
  TestOk(json, "$['b']", "2");
  //   TestOk(json, "$[b]", "2");
  TestOk(json, "$[\"b\"]", "2");
  TestOk(json, "$.d", "{\"d1\":4,\"d2\":[0,1,{\"d21\":5},[true],[],[[null]]]}");

  TestFail(json, "$[1]");
  TestFail(json, "$.a.b");
  TestFail(json, "$.a[1]");

  TestOk(json, "$.d.d2[0]", "0");
  TestOk(json, "$.d.d2[1]", "1");
  TestOk(json, "$.d.d2[2]", "{\"d21\":5}");
  TestOk(json, "$.d.d2[3]", "[true]");
  TestOk(json, "$.d.d2[3][0]", "true");
  TestOk(json, "$.d.d2[4]", "[]");
  TestOk(json, "$.d.d2[5][0][0]", "null");
  TestFail(json, "$.d.d2[4].a");
  TestFail(json, "$.d.d2[5][0][0][0]");
}

TEST(JsonPath, EscapedKeySelector) {
  auto json = R"({
        "a\\": 1,
        "b\"": 2,
        "bA": 3,
        "b.9": 4,
        "b@": 5
    })";
  TestOk(json, R"($["a\\"])", "1");
  TestOk(json, R"($['a\'])", "1");
  TestOk(json, R"($["b\""])", "2");
  TestOk(json, R"($["b\u0041"])", "3");
  TestOk(json, "$['b.9']", "4");
  TestOk(json, "$['b@']", "5");
}

TEST(JsonPath, BadCases) {
  auto json = R"({
    "a": {
      "b": {
        "c": "value1",
        "d": "value2"
      }
    },
    "e.f": "value3",
    "g.h.i": "value4"
  })";
  TestOk(json, "$.a.b.c", "value1");

  TestOk(R"({"root": [{"a":null},{"a":"foo"},{"a":"bar"}]})", "$.root[*].a",
         R"(["foo","bar"])");
}

TEST(JsonPath, KeyNullElementPair) {
  auto json =
      R"( { "last_name": "Kim", "first_name":"Sam", "gender": false, "income": null, "age": 38} )";
  TestOk(json, "$.income", "");
}

TEST(JsonPath, KeyIntoStringValue) {
  auto json = R"(   {"name": {"name" : "bytedance"}}  )";
  TestOk(json, "$.name.name.name.name.name", "");
}

TEST(JsonPath, BeforeNan) {
  auto json =
      R"( {"name":"xiaoxiao", "gender": false, "height": Nan, "passed": true} )";
  TestOk(json, "$.name", "xiaoxiao");
  TestOk(json, "$.gender", "false");
  TestOk(json, "$.height", "");
  TestOk(json, "$.passed", "");
}

TEST(JsonPath, BackslashZero) {
  std::string json = "  {\"name\": 321, \"req_id\": \"344  ";
  json.push_back('\0');
  json += " 43321\"} ";
  TestOk(json, "$.req_id",
         std::string("344  ") + std::string{'\0'} + std::string(" 43321"));
  TestOk(json, "$.name", "321");
}

TEST(JsonPath, sparkFeature) {
  auto json =
      R"(  {"price":"129.99","suggested_price":"106.39","sku_name":"Shoe Model 4    825 Mint Green [High Quality Basketball Shoe )";
  TestOk(json, "$.price", "129.99");

  json = R"(
{"key":"fakekey","labels":"ProductMgmt","labelsIterator":"ProductMgmt","labelsSize":1,"name":"Link","setExtra":false,"setKey":true,"setLabels":true,"setName":true,"setType":false,"setValues":true,"type":0,"values":"2mPs6","valuesIterator":"2mPs6","valuesSize":1
)";
  TestOk(json, "$.key", "fakekey");
  TestOk(json, "$.labels", "ProductMgmt");
}

TEST(JsonPath, illegalJson) {
  auto badJson3 =
      R"({
          "creative_setting": {
          "CreativeKeywords": [
          "careful_mom",
          "good_stuff_recommendation\\\\\",
          "life_fun"
          ] }
          })";
  TestOk(badJson3, "$.creative_setting.CreativeKeywords", "");

  // Make sure this is treated as parse error, not a normal "no match".
  auto got = GetByJsonPathOnDemand<kSerializeJavaStyleFlag>(
      badJson3, "$.creative_setting.CreativeKeywords");
  EXPECT_NE(std::get<1>(got), kErrorNone);
  EXPECT_NE(std::get<1>(got), kErrorNoneNoMatch);
}

TEST(JsonPath, InvalidJsonPath) {
  auto json = R"({})";

  TestFail(json, "$[01]");
  TestFail(json, "$[-01]");
  TestFail(json, "$[-0");
  TestFail(json, "$[18446744073709551616]");
  TestFail(json, "$[]");
}

TEST(JsonPath, InvalidJsonPathMore) {
  auto json = R"({})";

  // Invalid token after '$'
  TestUnsupportedPath(json, "$a");
  TestUnsupportedPath(json, "$$");
  TestUnsupportedPath(json, "$ ");

  // Unclosed bracket / quote
  TestUnsupportedPath(json, "$[0");
  TestUnsupportedPath(json, "$[*");
  TestUnsupportedPath(json, R"($["a])");
  TestUnsupportedPath(json, R"($['a])");

  // Unsupported features / expressions
  TestUnsupportedPath(json, "$..a");
  TestUnsupportedPath(json, "$[a]");
  TestUnsupportedPath(json, "$[1:3]");
  TestUnsupportedPath(json, "$[0,1]");
  TestUnsupportedPath(json, "$[?(@.a)]");

  // Empty key
  TestUnsupportedPath(json, "$.");
  TestNoMatch(R"({})", R"($[""])");
  TestNoMatch(R"({})", R"($[''])");

  // Index > INT64_MAX
  TestUnsupportedPath(json, "$[9223372036854775808]");
  TestUnsupportedPath(json, "$[43788737869027501872]");
}

TEST(JsonPath, QuotedNameEscapes) {
  // Double-quoted name supports backslash escapes.
  TestOk(R"({"a\"b":1})", R"($["a\"b"])", "1");
  TestOk(R"({"":1})", R"($[""])", "1");
  TestOk(R"({"":1})", R"($[''])", "1");

  // Invalid escape sequence should fail parsing and be treated as unsupported.
  TestUnsupportedPath(R"({})", R"($["a\x"])");
  TestNoMatch(R"({})", R"($["\\"])");

  // Missing closing bracket after quoted name.
  TestUnsupportedPath(R"({})", R"($["a"])x)");
}

TEST(JsonPath, InternalParserBranches) {
  using sonic_json::internal::JsonPath;
  using sonic_json::internal::paddingJsonPath;

  auto parse_ok = [](const std::string& p) {
    std::string padded = paddingJsonPath(p);
    JsonPath path;
    return path.ParsePadded(StringView(padded.data(), padded.size()), p.size());
  };

  // Valid cases.
  EXPECT_TRUE(parse_ok("$"));
  EXPECT_TRUE(parse_ok("$.a"));
  EXPECT_TRUE(parse_ok("$.*"));
  EXPECT_TRUE(parse_ok("$[*]"));
  EXPECT_TRUE(parse_ok("$[0]"));
  EXPECT_TRUE(parse_ok("$[-1]"));
  // Use "\\u" to keep it as two chars: '\\' + 'u' (avoid universal-char-name).
  EXPECT_TRUE(parse_ok("$[\"\\uD83D\\uDE0A\"]"));

  // Invalid / edge cases.
  EXPECT_FALSE(parse_ok(""));
  EXPECT_FALSE(parse_ok("a"));
  EXPECT_FALSE(parse_ok("$."));
  EXPECT_FALSE(parse_ok("$["));
  EXPECT_FALSE(parse_ok("$[01]"));
  EXPECT_FALSE(parse_ok("$[999999999999999999999999999999999999999999]"));
  EXPECT_FALSE(parse_ok("$..a"));
}

TEST(JsonPath, TruncatedJsonReturnsInvalidChar) {
  // Root parsing path is slightly different from deep-path scanning and may
  // report invalid-char for truncated containers.
  TestOkWithErr("[8", "$", "", kParseErrorInvalidChar);
}

TEST(JsonPath, KeyNumSelector) {
  auto json = R"({
        "1": 1,
        "2": [0,1,2,3],
        "3": {"33": 123}
        })";
  TestOk(json, "$.1", "1");
  TestOk(json, "$.2[2]", "2");
  TestOk(json, "$.3.33", "123");
}

TEST(JsonPath, WildCardSpark) {
  auto json = R"({
      "Person": [
        {
          "name": "a",
          "value": ["9.2", "3.0"]
        },
        {
          "name": "b",
          "value": ["6", "666"]
        }
      ]
    })";
  TestOk(json, "$.Person[*].value[*]", R"([["9.2","3.0"],["6","666"]])");

  json = R"({
      "Person": [
        {
          "name": "a",
          "value": ["9.2"]
        },
        {
          "name": "b",
          "value": ["6"]
        }
      ]
    })";
  TestOk(json, "$.Person[*].value[*]", R"([["9.2"],["6"]])");
}

TEST(JsonPath, DoubleWildcardFlattenArrays) {
  TestOk("[[1,2],[3]]", "$[*][*]", "[1,2,3]");
  TestOk(R"([[{"a":1}],[{"a":2}]])", "$[*][*]", R"([{"a":1},{"a":2}])");
}

TEST(JsonPath, IndexThenWildcard) {
  TestOk("[[1,2],[3,4]]", "$[1][*]", "[3,4]");
  TestOk("[[1,2],[3,4]]", "$[0][*]", "[1,2]");
}
TEST(JsonPath, DoubleEscape) {
  auto json = R"({"output":{"ens_prob":0.004286}})";
  TestOk(json, "$.output.ens_prob", "0.004286");
}
std::vector<int> splitToInts(const std::string& str) {
  std::vector<int> numbers;
  std::string_view sv(str);

  while (!sv.empty()) {
    auto space = sv.find(' ');
    auto token = sv.substr(0, space);

    if (!token.empty()) {
      numbers.push_back(std::stoi(std::string(token)));
    }

    if (space == std::string_view::npos) break;
    sv = sv.substr(space + 1);
  }

  return numbers;
}
TEST(JsonPath, JsonInfiniteLoop) {
#if defined(_WIN32)
  GTEST_SKIP() << "Windows does not support alarm()-based timeout guard.";
#else
  // This case historically could hang; run it in a subprocess with an alarm so
  // the whole test suite won't get stuck.
  ASSERT_EXIT(
      {
        alarm(2);

        const std::string integers(
            "123 34 -28 -65 -99 -23 -103 -87 34 58 32 48 46 55 57 49 53 44 32 "
            "34 "
            "-23 "
            "-127 -109 -27 -91 -121 -23 -123 -73 -27 -88 -127 34 58 32 48 46 "
            "55 "
            "56 "
            "48 56 44 32 34 -24 -121 -86 -27 -118 -88 -26 -116 -95 34 58 32 48 "
            "46 55 "
            "55 56 49 125");
        auto ints = splitToInts(integers);
        std::string json("");
        for (const auto i : ints) {
          json.push_back((char)i);
        }

        auto got = GetByJsonPathOnDemand<
            SerializeFlags::kSerializeUnicodeEscapeUppercase>(
            json, "$.motor_content_boost");
        const auto& out = std::get<0>(got);
        const auto err = std::get<1>(got);
        if (err == kErrorNone) {
          std::fprintf(stderr, "unexpected success, out=%s\n", out.c_str());
          std::_Exit(1);
        }
        if (!out.empty()) {
          std::fprintf(stderr, "expected empty output, err=%d, out=%s\n",
                       static_cast<int>(err), out.c_str());
          std::_Exit(1);
        }
        std::_Exit(0);
      },
      ::testing::ExitedWithCode(0), "");
#endif
}
TEST(JsonPath, JsonInfiniteLoop2) {
  std::string json("[8");
  auto got =
      GetByJsonPathOnDemand<SerializeFlags::kSerializeUnicodeEscapeUppercase>(
          json, "$.motor_content_boost");
  EXPECT_EQ(std::get<1>(got), kParseErrorEof);
  EXPECT_EQ(std::get<0>(got), "");
}

TEST(JsonPath, JsonInfiniteLoop3) {
  std::string json = R"json([{"a":["c"]},)json";
  std::string path = "$[*].a";
  auto got =
      GetByJsonPathOnDemand<SerializeFlags::kSerializeUnicodeEscapeUppercase>(
          json, path);
  EXPECT_EQ(std::get<1>(got), kParseErrorEof);
  EXPECT_EQ(std::get<0>(got), "");
}

TEST(JsonPath, JsonTuple) {
  auto json = R"({a:1, b:2c})";
  auto got =
      GetByJsonPathOnDemand<SerializeFlags::kSerializeUnicodeEscapeUppercase>(
          json, "$.b");
  EXPECT_EQ(std::get<1>(got), kParseErrorUnexpect);
  EXPECT_EQ(std::get<0>(got), "");
}

TEST(JsonPathDom, SingleStringIsUnquoted) {
  auto got = GetByJsonPath(R"({"s":"abc"})", "$.s");
  EXPECT_EQ(std::get<1>(got), kErrorNone);
  EXPECT_EQ(std::get<0>(got), "abc");
}

TEST(JsonPathDom, ConstAtJsonPathIsConstCorrect) {
  Document doc;
  doc.Parse(R"({"s":"abc"})");
  ASSERT_FALSE(doc.HasParseError());

  const Document& cdoc = doc;
  JsonPathResult<const Node> result = cdoc.AtJsonPath("$.s");
  EXPECT_EQ(result.error, kErrorNone);
  ASSERT_EQ(result.nodes.size(), 1U);
  const Node* n = result.nodes[0];
  ASSERT_NE(n, nullptr);
  EXPECT_TRUE(n->IsString());
  EXPECT_EQ(n->GetStringView(), "abc");
}

TEST(JsonPathDom, NullNodeFilteredToEmptyString) {
  auto got = GetByJsonPath(R"({"a":null})", "$.a");
  EXPECT_EQ(std::get<1>(got), kErrorNone);
  EXPECT_EQ(std::get<0>(got), "");
}

TEST(JsonPathDom, MultiNodesSerializeAndFilterNulls) {
  auto got = GetByJsonPath(R"({"a":[null,"x",1]})", "$.a[*]");
  EXPECT_EQ(std::get<1>(got), kErrorNone);
  EXPECT_EQ(std::get<0>(got), R"(["x",1])");
}

TEST(JsonPathDom, NotFoundAndUnsupportedPath) {
  auto not_found = GetByJsonPath(R"({"a":1})", "$.b");
  EXPECT_EQ(std::get<1>(not_found), kNotFoundByJsonPath);
  EXPECT_EQ(std::get<0>(not_found), "");

  auto unsupported = GetByJsonPath(R"({"a":1})", "$..a");
  EXPECT_EQ(std::get<1>(unsupported), kUnsupportedJsonPath);
  EXPECT_EQ(std::get<0>(unsupported), "");
}

TEST(JsonPathDom, NegativeIndexSupportedButOnDemandUnsupported) {
  auto dom_got = GetByJsonPath("[0,1,2]", "$[-1]");
  EXPECT_EQ(std::get<1>(dom_got), kErrorNone);
  EXPECT_EQ(std::get<0>(dom_got), "2");

  auto ondemand_got =
      GetByJsonPathOnDemand<kSerializeJavaStyleFlag>("[0,1,2]", "$[-1]");
  // OnDemand mode does not support negative indexes.
  EXPECT_NE(std::get<1>(ondemand_got), kErrorNone);
  EXPECT_EQ(std::get<0>(ondemand_got), "");
}

TEST(JsonPathDump, SerializeCoversEmptySingleAndMulti) {
  Document doc;
  doc.Parse(R"({"a":null,"s":"abc","arr":[null,"x",1]})");
  ASSERT_FALSE(doc.HasParseError());

  {
    auto result = doc.AtJsonPath("$.a");
    ASSERT_EQ(result.error, kErrorNone);
    auto dumped = sonic_json::internal::Serialize(result);
    EXPECT_EQ(std::get<1>(dumped), kErrorNone);
    EXPECT_EQ(std::get<0>(dumped), "");
  }

  {
    auto result = doc.AtJsonPath("$.s");
    ASSERT_EQ(result.error, kErrorNone);
    auto dumped = sonic_json::internal::Serialize(result);
    EXPECT_EQ(std::get<1>(dumped), kErrorNone);
    EXPECT_EQ(std::get<0>(dumped), "abc");
  }

  {
    auto result = doc.AtJsonPath("$.arr[*]");
    ASSERT_EQ(result.error, kErrorNone);
    auto dumped = sonic_json::internal::Serialize(result);
    EXPECT_EQ(std::get<1>(dumped), kErrorNone);
    EXPECT_EQ(std::get<0>(dumped), R"(["x",1])");
  }
}

TEST(JsonPathDump, SerializeCoversSingleNonStringAndErrorPaths) {
  Document doc;
  doc.Parse(R"({"n":1,"inf":0})");
  ASSERT_FALSE(doc.HasParseError());

  // size == 1 and not string
  {
    auto result = doc.AtJsonPath("$.n");
    ASSERT_EQ(result.error, kErrorNone);
    auto dumped = sonic_json::internal::Serialize(result);
    EXPECT_EQ(std::get<1>(dumped), kErrorNone);
    EXPECT_EQ(std::get<0>(dumped), "1");
  }

  // serialization error path (infinity is not allowed unless kSerializeInfNan)
  doc["inf"].SetDouble(std::numeric_limits<double>::infinity());
  {
    auto result = doc.AtJsonPath("$.inf");
    ASSERT_EQ(result.error, kErrorNone);
    auto dumped = sonic_json::internal::Serialize(result);
    EXPECT_EQ(std::get<1>(dumped), kSerErrorInfinity);
    EXPECT_EQ(std::get<0>(dumped), "");
  }

  // multi-nodes error path
  {
    JsonPathResult<Node> result;
    result.error = kErrorNone;
    result.nodes.push_back(&doc["n"]);
    result.nodes.push_back(&doc["inf"]);

    auto dumped = sonic_json::internal::Serialize(result);
    EXPECT_EQ(std::get<1>(dumped), kSerErrorInfinity);
    EXPECT_EQ(std::get<0>(dumped), "");
  }
}

TEST(JsonPathDump, SerializeMatchesPublicJsonPathForFilteredNullOnly) {
  Document doc;
  doc.Parse(R"({"a":null})");
  ASSERT_FALSE(doc.HasParseError());

  auto result = doc.AtJsonPath("$.a");
  ASSERT_EQ(result.error, kErrorNone);

  auto dumped = sonic_json::internal::Serialize(result);
  auto public_got = GetByJsonPath(R"({"a":null})", "$.a");

  EXPECT_EQ(std::get<1>(dumped), std::get<1>(public_got));
  EXPECT_EQ(std::get<0>(dumped), std::get<0>(public_got));
}

TEST(JsonPathDump, SerializeDoesNotMutateInputResult) {
  Document doc;
  doc.Parse(R"({"arr":[null,"x",1]})");
  ASSERT_FALSE(doc.HasParseError());

  auto result = doc.AtJsonPath("$.arr[*]");
  ASSERT_EQ(result.error, kErrorNone);
  ASSERT_EQ(result.nodes.size(), 3U);
  ASSERT_TRUE(result.nodes[0]->IsNull());

  auto dumped = sonic_json::internal::Serialize(result);
  EXPECT_EQ(std::get<1>(dumped), kErrorNone);
  EXPECT_EQ(std::get<0>(dumped), R"(["x",1])");

  ASSERT_EQ(result.nodes.size(), 3U);
  EXPECT_TRUE(result.nodes[0]->IsNull());
  EXPECT_TRUE(result.nodes[1]->IsString());
}

TEST(JsonPathDump, SerializeAcceptsConstResult) {
  Document doc;
  doc.Parse(R"({"s":"abc"})");
  ASSERT_FALSE(doc.HasParseError());

  const Document& cdoc = doc;
  const auto result = cdoc.AtJsonPath("$.s");
  ASSERT_EQ(result.error, kErrorNone);

  auto dumped = sonic_json::internal::Serialize(result);
  EXPECT_EQ(std::get<1>(dumped), kErrorNone);
  EXPECT_EQ(std::get<0>(dumped), "abc");
}
}  // namespace
