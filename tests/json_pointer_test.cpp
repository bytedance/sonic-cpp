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

#include "sonic/dom/json_pointer.h"

#include <string>

#include "gtest/gtest.h"
#include "sonic/string_view.h"

namespace {

using namespace sonic_json;

template <typename T>
class JsonPointerTest : public testing::Test {
 public:
  using JsonPointerType = T;
};

using JsonPointerTypes =
    testing::Types<GenericJsonPointer<std::string>,
                   GenericJsonPointer<sonic_json::StringView>>;

TYPED_TEST_SUITE(JsonPointerTest, JsonPointerTypes);

TYPED_TEST(JsonPointerTest, NodeConstructor) {
  using JsonPointerType = TypeParam;
  using JPNodeType = typename JsonPointerType::JsonPointerNodeType;

  // string
  { JPNodeType node("hi"); }
  {
    std::string hi = "hi";
    JPNodeType node(hi);
  }
  { JPNodeType node(sonic_json::StringView("hi")); }
  {
    char hi[3] = "hi";
    JPNodeType node(hi);
  }

  // number
#define TEST_INIT_TYPE(type) \
  { JPNodeType node((type)(0)); }
  TEST_INIT_TYPE(int);
  TEST_INIT_TYPE(unsigned int);
  TEST_INIT_TYPE(int64_t);
  TEST_INIT_TYPE(uint64_t);
  TEST_INIT_TYPE(size_t);
  TEST_INIT_TYPE(bool);

  // MUST compile failed code here.
  // TEST_INIT_TYPE(double);
  // TEST_INIT_TYPE(float);
  // TEST_INIT_TYPE(std::nullptr_t);
  // { uint8_t hi[3] = "hi"; JPNodeType node(hi); }
}

TYPED_TEST(JsonPointerTest, Constructor) {
  using JsonPointerType = TypeParam;

  JsonPointerType path_t;
  EXPECT_TRUE(path_t.empty());

  JsonPointerType path{"path0", "path1", 5, "path 2", StringView("path\0A", 6)};
  EXPECT_FALSE(path.empty());
  EXPECT_EQ(path.size(), 5);
  auto itr = path.begin();
  EXPECT_STREQ("path0", itr->GetStr().data());
  EXPECT_EQ(5, itr->GetStr().size());
  itr++;
  EXPECT_STREQ("path1", itr->GetStr().data());
  EXPECT_EQ(5, itr->GetStr().size());
  itr++;
  EXPECT_EQ(5, itr->GetNum());
  itr++;
  EXPECT_STREQ("path 2", itr->GetStr().data());
  EXPECT_EQ(6, itr->GetStr().size());
  itr++;
  EXPECT_EQ(std::string("path\0A", 6),
            std::string(itr->GetStr().data(), itr->GetStr().size()));

  itr++;
  EXPECT_TRUE(itr == path.end());

  {
    JsonPointerType p;
    JsonPointerType p1{};
  }

  { JsonPointerType p(path); }

  { JsonPointerType p(std::move(path)); }

  { JsonPointerType p = path; }

  { JsonPointerType p = std::move(path); }
}

TYPED_TEST(JsonPointerTest, PushAndPop) {
  using JsonPointerType = TypeParam;
  using QueryNode = typename JsonPointerType::JsonPointerNodeType;

  JsonPointerType path;
  for (int i = 0; i < 100; ++i) {
    path /= QueryNode(i);
  }
  EXPECT_EQ(100, path.size());
  path /= JsonPointerType(path);
  EXPECT_EQ(200, path.size());
  for (int i = 0; i < 200; ++i) {
    path.pop_back();
    EXPECT_EQ(200 - 1 - i, path.size());
  }
  EXPECT_TRUE(path.empty());
}

TYPED_TEST(JsonPointerTest, Operator) {
  using JsonPointerType = TypeParam;
  using JsonPointerNode = typename JsonPointerType::JsonPointerNodeType;

  JsonPointerType expect = {"a", 0, "b", 1, "c", 2, "d", "3"};
  {
    JsonPointerType path({"a", 0, "b", 1});
    EXPECT_EQ(path / JsonPointerType({"c", 2, "d", "3"}), expect);
    path /= JsonPointerType({"c", 2, "d", "3"});
    EXPECT_EQ(path, expect);
  }
  {
    JsonPointerType path({"a", 0, "b", 1});
    JsonPointerType path2({"c", 2, "d", "3"});
    EXPECT_EQ(path / path2, expect);
    path /= path2;
    EXPECT_EQ(path, expect);
  }
  {
    JsonPointerType path({"a", 0, "b", 1, "c", 2, "d"});
    EXPECT_EQ(path / JsonPointerNode("3"), expect);
    path /= JsonPointerNode("3");
    EXPECT_EQ(path, expect);
  }
  {
    JsonPointerType path({"a", 0, "b", 1, "c", 2, "d"});
    JsonPointerNode other = "3";
    EXPECT_EQ(path / other, expect);
    path /= other;
    EXPECT_EQ(path, expect);
  }
  {
    JsonPointerType path({"a", 0, "b", 1, "c", 2, "d"});
    EXPECT_EQ(path / "3", expect);
    path /= "3";
    EXPECT_EQ(path, expect);
  }
  {
    JsonPointerType path({"a", 0, "b", 1, "c"});
    EXPECT_EQ(path / 2 / JsonPointerType({"d", "3"}), expect);
    path /= 2;
    path /= JsonPointerType({"d", "3"});
    EXPECT_EQ(path, expect);
  }
}

TYPED_TEST(JsonPointerTest, QueryNode) {
  using JsonPointerType = TypeParam;
  using QueryNode = typename JsonPointerType::JsonPointerNodeType;
  {
    QueryNode n1(0);
    EXPECT_TRUE(n1.IsNum());
    EXPECT_FALSE(n1.IsStr());
    EXPECT_EQ(0, n1.GetNum());
    EXPECT_EQ(n1.GetStr(), "");
    EXPECT_EQ(0, n1.GetStr().size());
  }

  {
    std::string str{"hello"};
    QueryNode n1(str);
    EXPECT_FALSE(n1.IsNum());
    EXPECT_TRUE(n1.IsStr());
    EXPECT_EQ(0, n1.GetNum());
    EXPECT_EQ(n1.GetStr(), "hello");
    EXPECT_EQ(5, n1.GetStr().size());
  }

  {
    QueryNode n1(StringView("hello", 3));
    EXPECT_FALSE(n1.IsNum());
    EXPECT_TRUE(n1.IsStr());
    EXPECT_EQ(0, n1.GetNum());
    EXPECT_EQ(n1.GetStr(), "hel");
    EXPECT_EQ(3, n1.GetStr().size());
  }

  {
    QueryNode n1("hello");
    EXPECT_FALSE(n1.IsNum());
    EXPECT_TRUE(n1.IsStr());
    EXPECT_EQ(0, n1.GetNum());
    EXPECT_EQ(n1.GetStr(), "hello");
    EXPECT_EQ(5, n1.GetStr().size());
  }

  {
    std::string str{"hello"};
    QueryNode n2(str);
    QueryNode n1(n2);
    EXPECT_FALSE(n1.IsNum());
    EXPECT_TRUE(n1.IsStr());
    EXPECT_EQ(0, n1.GetNum());
    EXPECT_EQ(n1.GetStr(), "hello");
    EXPECT_EQ(5, n1.GetStr().size());
  }

  {
    std::string str{"hello"};
    QueryNode n2(str);
    QueryNode n1(std::move(n2));
    EXPECT_FALSE(n1.IsNum());
    EXPECT_TRUE(n1.IsStr());
    EXPECT_EQ(0, n1.GetNum());
    EXPECT_EQ(n1.GetStr(), "hello");
    EXPECT_EQ(5, n1.GetStr().size());
  }

  {
    std::string str{"hello"};
    QueryNode n2(str);
    QueryNode n1(StringView("world", 5));
    n1 = n2;
    EXPECT_FALSE(n1.IsNum());
    EXPECT_TRUE(n1.IsStr());
    EXPECT_EQ(0, n1.GetNum());
    EXPECT_EQ(n1.GetStr(), "hello");
    EXPECT_EQ(5, n1.GetStr().size());
  }

  {
    std::string str{"hello"};
    QueryNode n2(str);
    QueryNode n1(StringView("world", 5));
    n1 = std::move(n2);
    EXPECT_FALSE(n1.IsNum());
    EXPECT_TRUE(n1.IsStr());
    EXPECT_EQ(0, n1.GetNum());
    EXPECT_EQ(n1.GetStr(), "hello");
    EXPECT_EQ(5, n1.GetStr().size());
  }
}

}  // namespace
