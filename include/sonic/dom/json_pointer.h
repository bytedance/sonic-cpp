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

#pragma once

#include <cstdint>
#include <type_traits>
#include <vector>

#include "sonic/string_view.h"

// define the string type of query node
#ifdef SONIC_JSON_POINTER_NODE_STRING
#define SONIC_JSON_POINTER_NODE_STRING_DEFAULT_TYPE \
  SONIC_JSON_POINTER_NODE_STRING
#else
#include <string>
#define SONIC_JSON_POINTER_NODE_STRING_DEFAULT_TYPE std::string
#endif

namespace sonic_json {

template <typename StringType = SONIC_JSON_POINTER_NODE_STRING_DEFAULT_TYPE>
class GenericJsonPointerNode {
 public:
  GenericJsonPointerNode() = delete;
  GenericJsonPointerNode(std::nullptr_t) = delete;
  GenericJsonPointerNode(StringView str)
      : str_(str), num_(0), is_number_(false), number_valid_(true) {}
  GenericJsonPointerNode(const std::string& str)
      : str_(str), num_(0), is_number_(false), number_valid_(true) {}
  GenericJsonPointerNode(const char* str)
      : str_(str), num_(0), is_number_(false), number_valid_(true) {}
  template <typename T, typename std::enable_if<std::is_integral<T>::value,
                                                bool>::type = true>
  GenericJsonPointerNode(T i)
      : str_(), num_(0), is_number_(true), number_valid_(true) {
    setNumber(i);
  }

  GenericJsonPointerNode(const GenericJsonPointerNode& rhs) = default;
  GenericJsonPointerNode(GenericJsonPointerNode&& rhs) = default;
  GenericJsonPointerNode& operator=(const GenericJsonPointerNode& rhs) =
      default;
  GenericJsonPointerNode& operator=(GenericJsonPointerNode&& rhs) = default;
  ~GenericJsonPointerNode() = default;

  bool operator==(const GenericJsonPointerNode& rhs) const {
    if (is_number_ != rhs.is_number_) return false;
    return IsStr() ? str_ == rhs.str_
                   : (number_valid_ == rhs.number_valid_ && num_ == rhs.num_);
  }
  bool operator!=(const GenericJsonPointerNode& rhs) const {
    return !(*this == rhs);
  }

  bool IsNum() const { return is_number_; }
  bool IsStr() const { return !is_number_; }
  bool IsValidNum() const { return !is_number_ || number_valid_; }
  uint64_t GetNum() const { return num_; }
  const StringType& GetStr() const { return str_; }
  size_t Size() const { return str_.size(); }
  const char* Data() const { return str_.data(); }

 private:
  template <typename T>
  void setNumber(T i) {
    if constexpr (std::is_signed<T>::value) {
      if (i < 0) {
        number_valid_ = false;
        num_ = 0;
        return;
      }
    }
    num_ = static_cast<uint64_t>(i);
  }

  StringType str_{};
  uint64_t num_{};
  bool is_number_{};
  bool number_valid_{};
};

template <typename StringType = SONIC_JSON_POINTER_NODE_STRING_DEFAULT_TYPE>
class GenericJsonPointer
    : public std::vector<GenericJsonPointerNode<StringType>> {
 public:
  using JsonPointerNodeType = GenericJsonPointerNode<StringType>;
  GenericJsonPointer() {}
  GenericJsonPointer(
      std::initializer_list<GenericJsonPointerNode<StringType>> nodes) {
    this->reserve(nodes.size());
    for (auto& i : nodes) {
      this->emplace_back(i);
    }
  }
  GenericJsonPointer(const std::vector<StringType>& nodes) {
    this->reserve(nodes.size());
    for (auto& i : nodes) {
      this->emplace_back(i);
    }
  }
  GenericJsonPointer(std::vector<StringType>&& nodes) {
    this->reserve(nodes.size());
    for (auto& i : nodes) {
      this->emplace_back(std::move(i));
    }
  }
  GenericJsonPointer(const std::vector<int>& nodes) {
    this->reserve(nodes.size());
    for (auto& i : nodes) {
      this->emplace_back(i);
    }
  }

  GenericJsonPointer(const GenericJsonPointer& rhs) = default;
  GenericJsonPointer(GenericJsonPointer&& rhs) = default;
  GenericJsonPointer& operator=(const GenericJsonPointer& other) = default;
  GenericJsonPointer& operator=(GenericJsonPointer&& other) = default;
  ~GenericJsonPointer() = default;

  GenericJsonPointer& operator/=(const GenericJsonPointer& other) {
    this->insert(this->end(), other.begin(), other.end());
    return *this;
  }

  GenericJsonPointer& operator/=(GenericJsonPointer&& other) {
    this->insert(this->end(), std::make_move_iterator(other.begin()),
                 std::make_move_iterator(other.end()));
    return *this;
  }

  GenericJsonPointer& operator/=(const JsonPointerNodeType& other) {
    this->push_back(other);
    return *this;
  }

  GenericJsonPointer& operator/=(JsonPointerNodeType&& other) {
    this->push_back(std::move(other));
    return *this;
  }

  friend GenericJsonPointer operator/(const GenericJsonPointer& lhs,
                                      const GenericJsonPointer& rhs) {
    return GenericJsonPointer(lhs) /= rhs;
  }

  friend GenericJsonPointer operator/(const GenericJsonPointer& lhs,
                                      GenericJsonPointer&& rhs) {
    return GenericJsonPointer(lhs) /= std::move(rhs);
  }

  friend GenericJsonPointer operator/(const GenericJsonPointer& lhs,
                                      const JsonPointerNodeType& rhs) {
    return GenericJsonPointer(lhs) /= rhs;
  }

  friend GenericJsonPointer operator/(const GenericJsonPointer& lhs,
                                      JsonPointerNodeType&& rhs) {
    return GenericJsonPointer(lhs) /= std::move(rhs);
  }
};

using JsonPointer =
    GenericJsonPointer<SONIC_JSON_POINTER_NODE_STRING_DEFAULT_TYPE>;
using JsonPointerNode =
    GenericJsonPointerNode<SONIC_JSON_POINTER_NODE_STRING_DEFAULT_TYPE>;

using JsonPointerView = GenericJsonPointer<StringView>;
using JsonPointerNodeView = GenericJsonPointerNode<StringView>;

}  // namespace sonic_json
