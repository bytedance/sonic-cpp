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

#ifndef _JSON_H_
#define _JSON_H_
#include <iostream>
#include <memory>
#include <string_view>

struct DocStat {
  size_t objects = 0;
  size_t arrays = 0;
  size_t numbers = 0;
  size_t strings = 0;
  size_t trues = 0;
  size_t falses = 0;
  size_t nulls = 0;

  size_t members = 0;
  size_t elements = 0;
  size_t length = 0;
  size_t depth = 0;

  bool operator==(const DocStat &rhs) const {
    return this->objects == rhs.objects && this->arrays == rhs.arrays &&
           this->numbers == rhs.numbers && this->strings == rhs.strings &&
           this->trues == rhs.trues && this->falses == rhs.falses &&
           this->nulls == rhs.nulls && this->members == rhs.members &&
           this->elements == rhs.elements && this->length == rhs.length;
  }
  bool operator!=(const DocStat &rhs) const { return !(*this == rhs); }
  void print() {
    printf("======== Members ========\n");
    printf("Objects: %ld\n", objects);
    printf("Arrays: %ld\n", arrays);
    printf("Numbers: %ld\n", numbers);
    printf("Strings: %ld\n", strings);
    printf("Trues: %ld\n", trues);
    printf("Falses: %ld\n", falses);
    printf("NULLs: %ld\n", nulls);
    printf("members: %ld\n", members);
    printf("Elements: %ld\n", elements);
    printf("Length: %ld\n", length);
    printf("Depth: %ld\n", depth);
    printf("\n");
  }
};

template <typename PR, typename SR>
class ParseResult {
 public:
  ParseResult() = default;
  ParseResult(const ParseResult &) = delete;
  ParseResult &operator=(const ParseResult &) = delete;

  std::unique_ptr<SR> stringfy() const {
    std::unique_ptr<SR> sr = std::make_unique<SR>();
    if (sr && static_cast<const PR *>(this)->stringfy_impl(*sr)) return sr;
    return nullptr;
  }

  std::unique_ptr<SR> prettify() const {
    std::unique_ptr<SR> sr = std::make_unique<SR>();
    if (sr && static_cast<const PR *>(this)->prettify_impl(*sr)) return sr;
    return nullptr;
  }

  bool stat(DocStat &stat) const {
    return static_cast<const PR *>(this)->stat_impl(stat);
  }

  bool find(DocStat &stat) const {
    return static_cast<const PR *>(this)->find_impl(stat);
  }

  bool contains(std::string_view str) const {
    return static_cast<const PR *>(this)->contains_impl(str);
  }
};

template <typename T>
class StringResult {
 public:
  std::string_view str() const {
    return static_cast<const T *>(this)->str_impl();
  }
};

template <typename Json, typename PR>
class JsonBase {
 public:
  std::unique_ptr<const PR> parse(std::string_view json_file) const {
    std::unique_ptr<PR> pr = std::make_unique<PR>(json_file);
    if (pr && static_cast<const Json *>(this)->parse_impl(json_file, *pr))
      return std::move(pr);
    return nullptr;
  }
};

#endif
