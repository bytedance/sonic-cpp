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

#include "gtest/gtest.h"
#include "sonic/sonic.h"

TEST(ParseSchemaSelection, RootProjectionAndReuse) {
  auto keep = [](sonic_json::StringView k) {
    return k == "keep" || k == "also" ||
           (k.size() >= 9 && k.substr(0, 9) == "bid_type_");
  };
  const char* inputs[] = {
      R"({"drop":42,"keep":{"x":[1,true]},"bid_type_7":3})",
      R"({"keep":1,"keep":2,"also":3})",
      R"({"drop":{"keep":42}})", R"({"keep":null})",
      R"([{"drop":1}])", R"({"drop":1,"ke\u0065p":2})"};
  const char* expected[] = {
      R"({"keep":{"x":[1,true]},"bid_type_7":3})",
      R"({"keep":1,"keep":2,"also":3})", R"({})",
      R"({"keep":null})", R"([{"drop":1}])", R"({"keep":2})"};
  sonic_json::Document d;
  for (size_t i = 0; i < 6; ++i) {
    d.ParseSchema(inputs[i], keep);
    ASSERT_FALSE(d.HasParseError());
    EXPECT_EQ(d.Dump(), expected[i]);
  }
}

TEST(ParseSchemaSelection, ExistingLengthOverloads) {
  sonic_json::Document d;
  const char* json = R"({"keep":1})";
  const char* schema = R"({"keep":null})";
  d.Parse(schema);
  d.ParseSchema(json, 10);
  ASSERT_FALSE(d.HasParseError());
  EXPECT_EQ(d.Dump(), json);
  d.Parse(schema);
  d.ParseSchema<ParseFlags::kParseDefault>(json, 10u);
  ASSERT_FALSE(d.HasParseError());
  EXPECT_EQ(d.Dump(), json);
  d.Parse(schema);
  d.ParseSchema(json, size_t(10));
  ASSERT_FALSE(d.HasParseError());
  EXPECT_EQ(d.Dump(), json);
}
