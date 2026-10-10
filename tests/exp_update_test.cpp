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
          "null",
          "123",
          "123",
      },
      {
          "[]",
          R"({"a":1})",
          R"({"a":1})",
      },
      {
          "[1,2,3]",
          R"([4,5,6])",
          R"([4,5,6])",
      },
      {
          R"({"a":2})",
          R"({"a":1})",
          R"({"a":1})",
      },
      {
          R"({"a":{"b":0}})",
          R"({"a":{"b":1}})",
          R"({"a":{"b":1}})",
      },
      {
          R"({"a":{"b":0}})",
          R"({"a":{"c":1}})",
          R"({"a":{"b":0,"c":1}})",
      },
      {
          R"({"a":{"b":0,"c":0}})",
          R"({"a":{"c":1}})",
          R"({"a":{"b":0,"c":1}})",
      },
      {
          R"({"a":{"b":0,"c":0,"d":0}})",
          R"({"a":{"c":1}})",
          R"({"a":{"b":0,"c":1,"d":0}})",
      },
      {
          R"({"b":0,"c":0,"d":0})",
          R"({"c":1})",
          R"({"b":0,"c":1,"d":0})",
      },
      {
          R"({"a":{"b":0, "c":0,"d":0}})",
          R"({"a":{"d":1}})",
          R"({"a":{"b":0,"c":0,"d":1}})",
      },
      {
          R"({"a":{"b":0,"c":0, "d":0}})",
          R"({"a":{"b":1,"e":1}})",
          R"({"a":{"b":1,"c":0,"d":0,"e":1}})",
      },
      {
          R"({"d":1})",
          R"({"b":0, "c":0,"d":0})",
          R"({"d":0,"b":0,"c":0})",
      },
      {
          R"({"a":{"d":1}})",
          R"({"a":{"b":0, "c":0,"d":0}})",
          R"({"a":{"d":0,"b":0,"c":0}})",
      },
      {
          R"({"a":{"b":1,"e":1}})",
          R"({"a":{"b":0,"c":0, "d":0}})",
          R"({"a":{"b":0,"e":1,"c":0,"d":0}})",
      },

      {
          R"({"a":{"b":0,"x":0}})",
          R"({"a":{"c":1,"b":1}})",
          R"({"a":{"b":1,"x":0,"c":1}})",
      },
      {
          R"({"a":{"b":0,"x":0}})",
          R"({"a":{"c":{"d":"xxxxxx"},"b":1}})",
          R"({"a":{"b":1,"x":0,"c":{"d":"xxxxxx"}}})",
      },
  };

  for (const auto &t : tests) {
    auto ret =
        sonic_json::UpdateLazy<ParseFlags::kParseDefault>(t.target, t.source);
    EXPECT_STREQ(ret.c_str(), t.updated.c_str());
  }
}

TEST(UpdateLazy, InvalidJson) {
  // invalid source -> keep target (when target parses ok)
  {
    std::string target = R"({"a":1})";
    std::string source = R"({"a":)";  // invalid json
    auto ret =
        sonic_json::UpdateLazy<ParseFlags::kParseDefault>(target, source);
    EXPECT_STREQ(ret.c_str(), target.c_str());
  }

  // invalid target -> return source (when source parses ok)
  {
    std::string target = R"({"a":)";  // invalid json
    std::string source = R"({"b":2})";
    auto ret =
        sonic_json::UpdateLazy<ParseFlags::kParseDefault>(target, source);
    EXPECT_STREQ(ret.c_str(), source.c_str());
  }

  // both invalid -> return empty object
  {
    std::string target = R"({"a":)";
    std::string source = R"({"b":)";
    auto ret =
        sonic_json::UpdateLazy<ParseFlags::kParseDefault>(target, source);
    EXPECT_STREQ(ret.c_str(), "{}");
  }
}

TEST(UpdateLazy, NestedInvalidTargetPropagates) {
  // Nested invalid target merged with nested valid source:
  // the error from the target-side lazy parse must be propagated and
  // the update must fail ("{}"); it must NOT be silently overwritten
  // by the source's successful parse.
  {
    std::string target = R"({"a":{"foo":}})";  // nested {"foo":} is invalid
    std::string source = R"({"a":{"bar":5}})";
    auto ret =
        sonic_json::UpdateLazy<ParseFlags::kParseDefault>(target, source);
    EXPECT_STREQ(ret.c_str(), "{}")
        << "invalid nested target must propagate as update failure";
  }
  {
    std::string target = R"({"a":{"foo": @}})";  // invalid token inside nested
    std::string source = R"({"a":{"bar":5}})";
    auto ret =
        sonic_json::UpdateLazy<ParseFlags::kParseDefault>(target, source);
    EXPECT_STREQ(ret.c_str(), "{}")
        << "invalid nested target must propagate as update failure";
  }
}

}  // namespace
