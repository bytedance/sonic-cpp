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

#include <string>

#include "gtest/gtest.h"
#include "sonic/dom/dynamicnode.h"

namespace {
using StringView = sonic_json::StringView;
enum class OpType { EQ, NE, LT, LE, GT, GE };

template <OpType T>
bool string_view_cmp(const char* s1, const char* s2) {
  StringView sv1(s1);
  StringView sv2(s2);
  switch (T) {
    case OpType::EQ:
      return sv1 == sv2;
    case OpType::NE:
      return sv1 != sv2;
    case OpType::LT:
      return sv1 < sv2;
    case OpType::LE:
      return sv1 <= sv2;
    case OpType::GT:
      return sv1 > sv2;
    case OpType::GE:
      return sv1 >= sv2;
    default:
      break;
  }
  return false;
}

TEST(StringView, Basic) {
  {
    StringView sv;
    EXPECT_EQ(sv.data(), nullptr);
    EXPECT_EQ(sv.size(), 0);
  }
  {
    const char* s = "Hello, World!";
    StringView sv(s);
    EXPECT_EQ(sv.data(), s);
    EXPECT_EQ(sv.size(), std::strlen(s));
  }
  {
    const char* s = "Hello, World!";
    StringView sv(s, std::strlen(s));
    EXPECT_EQ(sv.data(), s);
    EXPECT_EQ(sv.size(), std::strlen(s));
  }
  {
    std::string s = "Hello, World!";
    StringView sv(s);
    EXPECT_EQ(sv.data(), s.data());
    EXPECT_EQ(sv.size(), s.size());
  }
  {
    StringView s("Hello, World!");
    StringView sv(s);
    EXPECT_EQ(sv.data(), s.data());
    EXPECT_EQ(sv.size(), s.size());
  }

  {  // operator==
    EXPECT_TRUE(string_view_cmp<OpType::EQ>("Hello", "Hello"));
    EXPECT_FALSE(string_view_cmp<OpType::EQ>("Hell", "Hello"));
    EXPECT_FALSE(string_view_cmp<OpType::EQ>("Hello", "Hell"));
  }
  {  // operator!=
    EXPECT_FALSE(string_view_cmp<OpType::NE>("Hello", "Hello"));
    EXPECT_TRUE(string_view_cmp<OpType::NE>("Hell", "Hello"));
    EXPECT_TRUE(string_view_cmp<OpType::NE>("Hello", "Hell"));
  }
  {  // operator<
    EXPECT_FALSE(string_view_cmp<OpType::LT>("Hello", "Hello"));
    EXPECT_FALSE(string_view_cmp<OpType::LT>("Hello", "Hell"));
    EXPECT_TRUE(string_view_cmp<OpType::LT>("Hell", "Hello"));
    EXPECT_TRUE(string_view_cmp<OpType::LT>("Hello", "hello"));
    EXPECT_FALSE(string_view_cmp<OpType::LT>("hello", "Hello"));
  }
  {  // operator<=
    EXPECT_TRUE(string_view_cmp<OpType::LE>("Hello", "Hello"));
    EXPECT_FALSE(string_view_cmp<OpType::LE>("Hello", "Hell"));
    EXPECT_TRUE(string_view_cmp<OpType::LE>("Hell", "Hello"));
    EXPECT_TRUE(string_view_cmp<OpType::LE>("Hello", "hello"));
    EXPECT_FALSE(string_view_cmp<OpType::LE>("hello", "Hello"));
  }
  {  // operator>
    EXPECT_FALSE(string_view_cmp<OpType::GT>("Hello", "Hello"));
    EXPECT_TRUE(string_view_cmp<OpType::GT>("Hello", "Hell"));
    EXPECT_FALSE(string_view_cmp<OpType::GT>("Hell", "Hello"));
    EXPECT_FALSE(string_view_cmp<OpType::GT>("Hello", "hello"));
    EXPECT_TRUE(string_view_cmp<OpType::GT>("hello", "Hello"));
  }
  {  // operator>=
    EXPECT_TRUE(string_view_cmp<OpType::GE>("Hello", "Hello"));
    EXPECT_TRUE(string_view_cmp<OpType::GE>("Hello", "Hell"));
    EXPECT_FALSE(string_view_cmp<OpType::GE>("Hell", "Hello"));
    EXPECT_FALSE(string_view_cmp<OpType::GE>("Hello", "hello"));
    EXPECT_TRUE(string_view_cmp<OpType::GE>("hello", "Hello"));
  }
}

}  // namespace
