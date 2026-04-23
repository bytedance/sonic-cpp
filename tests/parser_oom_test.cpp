/*
 * Copyright ByteDance Inc.
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

#include <algorithm>
#include <atomic>
#include <cstring>
#include <string>
#include <vector>

#include "sonic/dom/handler.h"
#include "sonic/dom/parser.h"
#include "sonic/sonic.h"

namespace {

using namespace sonic_json;

TEST(Document, VersionStringExpandsMacros) {
  const std::string v = SONIC_VERSION_STRING;
  EXPECT_EQ(std::string::npos, v.find("SONIC_MAJOR_VERSION"));
  EXPECT_EQ(std::string::npos, v.find("SONIC_PATCH"));
  EXPECT_EQ(2u, std::count(v.begin(), v.end(), '.'));
  for (char c : v) {
    EXPECT_TRUE(c == '.' || (c >= '0' && c <= '9'));
  }
}

TEST(Document, RejectInvalidSurrogate) {
  {
    Document doc;
    std::string json = "\"\\uDC00\"";
    doc.Parse(json);
    EXPECT_TRUE(doc.HasParseError());
  }
  {
    Document doc;
    std::string json = "\"\\uD800\\u0041\"";
    doc.Parse(json);
    EXPECT_TRUE(doc.HasParseError());
  }
  {
    Document doc;
    std::string json = "\"\\uDFFF\"";
    doc.Parse(json);
    EXPECT_TRUE(doc.HasParseError());
  }
  {
    Document doc;
    std::string json = "\"\\uD83D\\uDE0A\"";
    doc.Parse(json);
    EXPECT_FALSE(doc.HasParseError());
  }
}

TEST(Document, ReparseDoesNotLeakPoolMemory) {
  Document doc;
  doc.Parse(R"({"key":"value","num":42})");
  ASSERT_FALSE(doc.HasParseError());
  size_t size_after_first = doc.GetAllocator().Size();

  doc.Parse(R"({"key":"value","num":42})");
  ASSERT_FALSE(doc.HasParseError());
  size_t size_after_second = doc.GetAllocator().Size();

  EXPECT_EQ(size_after_first, size_after_second);
}

struct AlwaysOomAllocator {
  void* Malloc(size_t) { return nullptr; }
  void* Realloc(void*, size_t, size_t) { return nullptr; }
  static void Free(void*) {}
  static constexpr bool kNeedFree = false;
};

static std::vector<uint8_t> pad_json_bytes(const char* json, size_t len) {
  std::vector<uint8_t> buf(len + 64, 0);
  std::memcpy(buf.data(), json, len);
  buf[len] = 'x';
  buf[len + 1] = '"';
  buf[len + 2] = 'x';
  return buf;
}

TEST(Document, OomDoesNotCrashPushBack) {
  AlwaysOomAllocator alloc;
  DNode<AlwaysOomAllocator> arr;
  arr.SetArray();
  DNode<AlwaysOomAllocator> val;
  val.SetInt64(42);
  arr.PushBack(std::move(val), alloc);
  EXPECT_EQ(0u, arr.Size());
}

TEST(Document, NoFreeAllocatorWithoutClearCompilesAndRuns) {
  GenericDocument<DNode<AlwaysOomAllocator>> doc;
  doc.Parse("{}");
  EXPECT_TRUE(doc.HasParseError());
}

TEST(Document, CreateMapOomFromEmptyObjectReturnsFalse) {
  AlwaysOomAllocator alloc;
  DNode<AlwaysOomAllocator> obj;
  obj.SetObject();
  EXPECT_FALSE(obj.CreateMap(alloc));
}

TEST(Document, OomDoesNotCrashCopyObject) {
  Document src;
  src.Parse(R"({"a":1,"b":2,"c":3})");
  ASSERT_FALSE(src.HasParseError());

  AlwaysOomAllocator alloc;
  DNode<AlwaysOomAllocator> dst(src, alloc);
  EXPECT_TRUE(dst.IsObject());
  EXPECT_EQ(0u, dst.Size());
}

TEST(Document, OomDoesNotCrashCopyArray) {
  Document src;
  src.Parse(R"([1, 2, 3])");
  ASSERT_FALSE(src.HasParseError());

  AlwaysOomAllocator alloc;
  DNode<AlwaysOomAllocator> dst(src, alloc);
  EXPECT_TRUE(dst.IsArray());
  EXPECT_EQ(0u, dst.Size());
}

struct OomAfterNthAllocator {
  size_t remaining = 0;
  OomAfterNthAllocator() = default;
  explicit OomAfterNthAllocator(size_t n) : remaining(n) {}
  void* Malloc(size_t n) {
    if (remaining == 0) return nullptr;
    --remaining;
    return std::malloc(n);
  }
  void* Realloc(void* p, size_t, size_t new_size) {
    if (new_size == 0) {
      std::free(p);
      return nullptr;
    }
    if (remaining == 0) return nullptr;
    if (p == nullptr) --remaining;
    return std::realloc(p, new_size);
  }
  static void Free(void* p) { std::free(p); }
  static constexpr bool kNeedFree = true;
};

TEST(Document, CreateMapOomForMapStorageReturnsFalse) {
  OomAfterNthAllocator alloc(1);
  DNode<OomAfterNthAllocator> obj;
  obj.SetObject();
  EXPECT_FALSE(obj.CreateMap(alloc));
}

TEST(Document, OomDoesNotCrashParseObject) {
  OomAfterNthAllocator alloc(1);
  GenericDocument<DNode<OomAfterNthAllocator>> doc(&alloc);
  doc.Parse(R"({"a":1,"b":2,"c":3})");
  EXPECT_TRUE(doc.HasParseError());
}

TEST(Document, OomDoesNotCrashParseArray) {
  OomAfterNthAllocator alloc(1);
  GenericDocument<DNode<OomAfterNthAllocator>> doc(&alloc);
  doc.Parse("[1,2,3]");
  EXPECT_TRUE(doc.HasParseError());
}

TEST(Document, ParseImplHandlesRepeatedOomCleanly) {
  OomAfterNthAllocator alloc(0);
  GenericDocument<DNode<OomAfterNthAllocator>> doc(&alloc);
  doc.Parse("{}");
  EXPECT_TRUE(doc.HasParseError());
  EXPECT_EQ(doc.GetParseError(), kErrorNoMem);
  doc.Parse("[]");
  EXPECT_TRUE(doc.HasParseError());
  EXPECT_EQ(doc.GetParseError(), kErrorNoMem);
}

struct SentinelTrackingAllocator {
  static int balance;
  void* Malloc(size_t n) {
    ++balance;
    void* p = std::malloc(n);
    if (p) std::memset(p, '"', n);
    return p;
  }
  void* Realloc(void* p, size_t, size_t n) { return std::realloc(p, n); }
  static void Free(void* p) {
    if (p) --balance;
    std::free(p);
  }
};
int SentinelTrackingAllocator::balance = 0;

struct RejectKeyLazySAX {
  using Allocator = SentinelTrackingAllocator;
  SentinelTrackingAllocator alloc;
  bool key_called = false;
  Allocator& GetAllocator() { return alloc; }
  bool StartObject() { return true; }
  bool EndObject(size_t) { return true; }
  bool StartArray() { return true; }
  bool EndArray(size_t) { return true; }
  bool Key(const char*, size_t, size_t) {
    key_called = true;
    return false;
  }
  bool Raw(const char*, size_t) { return true; }
};

TEST(Document, ParseLazyEscapedKeyOomReportsNoMem) {
  struct OomLazySAX {
    using Allocator = OomAfterNthAllocator;
    OomAfterNthAllocator alloc;
    OomLazySAX() : alloc(0) {}
    Allocator& GetAllocator() { return alloc; }
    bool StartObject() { return true; }
    bool EndObject(size_t) { return true; }
    bool StartArray() { return true; }
    bool EndArray(size_t) { return true; }
    bool Key(const char*, size_t, size_t) { return true; }
    bool Raw(const char*, size_t) { return true; }
  };
  OomLazySAX sax;
  Parser<ParseFlags::kParseDefault> p;
  const char* json = R"({"\n": 1})";
  auto buf = pad_json_bytes(json, std::strlen(json));
  auto res = p.ParseLazy(buf.data(), std::strlen(json), sax);
  EXPECT_EQ(kErrorNoMem, res.Error());
}

struct TrackingNthOomAllocator {
  static int balance;
  static size_t remaining;
  void* Malloc(size_t n) {
    if (remaining == 0) return nullptr;
    --remaining;
    ++balance;
    return std::malloc(n);
  }
  void* Realloc(void* p, size_t, size_t n) { return std::realloc(p, n); }
  static void Free(void* p) {
    if (p) --balance;
    std::free(p);
  }
  static constexpr bool kNeedFree = true;
};
int TrackingNthOomAllocator::balance = 0;
size_t TrackingNthOomAllocator::remaining = 0;

TEST(Document, AddMemberWithoutMapOnOomLeavesObjectEmpty) {
  OomAfterNthAllocator alloc(0);
  DNode<OomAfterNthAllocator> obj;
  obj.SetObject();
  DNode<OomAfterNthAllocator> val;
  val.SetInt64(1);
  obj.AddMember("k", std::move(val), alloc);
  EXPECT_EQ(0u, obj.Size());
}

TEST(LazySAXHandler, EndObjectOomLeavesStackMatchingSuccessArm) {
  TrackingNthOomAllocator::balance = 0;
  TrackingNthOomAllocator::remaining = 1;
  TrackingNthOomAllocator alloc;

  using Node = DNode<TrackingNthOomAllocator>;
  LazySAXHandler<Node> sax(alloc);

  ASSERT_TRUE(sax.StartObject());
  constexpr char kKey[] = "key";
  void* buf = alloc.Malloc(sizeof(kKey));
  ASSERT_NE(nullptr, buf);
  std::memcpy(buf, kKey, sizeof(kKey));
  ASSERT_TRUE(sax.Key(static_cast<const char*>(buf), sizeof(kKey) - 1, 1));
  ASSERT_TRUE(sax.Raw("1", 1));
  ASSERT_TRUE(sax.EndObject(1));
  EXPECT_TRUE(sax.oom_);
  EXPECT_EQ(sizeof(Node), sax.stack_.Size());
}

TEST(Document, ParseLazyFreesEscapedKeyOnKeyFailure) {
  SentinelTrackingAllocator::balance = 0;

  RejectKeyLazySAX sax;
  Parser<ParseFlags::kParseDefault> p;
  const char* json = R"({"\n": 1})";
  auto buf = pad_json_bytes(json, std::strlen(json));
  p.ParseLazy(buf.data(), std::strlen(json), sax);

  ASSERT_TRUE(sax.key_called);
  EXPECT_EQ(0, SentinelTrackingAllocator::balance);
}

struct RejectingSAX {
  bool reject_start_array = false;
  bool reject_start_object = false;
  bool reject_end_array = false;
  bool reject_end_object = false;
  bool reject_key = false;
  bool reject_string = false;
  bool reject_int = false;
  bool reject_uint = false;
  bool reject_double = false;
  bool reject_numstr = false;
  bool reject_raw = false;
  bool reject_null = false;
  bool reject_bool = false;

  bool Null() { return !reject_null; }
  bool Bool(bool) { return !reject_bool; }
  bool Int(int64_t) { return !reject_int; }
  bool Uint(uint64_t) { return !reject_uint; }
  bool Double(double) { return !reject_double; }
  bool NumStr(StringView) { return !reject_numstr; }
  bool Raw(const char*, size_t) { return !reject_raw; }
  bool Key(StringView) { return !reject_key; }
  bool String(StringView) { return !reject_string; }
  bool StartArray() { return !reject_start_array; }
  bool EndArray(uint32_t) { return !reject_end_array; }
  bool StartObject() { return !reject_start_object; }
  bool EndObject(uint32_t) { return !reject_end_object; }
};

static std::vector<char> pad_json_for_parser(const char* json, size_t len) {
  std::vector<char> buf(len + 64, 0);
  std::memcpy(buf.data(), json, len);
  buf[len] = 'x';
  buf[len + 1] = '"';
  buf[len + 2] = 'x';
  return buf;
}

TEST(Parser, StartArrayFalseAbortsParse) {
  const char* json = "[1,2,3]";
  auto buf = pad_json_for_parser(json, std::strlen(json));
  RejectingSAX sax;
  sax.reject_start_array = true;
  Parser<ParseFlags::kParseDefault> p;
  auto res = p.Parse(buf.data(), std::strlen(json), sax);
  EXPECT_EQ(kSaxTermination, res.Error());
}

TEST(Parser, StartObjectFalseAbortsParse) {
  const char* json = R"({"a":1})";
  auto buf = pad_json_for_parser(json, std::strlen(json));
  RejectingSAX sax;
  sax.reject_start_object = true;
  Parser<ParseFlags::kParseDefault> p;
  auto res = p.Parse(buf.data(), std::strlen(json), sax);
  EXPECT_EQ(kSaxTermination, res.Error());
}

TEST(Parser, EndArrayFalseAbortsParse) {
  const char* json = "[1,2,3]";
  auto buf = pad_json_for_parser(json, std::strlen(json));
  RejectingSAX sax;
  sax.reject_end_array = true;
  Parser<ParseFlags::kParseDefault> p;
  auto res = p.Parse(buf.data(), std::strlen(json), sax);
  EXPECT_EQ(kSaxTermination, res.Error());
}

TEST(Parser, EndObjectFalseAbortsParse) {
  const char* json = R"({"a":1})";
  auto buf = pad_json_for_parser(json, std::strlen(json));
  RejectingSAX sax;
  sax.reject_end_object = true;
  Parser<ParseFlags::kParseDefault> p;
  auto res = p.Parse(buf.data(), std::strlen(json), sax);
  EXPECT_EQ(kSaxTermination, res.Error());
}

TEST(Parser, KeyFalseAbortsParseWhenNotCheckKeyReturn) {
  const char* json = R"({"a":1})";
  auto buf = pad_json_for_parser(json, std::strlen(json));
  RejectingSAX sax;
  sax.reject_key = true;
  Parser<ParseFlags::kParseDefault> p;
  auto res = p.Parse(buf.data(), std::strlen(json), sax);
  EXPECT_EQ(kSaxTermination, res.Error());
}

TEST(Parser, StringFalseAbortsParse) {
  const char* json = R"(["x"])";
  auto buf = pad_json_for_parser(json, std::strlen(json));
  RejectingSAX sax;
  sax.reject_string = true;
  Parser<ParseFlags::kParseDefault> p;
  auto res = p.Parse(buf.data(), std::strlen(json), sax);
  EXPECT_EQ(kSaxTermination, res.Error());
}

TEST(Parser, NumberIntRejectionReportsSaxTermination) {
  const char* json = "1";
  auto buf = pad_json_for_parser(json, std::strlen(json));
  RejectingSAX sax;
  sax.reject_uint = true;
  Parser<ParseFlags::kParseDefault> p;
  auto res = p.Parse(buf.data(), std::strlen(json), sax);
  EXPECT_EQ(kSaxTermination, res.Error());
}

TEST(Parser, NumberNegativeIntRejectionReportsSaxTermination) {
  const char* json = "-1";
  auto buf = pad_json_for_parser(json, std::strlen(json));
  RejectingSAX sax;
  sax.reject_int = true;
  Parser<ParseFlags::kParseDefault> p;
  auto res = p.Parse(buf.data(), std::strlen(json), sax);
  EXPECT_EQ(kSaxTermination, res.Error());
}

TEST(Parser, NumberDoubleRejectionReportsSaxTermination) {
  const char* json = "1.5";
  auto buf = pad_json_for_parser(json, std::strlen(json));
  RejectingSAX sax;
  sax.reject_double = true;
  Parser<ParseFlags::kParseDefault> p;
  auto res = p.Parse(buf.data(), std::strlen(json), sax);
  EXPECT_EQ(kSaxTermination, res.Error());
}

TEST(Parser, NumberIntegerAsRawRejectionReportsSaxTermination) {
  const char* json = "123";
  auto buf = pad_json_for_parser(json, std::strlen(json));
  RejectingSAX sax;
  sax.reject_raw = true;
  Parser<ParseFlags::kParseIntegerAsRaw> p;
  auto res = p.Parse(buf.data(), std::strlen(json), sax);
  EXPECT_EQ(kSaxTermination, res.Error());
}

TEST(Parser, NestedNumberRejectionReportsSaxTermination) {
  struct Case {
    const char* json;
    bool reject_double;
  } cases[] = {
      {R"({"a":1})", false},
      {R"([1])", false},
      {R"({"a":1.5})", true},
  };
  for (const auto& c : cases) {
    auto buf = pad_json_for_parser(c.json, std::strlen(c.json));
    RejectingSAX sax;
    if (c.reject_double) {
      sax.reject_double = true;
    } else {
      sax.reject_uint = true;
    }
    Parser<ParseFlags::kParseDefault> p;
    auto res = p.Parse(buf.data(), std::strlen(c.json), sax);
    EXPECT_EQ(kSaxTermination, res.Error());
  }
}

TEST(Parser, NumberOverflowAsNumStrRejectionReportsSaxTermination) {
  const char* json = "18446744073709551616";
  auto buf = pad_json_for_parser(json, std::strlen(json));
  RejectingSAX sax;
  sax.reject_numstr = true;
  Parser<ParseFlags::kParseOverflowNumAsNumStr> p;
  auto res = p.Parse(buf.data(), std::strlen(json), sax);
  EXPECT_EQ(kSaxTermination, res.Error());
}

TEST(Parser, NullRejectionReportsSaxTermination) {
  const char* json = "null";
  auto buf = pad_json_for_parser(json, std::strlen(json));
  RejectingSAX sax;
  sax.reject_null = true;
  Parser<ParseFlags::kParseDefault> p;
  auto res = p.Parse(buf.data(), std::strlen(json), sax);
  EXPECT_EQ(kSaxTermination, res.Error());
}

TEST(Parser, BoolRejectionReportsSaxTermination) {
  for (const char* json : {"true", "false"}) {
    auto buf = pad_json_for_parser(json, std::strlen(json));
    RejectingSAX sax;
    sax.reject_bool = true;
    Parser<ParseFlags::kParseDefault> p;
    auto res = p.Parse(buf.data(), std::strlen(json), sax);
    EXPECT_EQ(kSaxTermination, res.Error());
  }
}

TEST(Parser, PrimitiveRootStringRejectionReportsSaxTermination) {
  const char* json = R"("x")";
  auto buf = pad_json_for_parser(json, std::strlen(json));
  RejectingSAX sax;
  sax.reject_string = true;
  Parser<ParseFlags::kParseDefault> p;
  auto res = p.Parse(buf.data(), std::strlen(json), sax);
  EXPECT_EQ(kSaxTermination, res.Error());
}

struct SkippingKeyCheckReturnSAX {
  static constexpr bool check_key_return = true;
  int keys_seen = 0;
  bool Null() { return true; }
  bool Bool(bool) { return true; }
  bool Int(int64_t) { return true; }
  bool Uint(uint64_t) { return true; }
  bool Double(double) { return true; }
  bool NumStr(StringView) { return true; }
  bool Key(StringView) {
    ++keys_seen;
    return false;
  }
  bool String(StringView) { return true; }
  bool StartArray() { return true; }
  bool EndArray(uint32_t) { return true; }
  bool StartObject() { return true; }
  bool EndObject(uint32_t) { return true; }
};

TEST(Parser, KeyFalsePreservesSkipSemanticsUnderCheckKeyReturn) {
  const char* json = R"({"a":1,"b":2})";
  auto buf = pad_json_for_parser(json, std::strlen(json));
  SkippingKeyCheckReturnSAX sax;
  Parser<ParseFlags::kParseDefault> p;
  auto res = p.Parse(buf.data(), std::strlen(json), sax);
  EXPECT_EQ(kErrorNone, res.Error());
  EXPECT_EQ(2, sax.keys_seen);
}

struct RejectAllLazySax {
  using Allocator = SONIC_DEFAULT_ALLOCATOR;
  Allocator alloc_;
  Allocator& GetAllocator() { return alloc_; }
  bool StartArray() { return false; }
  bool EndArray(size_t) { return false; }
  bool StartObject() { return false; }
  bool EndObject(size_t) { return false; }
  bool Key(const char*, size_t, size_t) { return false; }
  bool Raw(const char*, size_t) { return false; }
};

TEST(ParseLazy, RawRejectionReportsSaxTermination) {
  RejectAllLazySax sax;
  Parser<ParseFlags::kParseDefault> p;
  const char* j = "42";
  auto buf = pad_json_bytes(j, 2);
  auto r = p.ParseLazy(buf.data(), 2, sax);
  EXPECT_EQ(r.Error(), kSaxTermination);
}

TEST(ParseLazy, StartArrayRejectionReportsSaxTermination) {
  RejectAllLazySax sax;
  Parser<ParseFlags::kParseDefault> p;
  const char* j = "[1,2,3]";
  auto buf = pad_json_bytes(j, 7);
  auto r = p.ParseLazy(buf.data(), 7, sax);
  EXPECT_EQ(r.Error(), kSaxTermination);
}

TEST(ParseLazy, StartObjectRejectionReportsSaxTermination) {
  RejectAllLazySax sax;
  Parser<ParseFlags::kParseDefault> p;
  const char* j = R"({"k":1})";
  auto buf = pad_json_bytes(j, 7);
  auto r = p.ParseLazy(buf.data(), 7, sax);
  EXPECT_EQ(r.Error(), kSaxTermination);
}

struct AcceptAllLazySax {
  using Allocator = SONIC_DEFAULT_ALLOCATOR;
  Allocator alloc_;
  Allocator& GetAllocator() { return alloc_; }
  bool StartArray() { return true; }
  bool EndArray(size_t) { return true; }
  bool StartObject() { return true; }
  bool EndObject(size_t) { return true; }
  bool Key(const char*, size_t, size_t) { return true; }
  bool Raw(const char*, size_t) { return true; }
};

TEST(ParseLazy, AcceptAllStillCompletesCleanly) {
  AcceptAllLazySax sax;
  Parser<ParseFlags::kParseDefault> p;
  const char* j = R"({"a":1,"b":[2,3]})";
  auto buf = pad_json_bytes(j, std::strlen(j));
  auto r = p.ParseLazy(buf.data(), std::strlen(j), sax);
  EXPECT_EQ(r.Error(), kErrorNone);
}

struct StringKeyCountingSAX {
  int string_calls = 0;
  int key_calls = 0;
  bool Null() { return true; }
  bool Bool(bool) { return true; }
  bool Int(int64_t) { return true; }
  bool Uint(uint64_t) { return true; }
  bool Double(double) { return true; }
  bool NumStr(StringView) { return true; }
  bool Raw(const char*, size_t) { return true; }
  bool Key(StringView) {
    ++key_calls;
    return true;
  }
  bool String(StringView) {
    ++string_calls;
    return true;
  }
  bool StartArray() { return true; }
  bool EndArray(uint32_t) { return true; }
  bool StartObject() { return true; }
  bool EndObject(uint32_t) { return true; }
};

TEST(Parser, InvalidSurrogateInValueDoesNotInvokeStringCallback) {
  const char* json = R"(["\uDC00"])";
  auto buf = pad_json_for_parser(json, std::strlen(json));
  StringKeyCountingSAX sax;
  Parser<ParseFlags::kParseDefault> p;
  auto res = p.Parse(buf.data(), std::strlen(json), sax);
  EXPECT_TRUE(res.Error() != kErrorNone);
  EXPECT_EQ(0, sax.string_calls);
}

TEST(Parser, InvalidSurrogateInKeyDoesNotInvokeKeyCallback) {
  const char* json = R"({"\uDC00":1})";
  auto buf = pad_json_for_parser(json, std::strlen(json));
  StringKeyCountingSAX sax;
  Parser<ParseFlags::kParseDefault> p;
  auto res = p.Parse(buf.data(), std::strlen(json), sax);
  EXPECT_TRUE(res.Error() != kErrorNone);
  EXPECT_EQ(0, sax.key_calls);
}

}  // namespace
