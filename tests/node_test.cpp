
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

#include <map>
#include <string>

#include "gtest/gtest.h"
#include "sonic/dom/dynamicnode.h"
#include "sonic/dom/parser.h"
#include "sonic/sonic.h"
namespace {

using namespace sonic_json;

template <typename T>
class NodeTest : public testing::Test {
 public:
  using NodeType = T;
  using Allocator = typename T::alloc_type;
  void CreateNode(NodeType& node, Allocator& a) {
    node.SetObject();
    {
      /*
       {
        "String":"Hello World!",
        "Double":1.0,
        "Int":1,
        "True":true,
        "False":false,
        "Null":null,
        "Object":{
            "New_object":{
                "String":"Hello World!",
                "Double":1.0,
                "Int":1,
                "True":true,
                "False":false,
                "Null":null,
                "Object":{},
                "Array":[]
            }
        },
        "Array":[
            {
                "String":"Hello World!",
                "Double":1.0,
                "Int":1,
                "True":true,
                "False":false,
                "Null":null,
                "Object":{
                    "New_object":{
                        "String":"Hello World!",
                        "Double":1.0,
                        "Int":1,
                        "True":true,
                        "False":false,
                        "Null":null,
                        "Object":{},
                        "Array":[]
                    }
                },
                "Array":[]
            }
            ],
            "EString":"",
            "EObject":{},
            "EArray":[]
        }
       */
      NodeType node_obj(kObject);
      node_obj.AddMember("String", NodeType("Hello World!", a), a);
      node_obj.AddMember("Double", NodeType(1.0), a);
      node_obj.AddMember("Int", NodeType(1), a);
      node_obj.AddMember("True", NodeType(true), a);
      node_obj.AddMember("False", NodeType(false), a);
      node_obj.AddMember("Null", NodeType(kNull), a);
      node_obj.AddMember("Object", NodeType(kObject), a);
      node_obj.AddMember("Array", NodeType(kArray), a);
      NodeType node_tmp(node_obj, a);
      node_obj["Object"].AddMember("New_object", std::move(node_tmp), a);
      node_tmp.CopyFrom(node_obj, a);
      node_obj["Array"].PushBack(std::move(node_tmp), a);

      // add empty containers
      node_obj.AddMember("EString", NodeType(kString), a);
      node_obj.AddMember("EObject", NodeType(kObject), a);
      node_obj.AddMember("EArray", NodeType(kArray), a);

      node = std::move(node_obj);
    }
    return;
  }

  void Add100Nodes(NodeType& node, Allocator& a) {
    node.SetObject();
    for (int i = 0; i < 100; ++i) {
      std::string key = "key" + std::to_string(i);
      auto iter = node.AddMember(key, NodeType(i), a);
      EXPECT_TRUE(iter->name == key);
    }
    EXPECT_EQ(100, node.Size());
  }

  void Push100Nodes(NodeType& node, Allocator& a) {
    node.SetArray();
    for (int i = 0; i < 100; ++i) {
      node.PushBack(NodeType(i), a);
    }
    EXPECT_EQ(100, node.Size());
  }
};

using MAllocType = MemoryPoolAllocator<>;
using SAllocType = SimpleAllocator;
using DNodeMempoolNode = DNode<MAllocType>;
using DNodeSimpleNode = DNode<SAllocType>;

using NodeTypes = testing::Types<DNodeMempoolNode, DNodeSimpleNode>;
TYPED_TEST_SUITE(NodeTest, NodeTypes);

TYPED_TEST(NodeTest, BasciConstrcut) {
  using NodeType = TypeParam;
  using Allocator = typename NodeType::alloc_type;
  Allocator alloc;

  EXPECT_TRUE(NodeType().IsNull());
  EXPECT_TRUE(NodeType(kNull).IsNull());

  NodeType arr = NodeType(kArray);
  EXPECT_TRUE(arr.IsArray());
  EXPECT_TRUE(arr.Size() == 0);

  NodeType obj = NodeType(kObject);
  EXPECT_TRUE(obj.IsObject());
  EXPECT_TRUE(obj.Size() == 0);

  EXPECT_TRUE(NodeType(kBool).IsFalse());
  EXPECT_TRUE(NodeType(true).IsTrue());
  EXPECT_TRUE(NodeType(false).IsFalse());
  EXPECT_TRUE(NodeType(int(0)).IsInt64());
  EXPECT_TRUE(NodeType(1).IsUint64());
  EXPECT_TRUE(NodeType(int64_t(-1)).IsInt64());
  EXPECT_TRUE(NodeType(uint64_t(10000)).IsUint64());
  EXPECT_TRUE(NodeType(double(1.2)).IsDouble());
  EXPECT_TRUE(NodeType(float(-1.2f)).IsDouble());

  EXPECT_TRUE(NodeType(kString).IsString());
  EXPECT_TRUE(NodeType(kString).Size() == 0);
  EXPECT_EQ(NodeType(kString).GetString(), "");
  EXPECT_EQ(NodeType(kString).GetStringView(), "");

  EXPECT_TRUE(NodeType("").IsString());
  EXPECT_TRUE(NodeType("").IsStringConst());
  EXPECT_TRUE(NodeType("").Size() == 0);
  EXPECT_EQ(NodeType("").GetString(), "");
  EXPECT_EQ(NodeType("").GetStringView(), "");

  EXPECT_TRUE(NodeType("hi").IsString());
  EXPECT_TRUE(NodeType("hi").Size() == 2);

  EXPECT_TRUE(NodeType("hi", 2).IsString());
  EXPECT_TRUE(NodeType("hi", 2).Size() == 2);
  EXPECT_TRUE(NodeType(std::string("hi")).IsString());
  EXPECT_TRUE(NodeType(std::string("hi")).Size() == 2);

  EXPECT_FALSE(NodeType("hi", alloc).IsStringConst());
  EXPECT_TRUE(NodeType("hi", alloc).Size() == 2);
  EXPECT_FALSE(NodeType("hi", 2, alloc).IsStringConst());
  EXPECT_TRUE(NodeType("hi", 2, alloc).Size() == 2);
  EXPECT_FALSE(NodeType(std::string("hi"), alloc).IsStringConst());
  EXPECT_TRUE(NodeType(std::string("hi"), alloc).Size() == 2);
}

TYPED_TEST(NodeTest, CopyConstrcut) {
  using NodeType = TypeParam;
  using Allocator = typename NodeType::alloc_type;
  Allocator alloc;
  NodeType old;
  TestFixture::CreateNode(old, alloc);
  for (auto it = old.MemberBegin(); it != old.MemberEnd(); ++it) {
    NodeType node = NodeType(it->value, alloc);
    EXPECT_TRUE(node == it->value);
  }
}

TYPED_TEST(NodeTest, MoveConstrcut) {
  using NodeType = TypeParam;
  using Allocator = typename NodeType::alloc_type;
  Allocator alloc;
  NodeType old;
  TestFixture::CreateNode(old, alloc);
  for (auto it = old.MemberBegin(); it != old.MemberEnd(); ++it) {
    NodeType& old = it->value;
    NodeType copied = NodeType(old, alloc);
    NodeType moved = NodeType(std::move(old));
    EXPECT_TRUE(moved == copied);
    // origin node become null after moved
    EXPECT_TRUE(old.IsNull());
  }
}

TYPED_TEST(NodeTest, Get) {
  using NodeType = TypeParam;
  using Allocator = typename NodeType::alloc_type;
  Allocator alloc;
  NodeType node;
  // Boolean
  node.SetBool(true);
  EXPECT_TRUE(node.GetBool());
  node.SetBool(false);
  EXPECT_FALSE(node.GetBool());

  // String
  node.SetString(std::string("Hello, World!\n"), alloc);
  EXPECT_EQ("Hello, World!\n", node.GetString());
  EXPECT_EQ("Hello, World!\n", node.GetStringView());

  // Number
  node.SetInt64(-1);
  EXPECT_EQ(-1, node.GetInt64());
  EXPECT_EQ(-1.0, node.GetDouble());
  node.SetUint64(0);
  EXPECT_EQ(0.0, node.GetDouble());
  node.SetDouble(double(0.0));
  EXPECT_EQ(0.0, node.GetDouble());
}

TYPED_TEST(NodeTest, Equal) {
  using NodeType = TypeParam;
  using Allocator = typename NodeType::alloc_type;
  Allocator a;
  NodeType node1, node2;
  {
    TestFixture::Add100Nodes(node1, a);
    node2.CopyFrom(node1, a);
    EXPECT_TRUE(node2.RemoveMember("key0"));
    EXPECT_FALSE(node1 == node2);
    EXPECT_FALSE(node2 == node1);
  }

  {
    node1.SetObject();
    node2.SetObject();
    node1.AddMember("key1", NodeType(1.0), a);
    node2.AddMember("key2", NodeType(1.0), a);
    EXPECT_FALSE(node1 == node2);
    EXPECT_FALSE(node2 == node1);
    node2.SetObject();
    node2.AddMember("key1", NodeType(1), a);
    EXPECT_FALSE(node1 == node2);
    EXPECT_FALSE(node2 == node1);
  }

  {
    node1.SetArray();
    node2.SetArray();
    node1.PushBack(NodeType(1.0), a);
    EXPECT_FALSE(node1 == node2);
    EXPECT_FALSE(node2 == node1);
    node2.PushBack(NodeType(0), a);
    EXPECT_FALSE(node1 == node2);
    EXPECT_FALSE(node2 == node1);
  }

  {
    node1.CopyFrom(NodeType(0), a);
    node2.CopyFrom(NodeType(-1), a);
    EXPECT_FALSE(node1 == node2);
    EXPECT_FALSE(node2 == node1);

    node2.CopyFrom(NodeType(0.0), a);
    EXPECT_FALSE(node1 == node2);
    EXPECT_FALSE(node2 == node1);
  }
}

TYPED_TEST(NodeTest, FindMember) {
  using NodeType = TypeParam;
  using Allocator = typename NodeType::alloc_type;
  NodeType obj;
  Allocator alloc;
  obj.SetObject();
  obj.CreateMap(alloc);
  TestFixture::CreateNode(obj, alloc);
  // Find with map
  EXPECT_TRUE(obj.FindMember("Array")->name == "Array");
  EXPECT_TRUE(obj.FindMember(std::string("Unknwon")) == obj.MemberEnd());
  EXPECT_TRUE(obj["Object"].IsObject());
  EXPECT_TRUE(obj["String"] == "Hello World!");
  EXPECT_TRUE(obj["Unknown"].IsNull());
  EXPECT_TRUE(obj[std::string("False")].IsFalse());
  {
    NodeType obj1;
    obj1.CopyFrom(obj, alloc);
    EXPECT_TRUE(obj.FindMember("Array")->name == "Array");
    EXPECT_TRUE(obj.FindMember(std::string("Unknwon")) == obj.MemberEnd());
    EXPECT_TRUE(obj["Object"].IsObject());
    EXPECT_TRUE(obj["String"] == "Hello World!");
    EXPECT_TRUE(obj["Unknown"].IsNull());
    EXPECT_TRUE(obj[std::string("False")].IsFalse());
  }

  // Check return value always Null when provided key does'nt exist.
  {
    auto& value = obj["Unknown"];
    EXPECT_TRUE(value.IsNull());
    value.SetInt64(1);  // illed codes
    auto& value1 = obj["Unknown"];
    EXPECT_TRUE(value1.IsNull());
  }
}

template <typename StringType, typename NodeType>
void AtPointerHelper(const NodeType& obj) {
  EXPECT_TRUE(
      obj.template AtPointer<StringType>({"Object", "New_object", "Double"})
          ->IsDouble());
  EXPECT_TRUE(
      obj.template AtPointer<StringType>({"Array", 0, "String"})->IsString());
  EXPECT_TRUE(obj.template AtPointer<StringType>({0}) == nullptr);
  EXPECT_TRUE(obj.template AtPointer<StringType>({"Unknown"}) == nullptr);
  EXPECT_TRUE(obj.template AtPointer<StringType>(
                  {"Object", "Array", 1, "Double"}) == nullptr);
  EXPECT_TRUE(obj.template AtPointer<StringType>({"EArray", 0}) == nullptr);
  EXPECT_TRUE(obj.template AtPointer<StringType>({"EArray", -1}) == nullptr);
  EXPECT_TRUE(obj.template AtPointer<StringType>({"Object", 0}) == nullptr);
}

TYPED_TEST(NodeTest, AtPointer) {
  using NodeType = TypeParam;
  using Allocator = typename NodeType::alloc_type;
  NodeType obj;
  Allocator alloc;
  TestFixture::CreateNode(obj, alloc);

  using JsonPointerType = sonic_json::GenericJsonPointer<std::string>;
  EXPECT_TRUE(obj.AtPointer(JsonPointerType({"Object", "New_object", "Double"}))
                  ->IsDouble());
  EXPECT_TRUE(
      obj.AtPointer(JsonPointerType({"Array", 0, "String"}))->IsString());
  EXPECT_TRUE(obj.AtPointer(JsonPointerType({0})) == nullptr);
  EXPECT_TRUE(obj.AtPointer(JsonPointerType({"Unknown"})) == nullptr);
  EXPECT_TRUE(obj.AtPointer(JsonPointerType(
                  {"Object", "Array", 1, "Double"})) == nullptr);
  EXPECT_TRUE(obj.AtPointer(JsonPointerType({"EArray", 0})) == nullptr);
  EXPECT_TRUE(obj.AtPointer(JsonPointerType({"EArray", -1})) == nullptr);
  EXPECT_TRUE(obj.AtPointer(JsonPointerType({"Object", 0})) == nullptr);

  AtPointerHelper<std::string>(obj);
  AtPointerHelper<sonic_json::StringView>(obj);

  // Test Recursive AtPointer
  {
    EXPECT_TRUE(obj.AtPointer("Object", "New_object", "Double")->IsDouble());
    EXPECT_TRUE(obj.AtPointer("Array", 0, "String")->IsString());
    EXPECT_TRUE(obj.AtPointer(0) == nullptr);
    EXPECT_TRUE(obj.AtPointer("Unknown") == nullptr);
    EXPECT_TRUE(obj.AtPointer("Object", "Array", 1, "Double") == nullptr);
    EXPECT_TRUE(obj.AtPointer("EArray", 0) == nullptr);
    EXPECT_TRUE(obj.AtPointer("EArray", -1) == nullptr);
    EXPECT_TRUE(obj.AtPointer("Object", 0) == nullptr);
  }
}

TYPED_TEST(NodeTest, AddMember) {
  using NodeType = TypeParam;
  using Allocator = typename NodeType::alloc_type;
  NodeType node1;
  Allocator a;

  TestFixture::Add100Nodes(node1, a);
}

TYPED_TEST(NodeTest, RemoveMember) {
  using NodeType = TypeParam;
  using Allocator = typename NodeType::alloc_type;
  NodeType node1;
  Allocator a;

  TestFixture::Add100Nodes(node1, a);

  for (int i = 99; i >= 0; --i) {
    std::string key = "key" + std::to_string(i);
    EXPECT_TRUE(node1.RemoveMember(key));
    EXPECT_FALSE(node1.HasMember(key));
    EXPECT_FALSE(node1.RemoveMember("Unkown"));
  }
  EXPECT_TRUE(node1.Empty());

  {
    NodeType node2;
    TestFixture::Add100Nodes(node1, a);
    node2.CopyFrom(node1, a);
    EXPECT_FALSE(node2.RemoveMember("Unkown"));
    for (int i = 99; i >= 0; --i) {
      std::string key = "key" + std::to_string(i);
      EXPECT_TRUE(node2.RemoveMember(key));
      EXPECT_FALSE(node2.RemoveMember("Unkown"));
    }
    EXPECT_TRUE(node2.Empty());
    EXPECT_FALSE(node2.RemoveMember("Unkown"));

    node2.SetObject();
    EXPECT_FALSE(node2.RemoveMember("Unkown"));
  }
}

TYPED_TEST(NodeTest, EraseMember) {
  using NodeType = TypeParam;
  using Allocator = typename NodeType::alloc_type;
  NodeType node1;
  Allocator a;

  TestFixture::Add100Nodes(node1, a);

  for (int i = 9; i >= 0; --i) {
    node1.EraseMember(node1.MemberBegin() + i * 10,
                      node1.MemberBegin() + (i + 1) * 10);
  }
  EXPECT_TRUE(node1.Empty());

  TestFixture::Add100Nodes(node1, a);
  using MemberIterator = typename NodeType::MemberIterator;
  for (int i = 0; i < 99; ++i) {
    MemberIterator m =
        node1.EraseMember(node1.MemberBegin(), node1.MemberBegin() + 1);
    std::string expect_key = "key" + std::to_string(i + 1);
    EXPECT_TRUE(m->name == expect_key);
    EXPECT_TRUE(m->value.GetInt64() == i + 1);
  }
  {
    MemberIterator m =
        node1.EraseMember(node1.MemberBegin(), node1.MemberBegin() + 1);
    EXPECT_TRUE(m == node1.MemberEnd());
  }
}

TYPED_TEST(NodeTest, HasMember) {
  using NodeType = TypeParam;
  using Allocator = typename NodeType::alloc_type;
  NodeType node1;
  Allocator a;

  TestFixture::Add100Nodes(node1, a);

  for (int i = 99; i >= 0; --i) {
    std::string key = "key" + std::to_string(i);
    std::string NonExistKey = "NonExist" + std::to_string(i);
    std::string hey = "hey" + std::to_string(i);
    std::string ley = "ley" + std::to_string(i);
    std::string ey = "ey" + std::to_string(i);
    EXPECT_EQ(node1[key].GetInt64(), i);
    EXPECT_TRUE(node1.HasMember(key));
    EXPECT_FALSE(node1.HasMember(NonExistKey));
    EXPECT_FALSE(node1.HasMember(hey));
    EXPECT_FALSE(node1.HasMember(ley));
    EXPECT_FALSE(node1.HasMember(ey));
  }
}

TYPED_TEST(NodeTest, RemoveMemberWithDupKey) {
  using NodeType = TypeParam;
  using Allocator = typename NodeType::alloc_type;
  NodeType node1(kObject);
  NodeType node;
  NodeType node_map(kObject);
  Allocator a;

  EXPECT_TRUE(node1.IsObject());
  EXPECT_TRUE(node_map.IsObject());
  std::string key = "key";
  for (int i = 0; i < 100; ++i) {
    node1.AddMember(key, NodeType(i), a);
    node_map.AddMember(key, NodeType(i), a);
  }
  node.CopyFrom(node1, a);

  for (int i = 0; i < 99; ++i) {
    EXPECT_TRUE(node.RemoveMember(key));
    EXPECT_TRUE(node_map.RemoveMember(key));
    EXPECT_TRUE(node.HasMember(key));
    EXPECT_TRUE(node_map.HasMember(key));
  }
  EXPECT_TRUE(node.RemoveMember(key));
  EXPECT_TRUE(node_map.RemoveMember(key));
  EXPECT_FALSE(node.HasMember(key));
  EXPECT_FALSE(node_map.HasMember(key));
  EXPECT_TRUE(node.Empty());
  EXPECT_TRUE(node_map.Empty());
}

TYPED_TEST(NodeTest, Erase) {
  using NodeType = TypeParam;
  using Allocator = typename NodeType::alloc_type;
  NodeType node1;
  Allocator a;

  TestFixture::Push100Nodes(node1, a);
  {
    NodeType node2;
    node2.CopyFrom(node1, a);
    {
      node2.Erase(node2.Begin() + 50);
      EXPECT_TRUE(node2[50] == 51);
      EXPECT_FALSE(node2[50] != 51);
      EXPECT_TRUE(node2[49] == 49);
      EXPECT_TRUE(node2.Back() == 99);
      EXPECT_TRUE(node2[0] == 0);
      EXPECT_TRUE(node2.Size() == 99);
    }
    {
      node2.Erase(node2.Begin());
      EXPECT_TRUE(node2.Back() == 99);
      EXPECT_TRUE(node2[0] == 1);
      EXPECT_TRUE(node2.Size() == 98);
    }
  }
  {
    NodeType node2;
    node2.CopyFrom(node1, a);
    for (int i = 0; i < 10; ++i) {
      node2.Erase(node2.Begin(), node2.Begin() + 10);
      EXPECT_TRUE(node2.Size() == size_t((9 - i) * 10));
    }
  }
  {
    NodeType node2;
    node2.CopyFrom(node1, a);
    for (int i = 0; i < 10; ++i) {
      node2.Erase(node2.End() - 10, node2.End());
      EXPECT_TRUE(node2.Size() == size_t((9 - i) * 10));
    }
  }
  {
    NodeType node2;
    node2.CopyFrom(node1, a);
    for (int i = 0; i < 10; ++i) {
      node2.Erase(0, 10);
      EXPECT_TRUE(node2.Size() == size_t((9 - i) * 10));
    }
  }
}

TYPED_TEST(NodeTest, Back) {
  using NodeType = TypeParam;
  using Allocator = typename NodeType::alloc_type;
  NodeType node1;
  Allocator a;

  TestFixture::Push100Nodes(node1, a);
  for (int i = 0; i < 100; ++i) {
    EXPECT_TRUE(node1.Back() == 99 - i);
    node1.PopBack();
  }
}

TYPED_TEST(NodeTest, CopyFrom) {
  using NodeType = TypeParam;
  using Allocator = typename NodeType::alloc_type;
  NodeType node1;
  Allocator a;

  TestFixture::CreateNode(node1, a);
  NodeType node2;
  node2.CopyFrom(node1, a);
  EXPECT_TRUE(node2 == node1);

  // Copy From a Empty
  node1["Array"].Clear();
  NodeType node3;
  node3.CopyFrom(node1, a);
  node2.CopyFrom(node1, a);
  EXPECT_TRUE(node2 == node3);

  node3["Object"].Clear();
  NodeType node4;
  node4.CopyFrom(node3, a);
  EXPECT_TRUE(node3 == node4);
}

TYPED_TEST(NodeTest, Set) {
  using NodeType = TypeParam;
  using Allocator = typename NodeType::alloc_type;
  std::vector<NodeType> vec;
  vec.emplace_back("123");
  vec.emplace_back(1);
  vec.emplace_back(false);
  vec.emplace_back(kNull);
  vec.emplace_back(kObject);
  vec.emplace_back(kArray);
  Allocator alloc;
  for (auto& node : vec) {
    const char* sv = "string view";
    EXPECT_TRUE(node.SetNull().IsNull());
    EXPECT_FALSE(node.SetString("copied string", alloc).IsStringConst());
    EXPECT_TRUE(node == "copied string");
    EXPECT_TRUE(node.SetString("string view").IsStringConst());
    EXPECT_TRUE(node == "string view");
    EXPECT_TRUE(node.SetString(sv) == "string view");
    EXPECT_FALSE(node.SetString(sv) != "string view");
    EXPECT_FALSE(node.SetString(sv) != sonic_json::StringView(sv));
    EXPECT_TRUE(node.SetString(sonic_json::StringView(sv)) == "string view");
    EXPECT_TRUE(node.IsString() && node.IsStringConst());
    EXPECT_TRUE(node.SetObject().IsObject());
    EXPECT_TRUE(node.SetUint64(1).IsUint64());
    EXPECT_TRUE(node.SetArray().IsArray());
    EXPECT_TRUE(node.SetUint64(1).IsUint64());
    EXPECT_TRUE(node.SetBool(false).IsBool());
    EXPECT_TRUE(node.SetInt64(1).IsInt64());
    EXPECT_TRUE(node.SetDouble(1.23).IsDouble());
  }
}

TYPED_TEST(NodeTest, Iterator) {
  using NodeType = TypeParam;

  {
    NodeType empty_obj(sonic_json::kObject);
    EXPECT_TRUE(empty_obj.MemberBegin() == empty_obj.MemberEnd());
    EXPECT_TRUE(empty_obj.CMemberBegin() == empty_obj.MemberEnd());
    EXPECT_TRUE(empty_obj.MemberBegin() == empty_obj.CMemberEnd());
    EXPECT_TRUE(empty_obj.CMemberBegin() == empty_obj.CMemberEnd());
  }
  {
    NodeType empty_arr(sonic_json::kArray);
    EXPECT_TRUE(empty_arr.Begin() == empty_arr.End());
    EXPECT_TRUE(empty_arr.CBegin() == empty_arr.End());
    EXPECT_TRUE(empty_arr.Begin() == empty_arr.CEnd());
    EXPECT_TRUE(empty_arr.CBegin() == empty_arr.CEnd());
  }
}
TYPED_TEST(NodeTest, PushBack) {
  using NodeType = TypeParam;
  using Allocator = typename NodeType::alloc_type;

  Allocator alloc;
  NodeType arr(kArray);

  for (int i = 0; i < 100; i++) {
    if (i % 2) {
      arr.PushBack(NodeType(i), alloc);
    } else {
      NodeType node(kArray);
      node.PushBack(NodeType(i), alloc);
      arr.PushBack(std::move(node), alloc);
    }
  }
  for (int i = 0; i < 100; i++) {
    if (i % 2) {
      EXPECT_TRUE(arr[i].GetInt64() == i);
      EXPECT_TRUE(arr[(size_t)i].GetInt64() == i);
    } else {
      EXPECT_TRUE(arr[i].Begin()->GetInt64() == i);
      EXPECT_TRUE(arr[(size_t)i].Begin()->GetInt64() == i);
    }
  }
}

TYPED_TEST(NodeTest, PopBack) {
  using NodeType = TypeParam;
  using Allocator = typename NodeType::alloc_type;

  Allocator alloc;
  NodeType arr{kArray};
  for (int i = 0; i < 100; i++) {
    if (i % 2) {
      arr.PushBack(NodeType(i), alloc);
    } else {
      NodeType node(kArray);
      node.PushBack(NodeType(i), alloc);
      arr.PushBack(std::move(node), alloc);
    }
  }
  for (int i = 99; i >= 0; i--) {
    if (i % 2) {
      EXPECT_TRUE(arr[i].GetInt64() == i);
    } else {
      EXPECT_TRUE(arr[i].Begin()->GetInt64() == i);
    }
    EXPECT_TRUE(arr.PopBack().Size() == static_cast<size_t>(i));
  }
  EXPECT_TRUE(arr.Empty());
}

// specific test for DNode
template <typename T>
class DNodeTest : public NodeTest<T> {};

using DNodeTypes = testing::Types<DNodeMempoolNode, DNodeSimpleNode>;
TYPED_TEST_SUITE(DNodeTest, DNodeTypes);

TYPED_TEST(DNodeTest, Reserve) {
  using NodeType = TypeParam;
  using Allocator = typename NodeType::alloc_type;

  NodeType arr(kArray);
  NodeType obj(kObject);
  Allocator alloc;

  arr.Reserve(0, alloc);
  EXPECT_TRUE(arr.Capacity() == 0);
  arr.Reserve(100, alloc);
  EXPECT_TRUE(arr.Capacity() == 100);
  EXPECT_TRUE(arr.Size() == 0);

  obj.MemberReserve(0, alloc);
  EXPECT_TRUE(obj.Capacity() == 0);
  obj.MemberReserve(100, alloc);
  EXPECT_TRUE(obj.Capacity() == 100);
  EXPECT_TRUE(obj.Size() == 0);
}

TYPED_TEST(DNodeTest, CopyFrom) {
  using NodeType = TypeParam;
  using Allocator = typename NodeType::alloc_type;
  Allocator a;

  {
    NodeType node1;
    TestFixture::CreateNode(node1, a);
    NodeType node2;
    node2.CopyFrom(node1, a);
    EXPECT_TRUE(node2 == node1);
    EXPECT_TRUE(node2.Capacity() == node2.Size());
    EXPECT_TRUE(node2["Array"].Capacity() == node2["Array"].Size());
  }

  {
    NodeType node1;
    TestFixture::CreateNode(node1, a);
    node1["Array"].Clear();
    node1["Object"].Clear();
    NodeType node2;
    node2.CopyFrom(node1, a);
    EXPECT_TRUE(node2 == node1);
    EXPECT_TRUE(node2.Capacity() == node2.Size());
    EXPECT_TRUE(node2["Array"].Capacity() == node2["Array"].Size());
  }
}

TYPED_TEST(DNodeTest, BasciConstrcut) {
  using NodeType = TypeParam;
  EXPECT_TRUE(NodeType(kArray).Capacity() == 0);
  EXPECT_TRUE(NodeType(kObject).Capacity() == 0);
}

// invalid allocator which only allocate nullptr
class InvalidAllocator {
 public:
  static void* Malloc(size_t) { return nullptr; }
  static void* Realloc(void*, size_t, size_t) { return nullptr; }

  static void Free(void*) { return; }
  static constexpr bool kNeedFree = true;
};

TEST(DNodeTest, AllocatorReturnNull) {
  using NodeType = DNode<InvalidAllocator>;
  InvalidAllocator a;
  std::string str = "Hello World";
  NodeType node1(str, a);
  EXPECT_TRUE(node1.IsString());
  EXPECT_TRUE(node1.Size() == 0);
  EXPECT_EQ(node1.GetString(), "");
  EXPECT_EQ(node1.GetStringView(), "");
}

TYPED_TEST(NodeTest, SourceAllocator) {
  using NodeType = TypeParam;
  using Allocator = typename NodeType::alloc_type;

  Node a("hello");
  Node b("world");
  Allocator alloc;

  NodeType c(a, alloc);
  EXPECT_EQ(c, a);
  EXPECT_NE(c, b);
  EXPECT_TRUE(c.GetStringView() == a.GetStringView());
}

}  // namespace
