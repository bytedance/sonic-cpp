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

#include <dirent.h>

#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "sonic/dom/generic_document.h"
#include "sonic/dom/parser.h"
#include "sonic/sonic.h"

namespace {

using namespace sonic_json;
using namespace sonic_json::internal;
using Document = GenericDocument<DNode<>>;

TEST(Document, DNode) {
  // constructors
  {
    int i = -1;
    const DNode<> node(i);
    EXPECT_TRUE(node.IsInt64());
    EXPECT_TRUE(node.IsNumber());
    EXPECT_EQ(-1, node.GetInt64());
  }
  {
    unsigned int i = 1;
    const DNode<> node(i);
    EXPECT_TRUE(node.IsUint64());
    EXPECT_TRUE(node.IsNumber());
    EXPECT_EQ(1, node.GetUint64());
  }
  {
    int64_t i = -1;
    const DNode<> node(i);
    EXPECT_TRUE(node.IsNumber());
    EXPECT_TRUE(node.IsInt64());
    EXPECT_FALSE(node.IsNull());
    EXPECT_FALSE(node.IsBool());
    EXPECT_FALSE(node.IsTrue());
    EXPECT_FALSE(node.IsFalse());
    EXPECT_FALSE(node.IsUint64());
    EXPECT_FALSE(node.IsDouble());
    EXPECT_FALSE(node.IsString());
    EXPECT_FALSE(node.IsArray());
    EXPECT_FALSE(node.IsObject());
    EXPECT_FALSE(node.IsContainer());
    EXPECT_EQ(-1, node.GetInt64());
  }
  if (0) {  // TODO: IsInt64() is false
    uint64_t i = 0xFFFFFFFFFFFFFFFF;
    const DNode<> node(i);
    EXPECT_TRUE(node.IsUint64());
    EXPECT_TRUE(node.IsNumber());
    EXPECT_FALSE(node.IsNull());
    EXPECT_FALSE(node.IsBool());
    EXPECT_FALSE(node.IsTrue());
    EXPECT_FALSE(node.IsFalse());
    EXPECT_TRUE(node.IsInt64());
    EXPECT_FALSE(node.IsDouble());
    EXPECT_FALSE(node.IsString());
    EXPECT_FALSE(node.IsArray());
    EXPECT_FALSE(node.IsObject());
    EXPECT_FALSE(node.IsContainer());
    EXPECT_EQ(0xFFFFFFFFFFFFFFFF, node.GetUint64());
  }
  {
    double d = 1.0;
    const DNode<> node(d);
    EXPECT_TRUE(node.IsDouble());
    EXPECT_TRUE(node.IsNumber());
    EXPECT_FALSE(node.IsNull());
    EXPECT_FALSE(node.IsBool());
    EXPECT_FALSE(node.IsTrue());
    EXPECT_FALSE(node.IsFalse());
    EXPECT_FALSE(node.IsInt64());
    EXPECT_FALSE(node.IsUint64());
    EXPECT_FALSE(node.IsString());
    EXPECT_FALSE(node.IsArray());
    EXPECT_FALSE(node.IsObject());
    EXPECT_FALSE(node.IsContainer());
    EXPECT_EQ(1.0, node.GetDouble());
  }
  {
    float f = 1.0;
    const DNode<> node(f);
    EXPECT_TRUE(node.IsDouble());
    EXPECT_TRUE(node.IsNumber());
    EXPECT_FALSE(node.IsNull());
    EXPECT_FALSE(node.IsBool());
    EXPECT_FALSE(node.IsTrue());
    EXPECT_FALSE(node.IsFalse());
    EXPECT_FALSE(node.IsInt64());
    EXPECT_FALSE(node.IsUint64());
    EXPECT_FALSE(node.IsString());
    EXPECT_FALSE(node.IsArray());
    EXPECT_FALSE(node.IsObject());
    EXPECT_FALSE(node.IsContainer());
    EXPECT_EQ(1.0, node.GetDouble());
  }
  {
    const char* s = "Hello world!";
    const DNode<> node(s);
    EXPECT_FALSE(node.IsDouble());
    EXPECT_FALSE(node.IsNumber());
    EXPECT_FALSE(node.IsNull());
    EXPECT_FALSE(node.IsBool());
    EXPECT_FALSE(node.IsTrue());
    EXPECT_FALSE(node.IsFalse());
    EXPECT_FALSE(node.IsInt64());
    EXPECT_FALSE(node.IsUint64());
    EXPECT_TRUE(node.IsString());
    EXPECT_FALSE(node.IsArray());
    EXPECT_FALSE(node.IsObject());
    EXPECT_FALSE(node.IsContainer());
    EXPECT_EQ(s, node.GetString());
    EXPECT_EQ(std::strlen(s), node.Size());
  }
  {
    std::string s = "Hello world!";
    const DNode<> node(s);
    EXPECT_FALSE(node.IsDouble());
    EXPECT_FALSE(node.IsNumber());
    EXPECT_FALSE(node.IsNull());
    EXPECT_FALSE(node.IsBool());
    EXPECT_FALSE(node.IsTrue());
    EXPECT_FALSE(node.IsFalse());
    EXPECT_FALSE(node.IsInt64());
    EXPECT_FALSE(node.IsUint64());
    EXPECT_TRUE(node.IsString());
    EXPECT_FALSE(node.IsArray());
    EXPECT_FALSE(node.IsObject());
    EXPECT_FALSE(node.IsContainer());
    EXPECT_EQ(s, node.GetString());
    EXPECT_EQ(s.size(), node.Size());
  }
  {
    std::string data =
        R"({"id":12125925,"ids":[-2147483648,2147483647],"title":"未来简史-从智人到智神hello: world, \\ {\" / \b \f \n \r \t } [景] 测试中文 😀","titles":["hello","world"],"price":345.67,"prices":[-0.1,0.1],"hot":true,"hots":[true,true,true],"author":{"name":"json","age":99,"male":true},"authors":[{"name":"json","age":99,"male":true},{"name":"json","age":99,"male":true},{"name":"json","age":99,"male":true}],"weights":[]})";
    Document doc;
    doc.Parse(data);
    EXPECT_FALSE(doc.HasParseError());
    const DNode<>& root = doc;
    EXPECT_EQ(11, root.Size());
    EXPECT_FALSE(root.Empty());
    EXPECT_FALSE(root.IsDouble());
    EXPECT_FALSE(root.IsNumber());
    EXPECT_FALSE(root.IsNull());
    EXPECT_FALSE(root.IsBool());
    EXPECT_FALSE(root.IsTrue());
    EXPECT_FALSE(root.IsFalse());
    EXPECT_FALSE(root.IsInt64());
    EXPECT_FALSE(root.IsUint64());
    EXPECT_FALSE(root.IsString());
    EXPECT_FALSE(root.IsArray());
    EXPECT_TRUE(root.IsObject());
    EXPECT_TRUE(root.IsContainer());
    EXPECT_FALSE(root.Empty());

    const DNode<>& member_ids = root["ids"];
    EXPECT_FALSE(member_ids.IsDouble());
    EXPECT_FALSE(member_ids.IsNumber());
    EXPECT_FALSE(member_ids.IsNull());
    EXPECT_FALSE(member_ids.IsBool());
    EXPECT_FALSE(member_ids.IsTrue());
    EXPECT_FALSE(member_ids.IsFalse());
    EXPECT_FALSE(member_ids.IsInt64());
    EXPECT_FALSE(member_ids.IsUint64());
    EXPECT_FALSE(member_ids.IsString());
    EXPECT_TRUE(member_ids.IsArray());
    EXPECT_FALSE(member_ids.IsObject());
    EXPECT_TRUE(member_ids.IsContainer());
    EXPECT_FALSE(member_ids.Empty());

    EXPECT_EQ(-2147483648, member_ids[0].GetInt64());
    EXPECT_EQ(2147483647, member_ids[1].GetUint64());
    EXPECT_EQ(2, member_ids.Size());

    std::string author_key = "author";
    const auto author_iter = root.FindMember(author_key);
    EXPECT_TRUE(author_iter != root.MemberEnd());
    EXPECT_TRUE(author_iter->name == "author");
    EXPECT_TRUE(author_iter->name == author_key);
  }
}

TEST(Document, ParseBasic) {
  std::vector<std::string> tests = {
      "true",
      "false",
      "null",
      "1",
      "2.5",
      "-0e23",
      "-0.1e+24",
      "9999",
      R"("hello")",
      R"("hello: world, \\ {\" / \b \f \n \r \t } [景] 测试中文 😀")",
      R"({"A":{"b":{"c":{}}}})",
      R"({ "a": "hello", "b": null, "c": 123, "d": { "f": [ {"g": null, "h": 0 } ] } } )",
      R"([ {"g": null, "h": 0} ])",
      R"([ {"g": null, "h": 0}, 1, 2, 3, [1, 2, 3], [], [{}], {}] )",
      R"({"id":12125925,"ids":[-2147483648,2147483647],"title":"未来简史-从智人到智神hello: world, \\ {\" / \b \f \n \r \t } [景] 测试中文 😀","titles":["hello","world"],"price":345.67,"prices":[-0.1,0.1],"hot":true,"hots":[true,true,true],"author":{"name":"json","age":99,"male":true},"authors":[{"name":"json","age":99,"male":true},{"name":"json","age":99,"male":true},{"name":"json","age":99,"male":true}],"weights":[]})",
#ifdef SONIC_NOT_VALIDATE_UTF8
      "\"\xa0\xe1\"",
#endif
  };
  {
    for (const auto& data : tests) {
      Document doc;
      doc.Parse(data.c_str(), data.size());
      EXPECT_FALSE(doc.HasParseError()) << "failed json is: " << data;
      EXPECT_EQ(doc.GetErrorOffset(), data.size())
          << "failed json is: " << data;
    }
  }
  {
    for (const auto& data : tests) {
      std::string error_info = "Error when parsing json: " + data + "\n";

      Document doc;
      doc.Parse(data.c_str(), data.size());
      EXPECT_FALSE(doc.HasParseError()) << error_info;
      EXPECT_EQ(doc.GetErrorOffset(), data.size()) << error_info;

      WriteBuffer wb;
      EXPECT_EQ(doc.Serialize(wb), kErrorNone);
      // EXPECT_STREQ(wb.ToString(), data.c_str());
    }
  }
}

template <typename Document>
class DocumentTest : public testing::Test {
 public:
  DocumentTest() { doc_.Parse(data_.c_str(), data_.size()); }
  void Parse(Document& doc) { doc.Parse(data_.c_str(), data_.size()); }

 protected:
  std::string data_ =
      R"({"id":12125925,"ids":[-2147483648,2147483647],"title":"未来简史","titles":["","world"],"price":345.67,"prices":[-0.1,0.1],"hot":true,"hots":[true,true,true],"author":{"name":"json","age":99,"male":true},"authors":[{"name":null,"age":99,"male":true}, [], [[]]],"weights":[],"extra":{},"other":null})";
  Document doc_{};
};

using DynSimpleDom = GenericDocument<DNode<SimpleAllocator>>;
using DynMempoolDom = GenericDocument<DNode<MemoryPoolAllocator<>>>;

using DomTypes = testing::Types<DynMempoolDom, DynSimpleDom>;
TYPED_TEST_SUITE(DocumentTest, DomTypes);

TYPED_TEST(DocumentTest, Parse) {
  using Document = TypeParam;
  using Check = bool (Document::*)() const;

  struct ParseTest {
    std::string data;
    Check check;
    bool has_error;
  };
  constexpr static bool kNoError = false;
  constexpr static bool kError = true;
  std::vector<ParseTest> tests = {
      // test valid json
      {"true", &Document::IsTrue, kNoError},
      {"false", &Document::IsFalse, kNoError},
      {"null", &Document::IsNull, kNoError},
      {"{}", &Document::IsObject, kNoError},
      {R"({"key":false})", &Document::IsObject, kNoError},
      {R"({"key":true})", &Document::IsObject, kNoError},
      {"[]", &Document::IsArray, kNoError},
      {"[null,null,null,true,true,false,false]", &Document::IsArray, kNoError},
      {"\"\"", &Document::IsString, kNoError},
      {"123", &Document::IsUint64, kNoError},
      {"-123", &Document::IsInt64, kNoError},
      {"0.000e0", &Document::IsDouble, kNoError},
      {"", &Document::IsNull, kError},
      {"1.", &Document::IsNull, kError},
      {"truef", &Document::IsNull, kError},
      {"true:", &Document::IsNull, kError},
      {"tru", &Document::IsNull, kError},
      {"alse", &Document::IsNull, kError},
      {"fals", &Document::IsNull, kError},
      {"nullnull", &Document::IsNull, kError},
      {"[fase0]", &Document::IsNull, kError},
      {"{\"\":true,}", &Document::IsNull, kError},
      {R"({"key":true,})", &Document::IsNull, kError},
      {"{:,}", &Document::IsNull, kError},
      {"[true:null]", &Document::IsNull, kError},
      {"{[]}", &Document::IsNull, kError},
      {"{[}]", &Document::IsNull, kError},
      {"[[[[[[", &Document::IsNull, kError},
      {"[NULL]", &Document::IsNull, kError},
      {R"([{"a":0})", &Document::IsNull, kError},
      {R"({"a":{"b":0})", &Document::IsNull, kError},
  };

  Document doc;
  for (auto& test : tests) {
    std::string error_info = "Error when parsing json: " + test.data + "\n";
    doc.Parse(test.data.c_str(), test.data.size());
    EXPECT_EQ(doc.HasParseError(), test.has_error) << error_info;
    EXPECT_TRUE((doc.*(test.check))()) << error_info;
  }
}

static std::string get_json(const std::string& file) {
  std::ifstream ifs;
  std::stringstream ss;
  ifs.open(file.data());
  ss << ifs.rdbuf();
  return ss.str();
}

static std::vector<std::string> get_all_jsons(const std::string& dirname) {
  DIR* dir = opendir(dirname.c_str());
  std::vector<std::string> jsons;
  EXPECT_TRUE(dir != nullptr) << "Error open directory: " + dirname;
  if (nullptr == dir) return jsons;
  struct dirent* ent = nullptr;
  while (nullptr != (ent = readdir(dir))) {
    struct stat st;
    std::string filename = dirname + ent->d_name;
    int ret = lstat(filename.c_str(), &st);
    if (ret == -1) {
      goto return_label;
    }
    if (S_ISDIR(st.st_mode)) {
      if (ent->d_name[0] == '.') {
        continue;
      }
      auto sub_jsons = get_all_jsons(filename + '/');
      jsons.insert(jsons.end(), sub_jsons.begin(), sub_jsons.end());
    } else {
      auto const pos = filename.find_last_of('.');
      const auto extension = filename.substr(pos + 1);
      if (extension == "json") {
        jsons.push_back(get_json(filename));
      }
    }
  }

return_label:
  closedir(dir);
  return jsons;
}

TYPED_TEST(DocumentTest, ParseFile) {
  using Document = TypeParam;
  auto jsons = get_all_jsons("./testdata/");
  for (const auto& json : jsons) {
    Document doc;
    doc.Parse(json);
    EXPECT_FALSE(doc.HasParseError());
  }
}

TYPED_TEST(DocumentTest, ParseOnDemandFile) {
  using Document = TypeParam;
  using CNode = DNode<SimpleAllocator>;

  static SimpleAllocator alloc_g;
  struct NodeWrapper {
    NodeWrapper() = default;
    NodeWrapper(CNode&& node) : node(std::move(node)){};
    NodeWrapper(const NodeWrapper& rhs) : node(CNode(rhs.node, alloc_g)) {}
    NodeWrapper(NodeWrapper&&) = default;
    ~NodeWrapper() = default;
    CNode node = {};
  };

  struct OnDemandParseTest {
    std::string json_file;
    JsonPointer path;
    NodeWrapper wnode;
  };

  auto jsons = get_all_jsons("./testdata/");
  std::vector<OnDemandParseTest> tests = {
      {"twitter", {"statuses", 0, "favorited"}, CNode(false)},
      {"twitter",
       {"statuses", 6, "id"},
       CNode(uint64_t(505874915338104800ULL))},
      {"twitter", {"search_metadata", "count"}, CNode(100)},
      {"twitter", {"search_metadata", 0}, CNode()},
      {"citm_catalog", {"events", "342742596", "id"}, CNode(342742596)},
  };

  for (const auto& t : tests) {
    Document doc;
    std::string json =
        get_json(std::string("./testdata/") + t.json_file + ".json");
    doc.ParseOnDemand(json, t.path);
    EXPECT_EQ(doc, t.wnode.node);
  }
}

TYPED_TEST(DocumentTest, ParseOnDemand) {
  using Document = TypeParam;
  using Path = JsonPointer;
  using Check = bool (Document::*)() const;
  struct OnDemandParseTest {
    std::string json;
    Path path;
    Check check;
    bool has_error;
    std::string Dump() const {
      // dump as the form of std::initializer_list
      std::string s = "Path {";
      for (const auto& i : path) {
        if (i.IsStr()) {
          s += '"' + i.GetStr() + "\",";
        } else {
          s += std::to_string(i.GetNum()) + ',';
        }
      }
      if (!path.empty()) {
        s.pop_back();
      }
      s += '}';
      return s + " Json: " + json;
    }
  };
  constexpr static bool kNoError = false;
  constexpr static bool kError = true;
  std::string json = this->data_;
  std::vector<OnDemandParseTest> tests = {
      // parse ondemand success
      {json, {}, &Document::IsObject, kNoError},
      {json, {"id"}, &Document::IsNumber, kNoError},
      {json, {"other"}, &Document::IsNull, kNoError},
      {json, {"extra"}, &Document::IsObject, kNoError},
      {json, {"ids"}, &Document::IsArray, kNoError},
      {json, {"author", "male"}, &Document::IsTrue, kNoError},
      {R"({"a":1,"b":{},"c":[]})", {"b"}, &Document::IsObject, kNoError},
      {json, {"ids", 1}, &Document::IsUint64, kNoError},
      {json, {"ids", 0}, &Document::IsInt64, kNoError},
      {json, {"titles", 0}, &Document::Empty, kNoError},
      {json, {"titles", 1}, &Document::IsString, kNoError},
      {json, {"hots", 2}, &Document::IsTrue, kNoError},
      {json, {"authors", 2, 0}, &Document::IsArray, kNoError},

      // parse ondemand failed
      {json, {"unknown"}, &Document::IsNull, kError},
      {json, {"author", "unknown"}, &Document::IsNull, kError},
      {json, {"ids", "name"}, &Document::IsNull, kError},
      {json, {"extra", "other"}, &Document::IsNull, kError},
      {json, {"authors", "name"}, &Document::IsNull, kError},
      {R"([{"a":1}])", {"a"}, &Document::IsNull, kError},
      {R"({"a":1,"b":{},"c":[]})", {"b", "c"}, &Document::IsNull, kError},
      {R"([] {"a":1})", {"a"}, &Document::IsNull, kError},
      {R"({a":1})", {"a"}, &Document::IsNull, kError},
      {R"({"a"})", {"a"}, &Document::IsNull, kError},
      {R"({"x":[[[]])", {"a"}, &Document::IsNull, kError},
      {R"({"x":{)", {"a"}, &Document::IsNull, kError},
      {json, {"authors", 2, 1}, &Document::IsNull, kError},
      {json, {"authors", 3}, &Document::IsNull, kError},
      {json, {"hots", 5}, &Document::IsNull, kError},
      {json, {"hots", "error"}, &Document::IsNull, kError},
      {json, {0, "hots"}, &Document::IsNull, kError},
  };
  Document doc;
  for (auto& test : tests) {
    std::string error_info = test.Dump();
    doc.ParseOnDemand(test.json.c_str(), test.json.size(),
                      JsonPointer(test.path));
    EXPECT_EQ(doc.HasParseError(), test.has_error) << error_info;
    EXPECT_TRUE((doc.*(test.check))()) << error_info;
  }
}

TYPED_TEST(DocumentTest, Move) {
  using Document = TypeParam;
  auto& alloc = this->doc_.GetAllocator();
  Document doc1(std::move(this->doc_));
  EXPECT_TRUE(doc1.IsObject());
  EXPECT_TRUE(doc1.GetAllocator() == alloc);

  Document doc2(std::move(doc1));
  EXPECT_TRUE(doc2.IsObject());
  EXPECT_TRUE(doc2.GetAllocator() == alloc);

  Document doc3;
  doc3 = std::move(doc2);
  EXPECT_TRUE(doc3.IsObject());
  EXPECT_TRUE(doc3.GetAllocator() == alloc);
}

TYPED_TEST(DocumentTest, CheckType) {
  EXPECT_TRUE(this->doc_.IsObject());
  EXPECT_TRUE(this->doc_["other"].IsNull());
  EXPECT_TRUE(this->doc_["title"].IsString());
  EXPECT_TRUE(this->doc_["id"].IsInt64());
  EXPECT_TRUE(this->doc_["id"].IsUint64());
  EXPECT_TRUE(this->doc_["ids"].IsArray());
}

TYPED_TEST(DocumentTest, Get) {
  EXPECT_EQ(this->doc_["id"].GetUint64(), 12125925);
  EXPECT_FLOAT_EQ(this->doc_["price"].GetDouble(), 345.67);
  EXPECT_EQ(this->doc_["hots"][2].GetBool(), true);
  EXPECT_EQ(this->doc_["id"].GetUint64(), 12125925);
  EXPECT_FLOAT_EQ(this->doc_["price"].GetDouble(), 345.67);
  EXPECT_FLOAT_EQ(this->doc_["prices"][0].GetDouble(), -0.1);
  EXPECT_EQ(this->doc_["hots"][2].GetBool(), true);
  EXPECT_EQ(this->doc_["title"].GetString(), "未来简史");
  EXPECT_TRUE(this->doc_.HasMember("other"));
  EXPECT_FALSE(this->doc_.HasMember("unknown name"));
  EXPECT_TRUE(this->doc_.FindMember("extra")->value.IsObject());
}

TYPED_TEST(DocumentTest, Length) {
  EXPECT_EQ(this->doc_["authors"].Size(), 3);
  EXPECT_EQ(this->doc_["weights"].Empty(), true);
  EXPECT_EQ(this->doc_["titles"][0].Empty(), true);
}

TYPED_TEST(DocumentTest, SerializeOK) {
  using Document = TypeParam;
  std::vector<std::string> vec = {
      // single values
      "1.0000000000000001e-60",
      "0",
      "[]",
      "{}",
      "true",
      "false",
      "null",
      R"("123, hi:{}, 中文 latin α 😁 escape \r\b\t\f\n\\\\\"\"")",
      // normal json
      R"({"id":12125925,"ids":[-2147483648,2147483647],"title":"未来简史","titles":["","world"],"price":345.67,"prices":[-0.1,0.1],"hot":true,"hots":[true,true,true],"author":{"name":"json","age":99,"male":true},"authors":[{"name":"json","age":99,"male":true},{"name":"json","age":99,"male":true},{"name":"json","age":99,"male":true}],"weights":[],"extra":{},"other":null})",
  };
  for (auto& v : vec) {
    Document doc;
    doc.Parse(v.c_str(), v.size());
    EXPECT_FALSE(doc.HasParseError());
    EXPECT_EQ(doc.GetErrorOffset(), v.size());

    WriteBuffer wb;
    auto err = doc.Serialize(wb);
    EXPECT_EQ(err, kErrorNone);
    EXPECT_STREQ(wb.ToString(), v.c_str());

    std::string json = doc.Dump();
    EXPECT_STREQ(json.c_str(), v.c_str());
  }
}

TYPED_TEST(DocumentTest, SerializeSort) {}

TYPED_TEST(DocumentTest, SonicErrorInvalidKey) {
  using DNode = typename TypeParam::NodeType;
  auto iter = this->doc_.MemberBegin();
  ((DNode*)(&(iter->name)))->SetNull();  // ill codes, just test.
  WriteBuffer wb;
  auto err = this->doc_.Serialize(wb);
  EXPECT_EQ(err, kSerErrorInvalidObjKey);
  EXPECT_TRUE(this->doc_.Dump().empty());
}

TYPED_TEST(DocumentTest, SonicErrorInfinity) {
  this->doc_["id"].SetDouble(std::numeric_limits<double>::infinity());
  WriteBuffer wb;
  auto err = this->doc_.Serialize(wb);
  EXPECT_EQ(err, kSerErrorInfinity);
  EXPECT_TRUE(this->doc_.Dump().empty());
}

TYPED_TEST(DocumentTest, swap) {
  using Document = TypeParam;
  Document doc1;
  this->Parse(doc1);
  Document doc2;
  EXPECT_FALSE(doc1.IsNull());
  EXPECT_TRUE(doc2.IsNull());
  doc2.Swap(doc1);
  EXPECT_FALSE(doc2.IsNull());
  EXPECT_TRUE(doc1.IsNull());
}

TYPED_TEST(DocumentTest, Iterator) {
  // test MemerberIterator
  {
    auto beg = this->doc_.MemberBegin();
    auto end = this->doc_.MemberEnd();
    EXPECT_EQ(end - beg, this->doc_.Size());
    EXPECT_TRUE(end != beg);
    EXPECT_FALSE(end == beg);
    EXPECT_TRUE(end > beg);
    EXPECT_TRUE(end >= beg);
    EXPECT_TRUE(beg < end);
    EXPECT_TRUE(beg <= end);

    auto end2 = beg - (-this->doc_.Size());
    auto beg2 = end + (-this->doc_.Size());
    EXPECT_EQ(end, end2);
    EXPECT_EQ(beg, beg2);
    // EXPECT_EQ(end.GetIdx(), this->doc_.Size());
    EXPECT_TRUE(beg[1].value.IsArray());  // ids":[-2147483648,2147483647]
    EXPECT_TRUE((++beg)->value.IsArray());
    EXPECT_TRUE((beg++)->value.IsArray());
    EXPECT_EQ(beg - 2, beg2);
    EXPECT_TRUE(end[-1].value.IsNull());  // "extra": null
    EXPECT_TRUE((--end)->value.IsNull());
    EXPECT_TRUE((end--)->value.IsNull());
    EXPECT_EQ(end + 2, end2);
  }

  // test ValueIterator
  auto& arr = this->doc_["ids"];
  {
    auto beg = arr.Begin();
    auto end = arr.End();
    auto size = arr.Size();
    EXPECT_EQ(end - beg, size);
    EXPECT_TRUE(end != beg);
    EXPECT_FALSE(end == beg);
    EXPECT_TRUE(end > beg);
    EXPECT_TRUE(end >= beg);
    EXPECT_TRUE(beg < end);
    EXPECT_TRUE(beg <= end);

    auto end2 = beg - (-size);
    auto beg2 = end + (-size);
    EXPECT_EQ(end, end2);
    EXPECT_EQ(beg, beg2);
    // EXPECT_EQ(end.GetIdx(), size);
    EXPECT_TRUE(beg[1].IsUint64());  // ids":[-2147483648,2147483647]
    EXPECT_TRUE((++beg)->IsUint64());
    EXPECT_TRUE((beg++)->IsUint64());
    EXPECT_EQ(beg - 2, beg2);
    // EXPECT_TRUE(end[-1].IsUint64());
    EXPECT_TRUE((--end)->IsUint64());
    EXPECT_TRUE((end--)->IsUint64());
    EXPECT_EQ(end + 2, end2);
  }

#if __cplusplus >= 201703L
  using NodeType = typename TypeParam::NodeType;
  auto& alloc = this->doc_.GetAllocator();
  arr.Clear();
  for (int i = 0; i < 10; i++)
    if (i % 2 == 0)
      arr.PushBack(NodeType(i), alloc);
    else
      arr.PushBack(NodeType(kNull), alloc);

  const NodeType null(kNull);
  arr.Erase(std::remove(arr.Begin(), arr.End(), null), arr.End());
  EXPECT_EQ(5u, arr.Size());
  for (int i = 0; i < 5; i++) EXPECT_EQ(i * 2, arr[i].GetInt64());
#endif
}

TYPED_TEST(DocumentTest, NodeCopyControl) {
  using Document = TypeParam;
  using NodeType = typename Document::NodeType;
  using Allocator = typename Document::Allocator;

  // test node assignment
  {  // case 1: assign another node
    auto snode = NodeType("new allocated", this->doc_.GetAllocator());
    EXPECT_EQ(snode.GetString(), "new allocated");
    this->doc_["title"] = std::move(snode);
    EXPECT_TRUE(snode.IsNull());
    EXPECT_EQ(this->doc_["title"].GetString(), "new allocated");
    EXPECT_FALSE(this->doc_["title"].IsStringConst());
  }

  {  // case 2: assign to dynamic node
    auto snode =
        NodeType("new allocated dynamic node", this->doc_.GetAllocator());
    this->doc_["titles"].PushBack(std::move(snode), this->doc_.GetAllocator());
    auto& dyn_node = this->doc_["titles"].End()[-1];
    EXPECT_FALSE(dyn_node.IsStringConst());
    auto snode2 =
        NodeType("new allocated in sub-node", this->doc_.GetAllocator());
    dyn_node = std::move(snode2);
    EXPECT_EQ(dyn_node.GetString(), "new allocated in sub-node");
  }

  {  // case 3: assign the subnode
    this->doc_["titles"].PushBack(
        NodeType("new allocated in sub-node", this->doc_.GetAllocator()),
        this->doc_.GetAllocator());
    EXPECT_FALSE(this->doc_["titles"].End()[-1].IsStringConst());
    this->doc_["titles"] = std::move(this->doc_["titles"].End()[-1]);
    EXPECT_FALSE(this->doc_["titles"].IsStringConst());
    EXPECT_EQ(this->doc_["titles"].GetString(), "new allocated in sub-node");
  }

  // test copy constructor with allocator
  {
    Allocator alloc;
    Document* doc = new Document;
    doc->Parse(this->data_.c_str(), this->data_.size());
    NodeType new_node((*doc)["title"], alloc);
    delete doc;
    EXPECT_EQ(new_node.GetString(), "未来简史");
  }

  // test move constructor
  NodeType new_node(std::move(this->doc_["titles"]));
  EXPECT_FALSE(new_node.IsStringConst());
  EXPECT_EQ(new_node.GetString(), "new allocated in sub-node");
  EXPECT_TRUE(this->doc_["titles"].IsNull());

  // test swap
  NodeType swaped;
  EXPECT_TRUE(swaped.IsNull());
  swaped.Swap(new_node);
  EXPECT_FALSE(swaped.IsStringConst());
  EXPECT_TRUE(new_node.IsNull());
}

TYPED_TEST(DocumentTest, DomCopyControl) {
  using Document = TypeParam;
  using Allocator = typename Document::Allocator;

  // test document move constucor
  Document copied(std::move(this->doc_));
  EXPECT_TRUE(copied.IsObject());

  // test document move assignment, different allocator
  {
    Document rhs;
    rhs.Parse(this->data_.c_str(), this->data_.size());
    EXPECT_FALSE(rhs.HasParseError());
    Document lhs;
    lhs.Parse(this->data_.c_str(), this->data_.size());
    EXPECT_FALSE(lhs.HasParseError());
    lhs = std::move(rhs);
  }
  // test document move assignment, sharing allocator
  {
    Allocator alloc;
    Document rhs(&alloc);
    rhs.Parse(this->data_.c_str(), this->data_.size());
    EXPECT_FALSE(rhs.HasParseError());
    Document lhs(&alloc);
    lhs.Parse(this->data_.c_str(), this->data_.size());
    EXPECT_FALSE(lhs.HasParseError());
    lhs = std::move(rhs);
  }
  // test document move assignment, sharing allocator
  {
    Document rhs;
    rhs.Parse(this->data_.c_str(), this->data_.size());
    EXPECT_FALSE(rhs.HasParseError());
    Document lhs(&(rhs.GetAllocator()));
    lhs.Parse(this->data_.c_str(), this->data_.size());
    EXPECT_FALSE(lhs.HasParseError());
    lhs = std::move(rhs);
  }

  // test document swap, different allocator
  {
    Document none;
    EXPECT_TRUE(none.IsNull());
    Document rhs;
    rhs.Parse(this->data_.c_str(), this->data_.size());
    none.Swap(rhs);
    EXPECT_TRUE(none.IsObject());
    EXPECT_TRUE(rhs.IsNull());
  }
  // test document swap, sharing allocator
  {
    Document none;
    EXPECT_TRUE(none.IsNull());
    Document rhs(&(none.GetAllocator()));
    rhs.Parse(this->data_.c_str(), this->data_.size());
    none.Swap(rhs);
    EXPECT_TRUE(none.IsObject());
    EXPECT_TRUE(rhs.IsNull());
  }
}

TYPED_TEST(DocumentTest, ArrayPushPop) {
  using Document = TypeParam;
  using NodeType = typename Document::NodeType;

  this->doc_["weights"].PushBack(NodeType(kArray),
                                 this->doc_.GetAllocator());  // "weights":[]
  NodeType* dnode = &(this->doc_["weights"][0]);              // dynamic node
  NodeType* snode = &(this->doc_["authors"]);                 // static node
  std::vector<NodeType*> arr_nodes{dnode, snode};

  for (auto np : arr_nodes) {
    NodeType& arr = *np;
    if (!arr.Empty()) {
      arr.Erase(arr.Begin(), arr.End());
    }
    for (int i = 0; i < 10; i++) {
      arr.PushBack(NodeType(1), this->doc_.GetAllocator());
      arr.PopBack();
    }
    EXPECT_TRUE(arr.Empty());

    for (int i = 0; i < 10; i++) {
      arr.PushBack(NodeType(1), this->doc_.GetAllocator());
    }
    for (int i = 0; i < 10; i++) {
      arr.PopBack();
    }
    EXPECT_TRUE(arr.Empty());

    for (int i = 0; i < 10; i++) {
      arr.PushBack(NodeType(1), this->doc_.GetAllocator());
    }
    for (int i = 0; i < 10; i++) {
      arr.Erase(arr.Begin());
    }
    EXPECT_TRUE(arr.Empty());

    for (int i = 0; i < 10; i++) {
      arr.PushBack(NodeType(1), this->doc_.GetAllocator());
    }
    for (int i = 9; i >= 0; i--) {
      arr.Erase(arr.Begin() + i / 2);
    }
    EXPECT_TRUE(arr.Empty());

    for (int i = 0; i < 10; i++) {
      arr.PushBack(NodeType(1), this->doc_.GetAllocator());
    }
    arr.Clear();
    EXPECT_TRUE(arr.Empty());
  }
}

TYPED_TEST(DocumentTest, ObjAddRemove) {
  using Document = TypeParam;
  using NodeType = typename Document::NodeType;

  this->doc_["weights"].PushBack(NodeType(kObject),
                                 this->doc_.GetAllocator());  // "weights":[]
  NodeType* dnode = &(this->doc_["weights"][0]);              // dynamic node
  NodeType* snode = &(this->doc_["author"]);                  // static node
  std::vector<NodeType*> obj_nodes{dnode, snode};
  WriteBuffer wb;

  // test AddMember/RemoveMember
  for (auto np : obj_nodes) {
    NodeType& obj = *np;
    if (!obj.Empty()) {
      obj.Clear();
    }
    EXPECT_TRUE(obj.Empty());
    for (int i = 0; i < 10; i++) {
      std::string new_key = "new key" + std::to_string(i);
      obj.AddMember(new_key, NodeType(i), this->doc_.GetAllocator());
      obj.RemoveMember(new_key);
    }
    EXPECT_TRUE(obj.Empty());

    char buf[2000] = {'{', 0};
    std::vector<size_t> commas;
    char* p = buf + 1;
    for (int i = 0; i < 10; i++) {
      std::string kv =
          "\"new key" + std::to_string(i) + "\":" + std::to_string(i) + ",";
      std::memcpy(p, kv.c_str(), kv.size());
      p += kv.size();
      commas.push_back(p - buf - 1);
    }

    for (int i = 0; i < 10; i++) {
      std::string new_key = "new key" + std::to_string(i);
      obj.AddMember(new_key, NodeType(i), this->doc_.GetAllocator());
      EXPECT_EQ(obj[new_key].GetInt64(), i);
      obj.Serialize(wb);
      buf[commas[i]] = '}';
      EXPECT_STREQ(wb.ToString(), std::string(buf, commas[i] + 1).c_str());
      buf[commas[i]] = ',';
    }
    buf[commas[9]] = '}';

    for (int i = 0; i < 10; i++) {
      std::string new_key = "new key" + std::to_string(i);
      EXPECT_TRUE(obj.FindMember(new_key) != obj.MemberEnd());
      obj.RemoveMember(new_key);
      EXPECT_TRUE(obj.FindMember(new_key) == obj.MemberEnd());
      obj.Serialize(wb);
      buf[commas[i]] = '{';
    }

    buf[commas[9] - 1] = '{';
    buf[commas[9]] = '}';
    EXPECT_STREQ(wb.ToString(), "{}");

    for (int i = 0; i < 10; i++) {
      std::string new_key = "new key" + std::to_string(i);
      obj.AddMember(new_key, NodeType(i), this->doc_.GetAllocator());
    }
    obj.Clear();
    obj.Serialize(wb);
    EXPECT_STREQ(wb.ToString(), "{}");
  }
}

TYPED_TEST(DocumentTest, CopyFrom) {
  using Document = TypeParam;
  using NodeType = typename Document::NodeType;
  using Allocator = typename Document::Allocator;

  this->doc_["weights"].PushBack(NodeType(kObject),
                                 this->doc_.GetAllocator());  // "weights":[]
  NodeType* dnode = &(this->doc_["weights"][0]);              // dynamic node
  NodeType* snode = &(this->doc_["author"]);                  // static node
  std::vector<NodeType*> nodes{dnode, snode};

  Allocator& a = this->doc_.GetAllocator();
  NodeType rhs(1.23);
  for (auto np : nodes) {
    NodeType& node = *np;
    // Copy primited type
    node.CopyFrom(rhs, a);
    EXPECT_TRUE(node == rhs);

    // Doesn't copy string
    rhs.SetString("Hello");
    node.CopyFrom(rhs, a);
    EXPECT_EQ(node, rhs);
    EXPECT_EQ(rhs.GetStringView().data(), node.GetStringView().data());
    EXPECT_EQ(rhs.GetString(), node.GetString());

    // Copy String
    rhs.SetString("Hello");
    node.CopyFrom(rhs, a, true);
    EXPECT_TRUE(node == rhs);
    EXPECT_EQ(rhs.GetString(), node.GetString());
    EXPECT_NE(rhs.GetStringView().data(), node.GetStringView().data());

    // Copy String always if src node is CopiedString
    rhs.SetString("Hello", a);
    node.CopyFrom(rhs, a);
    EXPECT_TRUE(node == rhs);
    EXPECT_EQ(rhs.GetString(), node.GetString());
    EXPECT_NE(rhs.GetStringView().data(), node.GetStringView().data());

    // Copy array
    rhs.SetArray().PushBack(NodeType(1), a);
    rhs.PushBack(NodeType(2), a);
    node.CopyFrom(rhs, a);
    EXPECT_TRUE(node == rhs);

    // Copy Object
    rhs.SetObject().AddMember("key1", NodeType("string"), a, false);
    rhs.AddMember("key2", NodeType(1.23), a, false);
    rhs.AddMember("key3", NodeType(true), a, false);
    node.CopyFrom(rhs, a);
    EXPECT_TRUE(node == rhs);
  }
}

}  // unnamed namespace
