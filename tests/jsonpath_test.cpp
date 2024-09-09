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

#define SONIC_SPARK_FORMAT

#include "sonic/sonic.h"

#include <gtest/gtest.h>

namespace {

using namespace sonic_json;

void TestOk(const std::string json, const std::string path,
            const std::string expect) {
  ASSERT_EQ(std::get<0>(GetByJsonPath(json, path)), expect)
      << "json: " << json << ", path: " << path;
}

void TestFail(const std::string json, const std::string path) {
  ASSERT_NE(std::get<1>(GetByJsonPath(json, path)), kErrorNone)
      << "json: " << json << ", path: " << path;
}

TEST(JsonPath, RootIdentifier) {
  TestOk("[\"[\\\",\"]", "$", "[\"[\\\",\"]");
  TestOk(" null ", "$", "null");

  // number
  TestOk("123  ", "$", "123");
  TestOk("100000000.0", "$", "1.0E8");

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

  // invalid json
  TestFail("123x  ", "$");
  TestFail(" nullx ", "$");
  TestFail(" [} ", "$");
  TestFail(R"( {"a:null} )", "$");
  TestFail(R"( [[], {[]}, []] )", "$");
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
  TestOk(json, "$[-7]", "0");
  TestOk(json, "$[1]", "1.23");
  TestOk(json, "$[1]", "1.23");
  TestOk(json, "$[2]", "4.0E56");
  TestOk(json, "$[3]", "null");
  TestOk(json, "$[4]", "true");
  TestOk(json, "$[5]", "{}");
  TestOk(json, "$[6]", "[]");
  TestOk(json, "$[-1]", "[]");

  TestOk("[1,2]", "$[1]", "2");

  TestFail(json, "$.a");
  TestFail(json, "$[7]");
  TestFail(json, "$[-8]");
  TestFail(json, "$[5].a");
  TestFail(json, "$[6][0]");
}

TEST(JsonPath, WildCard) {
  auto json = R"([
        0,
        [1,2,3],
        {"a":1,"b":[1,2,3]},
        []
    ])";

  TestOk(json, "$.*", R"([0,[1,2,3],{"a":1,"b":[1,2,3]},[]])");
  TestOk(json, "$[1].*", "[1,2,3]");
  TestOk(json, "$[2].*", "[1,[1,2,3]]");
  TestOk(json, "$[2].b.*", "[1,2,3]");
  TestOk(json, "$[3].*", "null");

  TestFail(json, "$[0].*");
  TestFail(json, "$[5].a");
  TestFail(json, "$[6][0].*");
}

TEST(JsonPath, WildCardMany) {
  auto json = R"([
        [0],
        [1,2,3],
        [{"a":1,"b":[1,2,3]}],
        []
    ])";

  TestOk(json, "$.*.*", R"([0,1,2,3,{"a":1,"b":[1,2,3]}])");
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

TEST(JSONPath, BadCases) {
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

TEST(JsonPath, InvalidJsonPath) {
  auto json = R"({})";

  TestFail(json, "$[01]");
  TestFail(json, "$[-01]");
  TestFail(json, "$[-0");
  TestFail(json, "$[18446744073709551616]");
  TestFail(json, "$[]");
}
}  // namespace
