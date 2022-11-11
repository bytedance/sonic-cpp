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

#include "sonic/internal/quote.h"

#include <gtest/gtest.h>

#include <cstring>
#include <iostream>
#include <string>
#include <vector>

namespace {
using namespace sonic_json;
using namespace sonic_json::internal;

typedef struct quoteTests {
  std::string input;
  std::string expect;
} quoteTests;

void TestQuote(const std::string& input, const std::string& expect) {
  size_t n = input.size();
  auto buf = std::unique_ptr<char[]>(new char[(n + 2) * 6 + 32]);
  char* end = Quote(input.data(), n, buf.get());
  *end = '\0';
  EXPECT_STREQ(buf.get(), expect.data());
}

TEST(Quote, Normal) {
  std::vector<quoteTests> tests = {
      {"", "\"\""},
      {"a", "\"a\""},
      {"\"", "\"\\\"\""},
      {"\\", "\"\\\\\""},
      {
          "\u666fhello\b\f\n\r\t\\\"world",
          R"("景hello\b\f\n\r\t\\\"world")",
      },
      {
          "<a href=\"http://twitter.com/download/iphone\" "
          "rel=\"nofollow\">Twitter for iPhone</a>",
          R"("<a href=\"http://twitter.com/download/iphone\" rel=\"nofollow\">Twitter for iPhone</a>")",
      }};
  for (const auto& t : tests) {
    TestQuote(t.input, t.expect);
  }
}

TEST(Quote, DiffSize) {
  for (size_t i = 0; i < 300; i++) {
    auto input = std::string(i, 'x');
    std::string expect = "\"" + std::string(i, 'x') + "\"";
    TestQuote(input, expect);
  }
  for (size_t i = 0; i < 300; i++) {
    auto input = std::string(i, '\\');
    std::string expect = "\"" + std::string(i * 2, '\\') + "\"";
    TestQuote(input, expect);
  }
}

}  // namespace
