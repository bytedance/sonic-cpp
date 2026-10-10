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

#include <limits>
#include <string>
#include <type_traits>
#include <vector>

#include "sonic/string_view.h"

// define the string type of query node
#ifdef SONIC_JSON_POINTER_NODE_STRING
#define SONIC_JSON_POINTER_NODE_STRING_DEFAULT_TYPE \
  SONIC_JSON_POINTER_NODE_STRING
#else
#define SONIC_JSON_POINTER_NODE_STRING_DEFAULT_TYPE std::string
#endif

namespace sonic_json {

template <typename StringType = SONIC_JSON_POINTER_NODE_STRING_DEFAULT_TYPE>
class GenericJsonPointerNode {
 public:
  GenericJsonPointerNode() = delete;
  GenericJsonPointerNode(std::nullptr_t) = delete;
  GenericJsonPointerNode(StringView str)
      : str_(str), num_(0), is_number_(false) {}
  GenericJsonPointerNode(const std::string& str)
      : str_(str), num_(0), is_number_(false) {}
  GenericJsonPointerNode(const char* str)
      : str_(str), num_(0), is_number_(false) {}
  template <typename T, typename std::enable_if<std::is_integral<T>::value,
                                                bool>::type = true>
  GenericJsonPointerNode(T i)
      : str_(), num_(static_cast<int>(i)), is_number_(true) {}

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

  /**
   * @brief Convert this token to an RFC 6901 array index when possible.
   * @param index receives the parsed zero-based array index
   * @return true for an integer node or a canonical decimal token
   * @note String indices must be "0" or start with [1-9]. Leading zeroes,
   *       signs, overflow, and the RFC "-" token are rejected.
   */
  bool TryGetArrayIndex(size_t& index) const noexcept {
    if (IsNum()) {
      if (num_ < 0) {
        return false;
      }
      index = static_cast<size_t>(num_);
      return true;
    }

    const size_t size = str_.size();
    const char* data = str_.data();
    if (size == 0 || (size > 1 && data[0] == '0')) {
      return false;
    }

    size_t value = 0;
    for (size_t i = 0; i < size; ++i) {
      if (data[i] < '0' || data[i] > '9') {
        return false;
      }
      const size_t digit = static_cast<size_t>(data[i] - '0');
      if (value > (std::numeric_limits<size_t>::max() - digit) / 10) {
        return false;
      }
      value = value * 10 + digit;
    }
    index = value;
    return true;
  }

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

  /**
   * @brief Parse the JSON Pointer string representation from RFC 6901.
   * @param pointer an empty root pointer or slash-prefixed reference tokens
   * @return true on success; false for an invalid prefix or '~' escape
   * @note Parsed tokens are copied into StringType. JsonPointerView is excluded
   *       because decoded '~0' and '~1' tokens require owned storage.
   */
  template <typename T = StringType,
            typename std::enable_if<!std::is_same<T, StringView>::value,
                                    int>::type = 0>
  bool Parse(StringView pointer) {
    this->clear();
    if (pointer.empty()) {
      return true;
    }
    if (pointer[0] != '/') {
      return false;
    }

    size_t token_count = 1;
    for (size_t i = 1; i < pointer.size(); ++i) {
      token_count += pointer[i] == '/';
    }
    this->reserve(token_count);

    std::string token;
    size_t i = 1;
    while (true) {
      token.clear();
      while (i < pointer.size() && pointer[i] != '/') {
        if (pointer[i] != '~') {
          token.push_back(pointer[i++]);
          continue;
        }
        if (i + 1 >= pointer.size() ||
            (pointer[i + 1] != '0' && pointer[i + 1] != '1')) {
          this->clear();
          return false;
        }
        token.push_back(pointer[i + 1] == '0' ? '~' : '/');
        i += 2;
      }
      this->emplace_back(token);
      if (i == pointer.size()) {
        return true;
      }
      ++i;
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
