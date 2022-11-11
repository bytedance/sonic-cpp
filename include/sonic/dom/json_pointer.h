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

#include <vector>

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
  GenericJsonPointerNode(const std::string& str)
      : str_(str), num_(0), is_number_(false) {}
  GenericJsonPointerNode(const char* str, size_t len)
      : str_(str, len), num_(0), is_number_(false) {}
  GenericJsonPointerNode(int i) : str_(), num_(i), is_number_(true) {}
  template <size_t N>
  GenericJsonPointerNode(const char (&str)[N])
      : str_(str, N - 1), num_(0), is_number_(false) {}

  GenericJsonPointerNode(const GenericJsonPointerNode& rhs) = default;
  GenericJsonPointerNode(GenericJsonPointerNode&& rhs) = default;
  GenericJsonPointerNode& operator=(const GenericJsonPointerNode& rhs) =
      default;
  GenericJsonPointerNode& operator=(GenericJsonPointerNode&& rhs) = default;
  ~GenericJsonPointerNode() = default;

  bool operator==(const GenericJsonPointerNode& rhs) const {
    return IsStr() ? str_ == rhs.str_ : num_ == rhs.num_;
  }
  bool operator!=(const GenericJsonPointerNode& rhs) const {
    return !(*this == rhs);
  }

  bool IsNum() const { return is_number_; }
  bool IsStr() const { return !is_number_; }
  int GetNum() const { return num_; }
  const StringType& GetStr() const { return str_; }
  size_t Size() const { return str_.size(); }
  const char* Data() const { return str_.data(); }

 private:
  StringType str_{};
  int num_{};
  bool is_number_{};
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

}  // namespace sonic_json