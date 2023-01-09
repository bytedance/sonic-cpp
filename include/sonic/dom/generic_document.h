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

#include "sonic/dom/dynamicnode.h"
#include "sonic/dom/json_pointer.h"
#include "sonic/dom/parser.h"

namespace sonic_json {
template <typename NodeType>
class GenericDocument : public NodeType {
 public:
  using node_traits = NodeTraits<NodeType>;
  using Allocator = typename node_traits::alloc_type;

  /**
   * @brief Default GenericDocument constructor.
   * @param allocator Allocator pointer to maintain all nodes' memory. If it is
   * not given or nullptr, GenericDocument will create one by itself.
   */
  GenericDocument(Allocator* allocator = nullptr)
      : NodeType(kNull), alloc_(allocator) {
    if (!alloc_) {
      own_alloc_ = std::unique_ptr<Allocator>(new Allocator());
      alloc_ = own_alloc_.get();
    }
  }

  GenericDocument(const GenericDocument& rhs) = delete;
  /**
   * @brief Move constructor
   */
  GenericDocument(GenericDocument&& rhs)
      : NodeType(std::forward<NodeType>(rhs)),
        own_alloc_(rhs.own_alloc_.release()),
        alloc_(rhs.alloc_),
        parse_result_(rhs.parse_result_),
        str_(rhs.str_),
        str_cap_(rhs.str_cap_),
        strp_(rhs.strp_) {
    rhs.clear();
  }

  /**
   * @brief Move assignement
   */
  GenericDocument& operator=(GenericDocument&& rhs) {
    // Step1: clear self memory
    // free the dynamic nodes in assignment
    NodeType::operator=(std::forward<NodeType>(rhs));
    // free the static memory if need
    if (Allocator::kNeedFree) {
      Allocator::Free(static_cast<void*>(str_));
    }

    // Step2: assignment data member
    parse_result_ = rhs.parse_result_;
    alloc_ = rhs.alloc_;
    own_alloc_ = std::move(rhs.own_alloc_);
    str_ = rhs.str_;
    str_cap_ = rhs.str_cap_;
    strp_ = rhs.strp_;

    // Step3: clear rhs memory
    rhs.clear();
    return *this;
  }

  /**
   * @brief Swap function
   */
  GenericDocument& Swap(GenericDocument& rhs) {
    NodeType::Swap(rhs);
    std::swap(parse_result_, rhs.parse_result_);
    own_alloc_.swap(rhs.own_alloc_);
    std::swap(alloc_, rhs.alloc_);
    std::swap(str_, rhs.str_);
    std::swap(str_cap_, rhs.str_cap_);
    std::swap(strp_, rhs.strp_);
    return *this;
  }

  /**
   * @brief Get the reference of memory allocator.
   */
  sonic_force_inline Allocator& GetAllocator() { return *alloc_; }

  /**
   * @brief Get the reference of memory allocator.
   */
  sonic_force_inline const Allocator& GetAllocator() const { return *alloc_; }

  ~GenericDocument() { destroyDom(); }

  /**
   * @brief Parse by std::string
   * @param parseFlags combination of different ParseFlag.
   * @param json json string
   * @note If using memorypool allocator, memory will be cleared every time
   * before parsing to avoid memory overuse.
   */
  template <unsigned parseFlags = kParseDefault>
  GenericDocument& Parse(StringView json) {
    return Parse<parseFlags>(json.data(), json.size());
  }

  template <unsigned parseFlags = kParseDefault>
  GenericDocument& Parse(const char* data, size_t len) {
    destroyDom();
    return parseImpl<parseFlags>(data, len);
  }

  /**
   * @brief Parse by std::string
   * @param parseFlags combination of different ParseFlag.
   * @param json json string pointer
   * @param len json string size
   * @param path the query path of the json keys
   * @note If using memorypool allocator, memory will be cleared every time
   * before parsing to avoid memory overuse.
   */
  template <unsigned parseFlags = kParseDefault,
            typename JPStringType = SONIC_JSON_POINTER_NODE_STRING_DEFAULT_TYPE>
  GenericDocument& ParseOnDemand(StringView json,
                                 const GenericJsonPointer<JPStringType>& path) {
    return ParseOnDemand(json.data(), json.size(), path);
  }

  template <unsigned parseFlags = kParseDefault,
            typename JPStringType = SONIC_JSON_POINTER_NODE_STRING_DEFAULT_TYPE>
  GenericDocument& ParseOnDemand(const char* data, size_t len,
                                 const GenericJsonPointer<JPStringType>& path) {
    destroyDom();
    return parseOnDemandImpl<parseFlags, JPStringType>(data, len, path);
  }
  /**
   * @brief Check parse has error
   */
  bool HasParseError() const { return parse_result_.Error() != kErrorNone; }

  /*
   * @brief Get parse error no.
   */
  sonic_force_inline SonicError GetParseError() const {
    return parse_result_.Error();
  }

  /*
   * @brief Get where has parse error
   */
  sonic_force_inline size_t GetErrorOffset() const {
    return parse_result_.Offset();
  }

 private:
  sonic_force_inline void clear() {
    parse_result_ = ParseResult();
    own_alloc_ = nullptr;
    alloc_ = nullptr;
    str_ = nullptr;
  }

  void destroyDom() {
    if (!Allocator::kNeedFree) {
      this->setType(kNull);
      return;
    }
    // NOTE: must free dynamic nodes at first
    reinterpret_cast<DNode<Allocator>*>(this)->~DNode();
    Allocator::Free(str_);
    // Avoid Double Free
    str_ = nullptr;
    this->setType(kNull);
  }

  template <unsigned parseFlags>
  GenericDocument& parseImpl(const char* json, size_t len) {
    Parser p;
    SAXHandler<NodeType> sax(*alloc_);
    parse_result_ = allocateStringBuffer(json, len);
    if (sonic_unlikely(HasParseError())) {
      return *this;
    }
    if (!sax.SetUp(StringView(json, len))) {
      parse_result_ = kErrorNoMem;
      return *this;
    }
    parse_result_ = p.template Parse<parseFlags>(str_, len, sax);
    if (sonic_unlikely(HasParseError())) {
      return *this;
    }
    NodeType::operator=(std::move(sax.st_[0]));
    return *this;
  }

  template <unsigned parseFlags, typename JPStringType>
  GenericDocument& parseOnDemandImpl(
      const char* json, size_t len,
      const GenericJsonPointer<JPStringType>& path) {
    // get the target json field
    StringView target;
    parse_result_ = internal::GetOnDemand(StringView(json, len), path, target);
    if (sonic_unlikely(HasParseError())) {
      return *this;
    }
    // parse the target field
    return parseImpl<parseFlags>(target.data(), target.size());
  }

  SonicError allocateStringBuffer(const char* json, size_t len) {
    size_t pad_len = len + 64;
    str_ = (char*)(alloc_->Malloc(pad_len));
    if (str_ == nullptr) {
      return kErrorNoMem;
    }
    std::memcpy(str_, json, len);
    // Add ending mask to support parsing invalid json
    str_[len] = 'x';
    str_[len + 1] = '"';
    str_[len + 2] = 'x';
    return kErrorNone;
  }

  friend class Parser;

  // Note: it is a callback function in parse.parse_impl
  void copyToRoot(DNode<Allocator>& node) {
    // copy to inherited DNode member
    NodeType::operator=(std::move(node));
  }

  std::unique_ptr<Allocator> own_alloc_{nullptr};
  Allocator* alloc_{nullptr};  // maybe external allocator
  ParseResult parse_result_{};

  // Node Buffer for internal stack
  DNode<Allocator>* st_{nullptr};
  size_t cap_{0};
  long np_{0};

  // String Buffer for all parsed string values
  char* str_{nullptr};
  size_t str_cap_{0};
  long strp_{0};
};

using Document = GenericDocument<DNode<SONIC_DEFAULT_ALLOCATOR>>;

}  // namespace sonic_json
