#pragma once

#include <rapidjson/document.h>
#include <rapidjson/internal/stack.h>

#include <string_view>
#include <vector>

struct GetUintHandler {
  using SizeType = size_t;

  const std::vector<std::string_view>& path_;
  size_t level_{0};
  uint value_{0};

  GetUintHandler(const std::vector<std::string_view>& path) : path_(path) {}

  bool terminated() const { return level_ == path_.size(); }

  bool Uint(unsigned i) {
    if (terminated()) {
      value_ = i;
      return false;
    }
    return true;
  }

  // check whether the path is finished
  bool Key(const char* key, SizeType length, bool copy) {
    if (terminated()) return false;
    auto target = path_[level_];
    // match the key
    if (length == target.size() &&
        std::memcmp(target.data(), key, length) == 0) {
      level_++;
    }
    return true;
  }

  bool StartObject() {
    if (terminated()) return false;
    return true;
  }

  bool EndObject(SizeType memberCount) {
    if (terminated()) return false;
    return true;
  }

  bool StartArray() {
    if (terminated()) return false;
    return true;
  }

  bool EndArray(SizeType elementCount) {
    if (terminated()) return false;
    return true;
  }

  bool String(const char* str, SizeType length, bool copy) {
    if (terminated()) return false;
    return true;
  }

  // Irrelevant events
  bool Null() {
    if (terminated()) return false;
    return true;
  }
  bool Bool(bool b) {
    if (terminated()) return false;
    return true;
  }
  bool Double(double d) {
    if (terminated()) return false;

    return true;
  }
  bool Int(int i) {
    if (terminated()) return false;
    return true;
  }
  bool Int64(int64_t i) {
    if (terminated()) return false;
    return true;
  }
  bool Uint64(uint64_t i) {
    if (terminated()) return false;
    return true;
  }
  bool RawNumber(const char* str, SizeType length, bool copy) {
    if (terminated()) return false;
    return true;
  }
};  // handler

static bool RapidjsonSaxOnDemand(const std::string& json,
                                 const std::vector<std::string_view>& path,
                                 uint64_t& val) {
  rapidjson::Reader reader;
  GetUintHandler handler(path);
  rapidjson::MemoryStream ms(json.data(), json.size());
  auto result = reader.Parse(ms, handler);
  if (result.Code() != rapidjson::kParseErrorTermination) {
    return false;
  }
  val = handler.value_;
  return true;
}
