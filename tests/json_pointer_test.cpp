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
#include "sonic/sonic.h"
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
#define TEST_INIT_TYPE(type)    \
  do {                          \
    JPNodeType node((type)(0)); \
  } while (0)
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

TEST(JsonPointerRfc6901, ParseReferenceTokens) {
  JsonPointer pointer;

  EXPECT_TRUE(pointer.Parse(""));
  EXPECT_TRUE(pointer.empty());

  ASSERT_TRUE(pointer.Parse("/foo/0/a~1b/m~0n/~01/"));
  ASSERT_EQ(pointer.size(), 6);
  const std::vector<std::string> expected = {"foo", "0",  "a/b",
                                             "m~n", "~1", ""};
  for (size_t i = 0; i < expected.size(); ++i) {
    EXPECT_TRUE(pointer[i].IsStr());
    EXPECT_EQ(pointer[i].GetStr(), expected[i]);
  }

  const char pointer_with_nul[] = {'/', 'a', '\0', 'b'};
  ASSERT_TRUE(
      pointer.Parse(StringView(pointer_with_nul, sizeof(pointer_with_nul))));
  ASSERT_EQ(pointer.size(), 1);
  EXPECT_EQ(pointer[0].GetStr(), std::string("a\0b", 3));
}

TEST(JsonPointerRfc6901, RejectInvalidSyntax) {
  JsonPointer pointer;
  for (StringView invalid : {StringView("foo"), StringView("/~"),
                             StringView("/~2"), StringView("/foo/~a")}) {
    EXPECT_FALSE(pointer.Parse(invalid));
    EXPECT_TRUE(pointer.empty());
  }
}

TEST(JsonPointerRfc6901, ResolveObjectsAndArrays) {
  Document doc;
  doc.Parse(R"({
    "foo":["bar","baz"],
    "":0,
    "a/b":1,
    "m~n":2,
    "c%d":3,
    "e^f":4,
    "g|h":5,
    "i\\j":6,
    "k\"l":7,
    " ":8,
    "0":"object zero",
    "arr":["zero","one"],
    "leading":{"01":"object leading zero"}
  })");
  ASSERT_FALSE(doc.HasParseError());

  auto resolve = [&doc](StringView source) -> Node* {
    JsonPointer pointer;
    return pointer.Parse(source) ? doc.AtPointer(pointer) : nullptr;
  };

  EXPECT_EQ(resolve(""), &doc);

  Node* node = resolve("/foo/0");
  ASSERT_NE(node, nullptr);
  EXPECT_EQ(node->GetString(), "bar");

  node = resolve("/");
  ASSERT_NE(node, nullptr);
  EXPECT_EQ(node->GetUint64(), 0);

  node = resolve("/a~1b");
  ASSERT_NE(node, nullptr);
  EXPECT_EQ(node->GetUint64(), 1);

  node = resolve("/m~0n");
  ASSERT_NE(node, nullptr);
  EXPECT_EQ(node->GetUint64(), 2);

  node = resolve("/c%d");
  ASSERT_NE(node, nullptr);
  EXPECT_EQ(node->GetUint64(), 3);

  node = resolve("/e^f");
  ASSERT_NE(node, nullptr);
  EXPECT_EQ(node->GetUint64(), 4);

  node = resolve("/g|h");
  ASSERT_NE(node, nullptr);
  EXPECT_EQ(node->GetUint64(), 5);

  node = resolve("/i\\j");
  ASSERT_NE(node, nullptr);
  EXPECT_EQ(node->GetUint64(), 6);

  node = resolve("/k\"l");
  ASSERT_NE(node, nullptr);
  EXPECT_EQ(node->GetUint64(), 7);

  node = resolve("/ ");
  ASSERT_NE(node, nullptr);
  EXPECT_EQ(node->GetUint64(), 8);

  node = resolve("/0");
  ASSERT_NE(node, nullptr);
  EXPECT_EQ(node->GetString(), "object zero");

  node = resolve("/arr/1");
  ASSERT_NE(node, nullptr);
  EXPECT_EQ(node->GetString(), "one");

  node = resolve("/leading/01");
  ASSERT_NE(node, nullptr);
  EXPECT_EQ(node->GetString(), "object leading zero");

  EXPECT_EQ(resolve("/arr/01"), nullptr);
  EXPECT_EQ(resolve("/arr/-"), nullptr);
  EXPECT_EQ(resolve("/arr/184467440737095516160"), nullptr);
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
