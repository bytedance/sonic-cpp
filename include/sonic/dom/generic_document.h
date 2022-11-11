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

  friend class Parser<NodeType>;

  /**
   * @brief Default GenericDocument constructor.
   * @param allocator Allocator pointer to maintain all nodes memory. If is not
   * given or nullptr, GenericDocument will create one by itself.
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
        pparse_result_(rhs.pparse_result_),
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
    pparse_result_ = rhs.pparse_result_;
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
    std::swap(pparse_result_, rhs.pparse_result_);
    own_alloc_.swap(rhs.own_alloc_);
    std::swap(alloc_, rhs.alloc_);
    std::swap(str_, rhs.str_);
    std::swap(str_cap_, rhs.str_cap_);
    std::swap(strp_, rhs.strp_);
    return *this;
  }

  /**
   * @brief Get reference of memory allocator.
   */
  sonic_force_inline Allocator& GetAllocator() { return *alloc_; }

  /**
   * @brief Get reference of memory allocator.
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
  GenericDocument& Parse(const std::string& json) {
    return parseImpl<parseFlags>(json.c_str(), json.size());
  }

  /**
   * @brief Parse by std::string
   * @param parseFlags combination of different ParseFlag.
   * @param json json string pointer
   * @param len json string size
   * @note If using memorypool allocator, memory will be cleared every time
   * before parsing to avoid memory overuse.
   */
  template <unsigned parseFlags = kParseDefault>
  GenericDocument& Parse(const char* json, size_t len) {
    return parseImpl<parseFlags>(json, len);
  }

  /**
   * @brief Parse by std::string
   * @param parseFlags combination of different ParseFlag.
   * @param json json string pointer
   * @param len json string size
   * @param path the query path of json keys
   * @note If using memorypool allocator, memory will be cleared every time
   * before parsing to avoid memory overuse.
   */
  template <unsigned parseFlags = kParseDefault,
            typename JPStringType = SONIC_JSON_POINTER_NODE_STRING_DEFAULT_TYPE>
  GenericDocument& ParseOnDemand(const char* json, size_t len,
                                 const GenericJsonPointer<JPStringType>& path) {
    return parseOnDemandImpl<parseFlags, JPStringType>(json, len, path);
  }

  template <unsigned parseFlags = kParseDefault,
            typename JPStringType = SONIC_JSON_POINTER_NODE_STRING_DEFAULT_TYPE>
  GenericDocument& ParseOnDemand(const std::string json,
                                 const GenericJsonPointer<JPStringType>& path) {
    return parseOnDemandImpl<parseFlags, JPStringType>(json.data(), json.size(),
                                                       path);
  }

  /**
   * @brief Check parse has error
   */
  bool HasParseError() const { return pparse_result_.Error() != kErrorNone; }

  /*
   * @brief Get parse error no.
   */
  sonic_force_inline SonicError GetParseError() const {
    return pparse_result_.Error();
  }

  /*
   * @brief Get where has parse error
   */
  sonic_force_inline size_t GetErrorOffset() const {
    return pparse_result_.Offset();
  }

 private:
  sonic_force_inline void clear() {
    pparse_result_ = ParseResult();
    own_alloc_ = nullptr;
    alloc_ = nullptr;
    str_ = nullptr;
  }

  void destroyDom() {
    if (!Allocator::kNeedFree) {
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
    Parser<DNode<Allocator>> p;
    // NOTE: free the current memory
    destroyDom();
    pparse_result_ = p.template Parse<parseFlags>(json, len, *this);
    if (sonic_unlikely(HasParseError())) {
      new (this) DNode<Allocator>(kNull);
    }
    return *this;
  }

  template <unsigned parseFlags, typename JPStringType>
  GenericDocument& parseOnDemandImpl(
      const char* json, size_t len,
      const GenericJsonPointer<JPStringType>& path) {
    Parser<DNode<Allocator>> p;
    // NOTE: free the current memory
    destroyDom();
    pparse_result_ = p.template ParseOnDemand<parseFlags, JPStringType>(
        json, len, *this, path);
    if (sonic_unlikely(HasParseError())) {
      new (this) DNode<Allocator>(kNull);
    }
    return *this;
  }

  // Note: it is a callback function in parse.parse_impl
  void copyToRoot(DNode<Allocator>& node) {
    // copy to inherited DNode member
    NodeType::operator=(std::move(node));
  }

  std::unique_ptr<Allocator> own_alloc_{nullptr};
  Allocator* alloc_{nullptr};  // maybe external allocator
  ParseResult pparse_result_{};

  // Node Buffer for internal stack
  DNode<Allocator>* st_{nullptr};
  size_t cap_{0};
  long np_{0};

  // String Buffer for parsed string value
  char* str_{nullptr};
  size_t str_cap_{0};
  long strp_{0};
};

using Document = GenericDocument<DNode<SONIC_DEFAULT_ALLOCATOR>>;

template <typename Allocator>
class JsonHandler<DNode<Allocator>> {
 public:
  using NodeType = DNode<Allocator>;
  using DomType = GenericDocument<NodeType>;

  sonic_force_inline static bool UseStringView() { return true; }

  sonic_force_inline static bool UseStackDirect() { return false; }

  sonic_force_inline static void SetNull(DNode<Allocator>& node) {
    new (&node) NodeType(kNull);
  };

  sonic_force_inline static void SetBool(DNode<Allocator>& node, bool val) {
    new (&node) NodeType(val);
  };

  sonic_force_inline static void SetDouble(DNode<Allocator>& node, double val) {
    new (&node) NodeType(val);
  };

  sonic_force_inline static void SetDoubleU64(DNode<Allocator>& node,
                                              uint64_t val) {
    union {
      uint64_t uval;
      double dval;
    } d;
    d.uval = val;
    new (&node) NodeType(d.dval);
  };

  sonic_force_inline static void SetSint(DNode<Allocator>& node, int64_t val) {
    new (&node) NodeType(val);
  };

  sonic_force_inline static void SetUint(DNode<Allocator>& node, uint64_t val) {
    new (&node) NodeType(val);
  };

  sonic_force_inline static void SetString(DNode<Allocator>& node,
                                           const char* data, size_t len) {
    node.setLength(len, kStringCopy);
    node.sv.p = data;
  };

  sonic_force_inline static DNode<Allocator>* SetObject(DNode<Allocator>& node,
                                                        DNode<Allocator>* begin,
                                                        size_t pairs,
                                                        DomType& dom) {
    node.setLength(pairs, kObject);
    if (pairs) {
      // Note: shallow copy here, because resource pointer is owned by the node
      // itself, likely move. But the node from begin to end will never call
      // dctor, so, we don't need to set null at here. And this is diffrent from
      // move.
      size_t size = pairs * 2 * sizeof(NodeType);
      void* mem = node.template containerMalloc<typename NodeType::MemberNode>(
          pairs, dom.GetAllocator());
      node.setChildren(mem);
      std::memcpy(static_cast<void*>(node.getObjChildrenFirstUnsafe()),
                  static_cast<void*>(begin), size);
    } else {
      node.setChildren(nullptr);
    }
    return &node + 1;
  }

  sonic_force_inline static DNode<Allocator>* SetArray(DNode<Allocator>& node,
                                                       DNode<Allocator>* begin,
                                                       size_t count,
                                                       DomType& dom) {
    node.setLength(count, kArray);
    if (count) {
      // As above note.
      size_t size = count * sizeof(NodeType);
      node.setChildren(
          node.template containerMalloc<NodeType>(count, dom.GetAllocator()));
      std::memcpy(static_cast<void*>(node.getArrChildrenFirstUnsafe()),
                  static_cast<void*>(begin), size);
    } else {
      node.setChildren(nullptr);
    }
    return &node + 1;
  }
};

}  // namespace sonic_json
