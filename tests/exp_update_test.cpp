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

#include <vector>

#include "sonic/experiment/lazy_update.h"

namespace {

TEST(UpdateLazy, Basic) {
  struct UpdateJsonTest {
    std::string target;
    std::string source;
    std::string updated;
  };
  std::vector<UpdateJsonTest> tests = {
      {
          .target = "null",
          .source = "123",
          .updated = "123",
      },
      {
          .target = "[]",
          .source = R"({"a":1})",
          .updated = R"({"a":1})",
      },
      {
          .target = "[1,2,3]",
          .source = R"([4,5,6])",
          .updated = R"([4,5,6])",
      },
      {
          .target = R"({"a":2})",
          .source = R"({"a":1})",
          .updated = R"({"a":1})",
      },
      {
          .target = R"({"a":{"b":0}})",
          .source = R"({"a":{"b":1}})",
          .updated = R"({"a":{"b":1}})",
      },
      {
          .target = R"({"a":{"b":0}})",
          .source = R"({"a":{"c":1}})",
          .updated = R"({"a":{"b":0,"c":1}})",
      },
      {
          .target = R"({"a":{"b":0,"c":0}})",
          .source = R"({"a":{"c":1}})",
          .updated = R"({"a":{"b":0,"c":1}})",
      },
      {
          .target = R"({"a":{"b":0,"c":0,"d":0}})",
          .source = R"({"a":{"c":1}})",
          .updated = R"({"a":{"b":0,"c":1,"d":0}})",
      },
      {
          .target = R"({"b":0,"c":0,"d":0})",
          .source = R"({"c":1})",
          .updated = R"({"b":0,"c":1,"d":0})",
      },
      {
          .target = R"({"a":{"b":0, "c":0,"d":0}})",
          .source = R"({"a":{"d":1}})",
          .updated = R"({"a":{"b":0,"c":0,"d":1}})",
      },
      {
          .target = R"({"a":{"b":0,"c":0, "d":0}})",
          .source = R"({"a":{"b":1,"e":1}})",
          .updated = R"({"a":{"b":1,"c":0,"d":0,"e":1}})",
      },
      {
          .target = R"({"d":1})",
          .source = R"({"b":0, "c":0,"d":0})",
          .updated = R"({"d":0,"b":0,"c":0})",
      },
      {
          .target = R"({"a":{"d":1}})",
          .source = R"({"a":{"b":0, "c":0,"d":0}})",
          .updated = R"({"a":{"d":0,"b":0,"c":0}})",
      },
      {
          .target = R"({"a":{"b":1,"e":1}})",
          .source = R"({"a":{"b":0,"c":0, "d":0}})",
          .updated = R"({"a":{"b":0,"e":1,"c":0,"d":0}})",
      },

      {
          .target = R"({"a":{"b":0,"x":0}})",
          .source = R"({"a":{"c":1,"b":1}})",
          .updated = R"({"a":{"b":1,"x":0,"c":1}})",
      },
      {
          .target = R"({"a":{"b":0,"x":0}})",
          .source = R"({"a":{"c":{"d":"xxxxxx"},"b":1}})",
          .updated = R"({"a":{"b":1,"x":0,"c":{"d":"xxxxxx"}}})",
      },
  };

  for (const auto &t : tests) {
    auto ret = sonic_json::UpdateLazy(t.target, t.source);
    EXPECT_STREQ(ret.c_str(), t.updated.c_str());
  }
}

}  // namespace