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

#include <string>

#include "sonic/dom/type.h"
#include "sonic/internal/haswell.h"
#include "sonic/string_view.h"
#include "sonic/writebuffer.h"

namespace sonic_json {

template <typename NodeType>
class GenericDocument;

template <typename NodeType>
class SAXHandler {
 public:
  using Allocator = typename NodeType::AllocatorType;
  using MemberType = typename NodeType::MemberNode;

  SAXHandler() = default;
  SAXHandler(Allocator &alloc) : alloc_(&alloc) {}

  SAXHandler(const SAXHandler &) = delete;
  SAXHandler &operator=(const SAXHandler &rhs) = delete;
  SAXHandler(SAXHandler &&rhs)
      : st_(rhs.st_),
        np_(rhs.np_),
        cap_(rhs.cap_),
        parent_(rhs.parent_),
        alloc_(rhs.alloc_) {
    rhs.st_ = nullptr;
    rhs.cap_ = 0;
    rhs.np_ = 0;
    rhs.alloc_ = 0;
  }

  SAXHandler &operator=(SAXHandler &&rhs) {
    TearDown();
    st_ = rhs.st_;
    np_ = rhs.np_;
    cap_ = rhs.cap_;
    parent_ = rhs.parent_;
    alloc_ = rhs.alloc_;

    rhs.st_ = nullptr;
    rhs.np_ = 0;
    rhs.cap_ = 0;
    rhs.parent_ = 0;
    rhs.alloc_ = 0;
    return *this;
  }

  ~SAXHandler() { TearDown(); }

  sonic_force_inline bool SetUp(StringView json) {
    size_t len = json.size();
    size_t cap = len / 2 + 2;
    if (cap < 16) cap = 16;
    if (!st_ || cap_ < cap) {
      st_ = static_cast<NodeType *>(
          std::realloc((void *)(st_), sizeof(NodeType) * cap));
      if (!st_) return false;
      cap_ = cap;
    }
    return true;
  };

  sonic_force_inline void TearDown() {
    if (st_ == nullptr) return;
    for (size_t i = 0; i < np_; i++) {
      st_[i].~NodeType();
    }
    std::free(st_);
    st_ = nullptr;
  };

#define SONIC_ADD_NODE()       \
  {                            \
    if (!node()) return false; \
  }

  sonic_force_inline bool Null() noexcept {
    SONIC_ADD_NODE();
    new (&st_[np_ - 1]) NodeType(kNull);
    return true;
  }

  sonic_force_inline bool Bool(bool val) noexcept {
    SONIC_ADD_NODE();
    new (&st_[np_ - 1]) NodeType(val);
    return true;
  }

  sonic_force_inline bool Uint(uint64_t val) noexcept {
    SONIC_ADD_NODE();
    new (&st_[np_ - 1]) NodeType(val);
    return true;
  }

  sonic_force_inline bool Int(int64_t val) noexcept {
    SONIC_ADD_NODE();
    new (&st_[np_ - 1]) NodeType(val);
    return true;
  }

  sonic_force_inline bool Double(double val) noexcept {
    SONIC_ADD_NODE();
    new (&st_[np_ - 1]) NodeType(val);
    return true;
  }

  sonic_force_inline bool Key(StringView s) { return stringImpl(s); }

  sonic_force_inline bool String(StringView s) { return stringImpl(s); }

  sonic_force_inline bool StartObject() noexcept {
    SONIC_ADD_NODE();
    NodeType *cur = &st_[np_ - 1];
    cur->o.next.ofs = parent_;
    parent_ = np_ - 1;
    return true;
  }

  sonic_force_inline bool StartArray() noexcept {
    SONIC_ADD_NODE();
    NodeType *cur = &st_[np_ - 1];
    cur->o.next.ofs = parent_;
    parent_ = np_ - 1;
    return true;
  }

  sonic_force_inline bool EndObject(uint32_t pairs) {
    NodeType &obj = st_[parent_];
    size_t old = obj.o.next.ofs;
    obj.setLength(pairs, kObject);
    if (pairs) {
      void *mem = obj.template containerMalloc<MemberType>(pairs, *alloc_);
      obj.setChildren(mem);
      internal::haswell::xmemcpy<sizeof(MemberType)>(
          (void *)obj.getObjChildrenFirstUnsafe(), (void *)(&obj + 1), pairs);
    } else {
      obj.setChildren(nullptr);
    }
    np_ = parent_ + 1;
    parent_ = old;
    return true;
  }

  sonic_force_inline bool EndArray(uint32_t count) {
    NodeType &arr = st_[parent_];
    size_t old = arr.o.next.ofs;
    arr.setLength(count, kArray);
    if (count) {
      arr.setChildren(arr.template containerMalloc<NodeType>(count, *alloc_));
      internal::haswell::xmemcpy<sizeof(NodeType)>(
          (void *)arr.getArrChildrenFirstUnsafe(), (void *)(&arr + 1), count);
    } else {
      arr.setChildren(nullptr);
    }
    np_ = parent_ + 1;
    parent_ = old;
    return true;
  }

 private:
  friend class GenericDocument<NodeType>;

  sonic_force_inline bool stringImpl(StringView s) {
    SONIC_ADD_NODE();
    st_[np_ - 1].setLength(s.size(), kStringCopy);
    st_[np_ - 1].sv.p = s.data();
    return true;
  }

#undef SONIC_ADD_NODE

  sonic_force_inline bool node() noexcept {
    if (sonic_likely(np_ < cap_)) {
      np_++;
      return true;
    } else {
      return false;
    }
  }

  NodeType *st_{nullptr};
  size_t np_{0};
  size_t cap_{0};
  size_t parent_{0};
  Allocator *alloc_{nullptr};
};

template <typename NodeType>
class LazySAXHandler {
 public:
  using Allocator = typename NodeType::AllocatorType;
  using MemberType = typename NodeType::MemberNode;

  LazySAXHandler() = delete;
  LazySAXHandler(Allocator &alloc) : alloc_(&alloc) {}

  ~LazySAXHandler() {
    NodeType *st_ = stack_.template Begin<NodeType>();
    // free allocated escaped buffers
    for (size_t i = 0; i < stack_.Size() / sizeof(NodeType); i++) {
      st_[i].~NodeType();
    }
  }

  sonic_force_inline bool StartArray() {
    new (stack_.PushSize<NodeType>(1)) NodeType(kArray);
    return true;
  }

  sonic_force_inline bool StartObject() {
    new (stack_.PushSize<NodeType>(1)) NodeType(kObject);
    return true;
  }

  sonic_force_inline bool EndArray(size_t count) {
    NodeType &arr = *stack_.template Begin<NodeType>();
    arr.setLength(count, kArray);
    if (count) {
      arr.setChildren(arr.template containerMalloc<NodeType>(count, *alloc_));
      internal::haswell::xmemcpy<sizeof(NodeType)>(
          (void *)arr.getArrChildrenFirstUnsafe(), (void *)(&arr + 1), count);
      stack_.Pop<NodeType>(count);
    } else {
      arr.setChildren(nullptr);
    }
    return true;
  }

  sonic_force_inline bool EndObject(size_t pairs) {
    NodeType &obj = *stack_.template Begin<NodeType>();
    obj.setLength(pairs, kObject);
    if (pairs) {
      void *mem = obj.template containerMalloc<MemberType>(pairs, *alloc_);
      obj.setChildren(mem);
      internal::haswell::xmemcpy<sizeof(MemberType)>(
          (void *)obj.getObjChildrenFirstUnsafe(), (void *)(&obj + 1), pairs);
      stack_.Pop<MemberType>(pairs);
    } else {
      obj.setChildren(nullptr);
    }
    return true;
  }

  sonic_force_inline bool Key(const char *data, size_t len, size_t allocated) {
    new (stack_.PushSize<NodeType>(1)) NodeType();
    NodeType *key = stack_.Top<NodeType>();
    key->setLength(len, allocated ? kStringFree : kStringCopy);
    key->sv.p = data;
    return true;
  }

  sonic_force_inline bool Raw(const char *data, size_t len) {
    new (stack_.PushSize<NodeType>(1)) NodeType();
    stack_.Top<NodeType>()->setRaw(StringView(data, len));
    return true;
  }

  sonic_force_inline Allocator &GetAllocator() { return *alloc_; }

  static constexpr size_t kDefaultNum = 16;
  // allocator for node stack and string buffers
  Allocator *alloc_{nullptr};
  internal::Stack stack_{};
};

}  // namespace sonic_json
