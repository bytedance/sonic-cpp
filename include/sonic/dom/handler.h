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

#include "sonic/dom/type.h"
#include "sonic/error.h"
#include "sonic/internal/arch/simd_base.h"
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

  bool oom_{false};

  SAXHandler() = default;
  SAXHandler(Allocator& alloc) : alloc_(&alloc) {}

  SAXHandler(const SAXHandler&) = delete;
  SAXHandler& operator=(const SAXHandler& rhs) = delete;
  SAXHandler(SAXHandler&& rhs)
      : oom_(rhs.oom_),
        error_(rhs.error_),
        st_(rhs.st_),
        np_(rhs.np_),
        cap_(rhs.cap_),
        parent_(rhs.parent_),
        alloc_(rhs.alloc_) {
    rhs.st_ = nullptr;
    rhs.cap_ = 0;
    rhs.np_ = 0;
    rhs.alloc_ = 0;
    rhs.oom_ = false;
    rhs.error_ = kErrorNone;
  }

  SAXHandler& operator=(SAXHandler&& rhs) {
    TearDown();
    st_ = rhs.st_;
    np_ = rhs.np_;
    cap_ = rhs.cap_;
    parent_ = rhs.parent_;
    alloc_ = rhs.alloc_;
    oom_ = rhs.oom_;
    error_ = rhs.error_;

    rhs.st_ = nullptr;
    rhs.np_ = 0;
    rhs.cap_ = 0;
    rhs.parent_ = 0;
    rhs.alloc_ = 0;
    rhs.oom_ = false;
    rhs.error_ = kErrorNone;
    return *this;
  }

  ~SAXHandler() { TearDown(); }

  sonic_force_inline SonicError GetError() const noexcept { return error_; }

  sonic_force_inline bool SetUp(StringView json) {
    oom_ = false;
    error_ = kErrorNone;
    size_t len = json.size();
    size_t cap = len / 2 + 2;
    if (cap < 16) cap = 16;
    return reserveStack(cap);
  }

  sonic_force_inline void TearDown() {
    if (st_ != nullptr) {
      for (size_t i = 0; i < np_; i++) {
        st_[i].~NodeType();
      }
      std::free(st_);
    }
    st_ = nullptr;
    np_ = 0;
    cap_ = 0;
    parent_ = 0;
  }

#define SONIC_ADD_NODE()       \
  do {                         \
    if (!node()) return false; \
  } while (0)

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

  sonic_force_inline bool NumStr(StringView s) {
    SONIC_ADD_NODE();
    new (&st_[np_ - 1]) NodeType();
    st_[np_ - 1].setLength(s.size(), kNumStr);
    st_[np_ - 1].sv.p = s.data();
    return true;
  }

  sonic_force_inline bool Raw(const char* data, size_t len) {
    SONIC_ADD_NODE();
    new (&st_[np_ - 1]) NodeType();
    auto raw = StringView(data, len);
    st_[np_ - 1].setRaw(raw);
    return true;
  }

  sonic_force_inline bool StartObject() noexcept {
    SONIC_ADD_NODE();
    new (&st_[np_ - 1]) NodeType();
    NodeType* cur = &st_[np_ - 1];
    cur->o.next.ofs = parent_;
    parent_ = np_ - 1;
    return true;
  }

  sonic_force_inline bool StartArray() noexcept {
    SONIC_ADD_NODE();
    new (&st_[np_ - 1]) NodeType();
    NodeType* cur = &st_[np_ - 1];
    cur->o.next.ofs = parent_;
    parent_ = np_ - 1;
    return true;
  }

  sonic_force_inline bool EndObject(uint32_t pairs) {
    NodeType& obj = st_[parent_];
    size_t old = obj.o.next.ofs;
    obj.setLength(pairs, kObject);
    bool ok = true;
    if (pairs) {
      void* mem = obj.template containerMalloc<MemberType>(pairs, *alloc_);
      if (sonic_unlikely(mem == nullptr)) {
        NodeType* children = &obj + 1;
        for (size_t i = 0; i < size_t(pairs) * 2; i++) children[i].~NodeType();
        obj.setLength(0, kObject);
        obj.setChildren(nullptr);
        setOom();
        ok = false;
      } else {
        obj.setChildren(mem);
        MemberType* dst =
            reinterpret_cast<MemberType*>(obj.getObjChildrenFirstUnsafe());
        NodeType* src = &obj + 1;
        for (size_t i = 0; i < pairs; ++i) {
          new (&dst[i])
              MemberType(std::move(src[i * 2]), std::move(src[i * 2 + 1]));
          src[i * 2].~NodeType();
          src[i * 2 + 1].~NodeType();
        }
      }
    } else {
      obj.setChildren(nullptr);
    }
    np_ = parent_ + 1;
    parent_ = old;
    return ok;
  }

  sonic_force_inline bool EndArray(uint32_t count) {
    NodeType& arr = st_[parent_];
    size_t old = arr.o.next.ofs;
    arr.setLength(count, kArray);
    bool ok = true;
    if (count) {
      void* mem = arr.template containerMalloc<NodeType>(count, *alloc_);
      if (sonic_unlikely(mem == nullptr)) {
        NodeType* children = &arr + 1;
        for (size_t i = 0; i < count; i++) children[i].~NodeType();
        arr.setLength(0, kArray);
        arr.setChildren(nullptr);
        setOom();
        ok = false;
      } else {
        arr.setChildren(mem);
        NodeType* dst = arr.getArrChildrenFirstUnsafe();
        NodeType* src = &arr + 1;
        for (size_t i = 0; i < count; ++i) {
          new (&dst[i]) NodeType(std::move(src[i]));
          src[i].~NodeType();
        }
      }
    } else {
      arr.setChildren(nullptr);
    }
    np_ = parent_ + 1;
    parent_ = old;
    return ok;
  }

 private:
  friend class GenericDocument<NodeType>;

  sonic_force_inline bool stringImpl(StringView s) {
    SONIC_ADD_NODE();
    new (&st_[np_ - 1]) NodeType();
    st_[np_ - 1].setLength(s.size(), kStringCopy);
    st_[np_ - 1].sv.p = s.data();
    return true;
  }

#undef SONIC_ADD_NODE

  sonic_force_inline bool node() noexcept {
    if (sonic_likely(np_ < cap_)) {
      np_++;
      return true;
    }
    if (sonic_unlikely(cap_ > std::numeric_limits<size_t>::max() / 2)) {
      setOom();
      return false;
    }
    size_t new_cap = cap_ * 2;
    if (sonic_unlikely(new_cap >
                       std::numeric_limits<size_t>::max() / sizeof(NodeType))) {
      setOom();
      return false;
    }
    if (sonic_unlikely(!reserveStack(new_cap))) return false;
    np_++;
    return true;
  }

  sonic_force_inline bool reserveStack(size_t new_cap) {
    if (new_cap <= cap_) return true;
    if (sonic_unlikely(new_cap >
                       std::numeric_limits<size_t>::max() / sizeof(NodeType))) {
      setOom();
      return false;
    }
    NodeType* new_st =
        static_cast<NodeType*>(std::malloc(sizeof(NodeType) * new_cap));
    if (!new_st) {
      setOom();
      return false;
    }
    for (size_t i = 0; i < np_; ++i) {
      new (&new_st[i]) NodeType(std::move(st_[i]));
      st_[i].~NodeType();
    }
    std::free(st_);
    st_ = new_st;
    cap_ = new_cap;
    return true;
  }

  sonic_force_inline void setOom() noexcept {
    oom_ = true;
    error_ = kErrorNoMem;
  }

  SonicError error_{kErrorNone};
  NodeType* st_{nullptr};
  size_t np_{0};
  size_t cap_{0};
  size_t parent_{0};
  Allocator* alloc_{nullptr};
};

template <typename NodeType>
class LazySAXHandler {
 public:
  using Allocator = typename NodeType::AllocatorType;
  using MemberType = typename NodeType::MemberNode;

  LazySAXHandler() = delete;
  LazySAXHandler(Allocator& alloc) : alloc_(&alloc) {}
  LazySAXHandler(const LazySAXHandler&) = delete;
  LazySAXHandler& operator=(const LazySAXHandler&) = delete;
  LazySAXHandler(LazySAXHandler&& rhs) noexcept
      : alloc_(rhs.alloc_),
        st_(rhs.st_),
        np_(rhs.np_),
        cap_(rhs.cap_),
        oom_(rhs.oom_),
        error_(rhs.error_) {
    rhs.alloc_ = nullptr;
    rhs.st_ = nullptr;
    rhs.np_ = 0;
    rhs.cap_ = 0;
    rhs.oom_ = false;
    rhs.error_ = kErrorNone;
  }
  LazySAXHandler& operator=(LazySAXHandler&& rhs) noexcept {
    if (this == &rhs) return *this;
    TearDown();
    alloc_ = rhs.alloc_;
    st_ = rhs.st_;
    np_ = rhs.np_;
    cap_ = rhs.cap_;
    oom_ = rhs.oom_;
    error_ = rhs.error_;
    rhs.alloc_ = nullptr;
    rhs.st_ = nullptr;
    rhs.np_ = 0;
    rhs.cap_ = 0;
    rhs.oom_ = false;
    rhs.error_ = kErrorNone;
    return *this;
  }

  ~LazySAXHandler() { TearDown(); }

  void TearDown() noexcept {
    if (st_ != nullptr) {
      for (size_t i = 0; i < np_; i++) st_[i].~NodeType();
      std::free(st_);
    }
    st_ = nullptr;
    np_ = 0;
    cap_ = 0;
  }

  sonic_force_inline bool StartArray() {
    NodeType* mem = pushNode();
    if (sonic_unlikely(mem == nullptr)) {
      setOom();
      return false;
    }
    new (mem) NodeType(kArray);
    return true;
  }

  sonic_force_inline bool StartObject() {
    NodeType* mem = pushNode();
    if (sonic_unlikely(mem == nullptr)) {
      setOom();
      return false;
    }
    new (mem) NodeType(kObject);
    return true;
  }

  sonic_force_inline bool EndArray(size_t count) {
    NodeType& arr = *st_;
    arr.setLength(count, kArray);
    if (count) {
      void* mem = arr.template containerMalloc<NodeType>(count, *alloc_);
      if (sonic_unlikely(mem == nullptr)) {
        NodeType* children = &arr + 1;
        for (size_t i = 0; i < count; i++) children[i].~NodeType();
        popNodes(count);
        arr.setLength(0, kArray);
        arr.setChildren(nullptr);
        setOom();
        return false;
      } else {
        arr.setChildren(mem);
        NodeType* dst = arr.getArrChildrenFirstUnsafe();
        NodeType* src = &arr + 1;
        for (size_t i = 0; i < count; ++i) {
          new (&dst[i]) NodeType(std::move(src[i]));
          src[i].~NodeType();
        }
        popNodes(count);
      }
    } else {
      arr.setChildren(nullptr);
    }
    return true;
  }

  sonic_force_inline bool EndObject(size_t pairs) {
    NodeType& obj = *st_;
    obj.setLength(pairs, kObject);
    if (pairs) {
      void* mem = obj.template containerMalloc<MemberType>(pairs, *alloc_);
      if (sonic_unlikely(mem == nullptr)) {
        NodeType* children = &obj + 1;
        for (size_t i = 0; i < size_t(pairs) * 2; i++) children[i].~NodeType();
        popNodes(size_t(pairs) * 2);
        obj.setLength(0, kObject);
        obj.setChildren(nullptr);
        setOom();
        return false;
      } else {
        obj.setChildren(mem);
        MemberType* dst =
            reinterpret_cast<MemberType*>(obj.getObjChildrenFirstUnsafe());
        NodeType* src = &obj + 1;
        for (size_t i = 0; i < pairs; ++i) {
          new (&dst[i])
              MemberType(std::move(src[i * 2]), std::move(src[i * 2 + 1]));
          src[i * 2].~NodeType();
          src[i * 2 + 1].~NodeType();
        }
        popNodes(size_t(pairs) * 2);
      }
    } else {
      obj.setChildren(nullptr);
    }
    return true;
  }

  sonic_force_inline bool Key(const char* data, size_t len, size_t allocated) {
    NodeType* mem = pushNode();
    if (sonic_unlikely(mem == nullptr)) {
      setOom();
      return false;
    }
    new (mem) NodeType();
    mem->setLength(len, allocated ? kStringFree : kStringCopy);
    mem->sv.p = data;
    return true;
  }

  sonic_force_inline bool Raw(const char* data, size_t len) {
    NodeType* mem = pushNode();
    if (sonic_unlikely(mem == nullptr)) {
      setOom();
      return false;
    }
    new (mem) NodeType();
    mem->setRaw(StringView(data, len));
    return true;
  }

  sonic_force_inline Allocator& GetAllocator() { return *alloc_; }
  sonic_force_inline SonicError GetError() const noexcept { return error_; }
  sonic_force_inline NodeType* Root() noexcept { return st_; }
  sonic_force_inline size_t StackSizeBytes() const noexcept {
    return np_ * sizeof(NodeType);
  }

  static constexpr size_t kDefaultNum = 16;

 private:
  sonic_force_inline NodeType* pushNode() {
    if (sonic_unlikely(np_ == cap_ && !reserveStack(nextCap()))) {
      return nullptr;
    }
    return &st_[np_++];
  }

  sonic_force_inline void popNodes(size_t n) { np_ -= n; }

  sonic_force_inline size_t nextCap() const {
    if (cap_ == 0) return kDefaultNum;
    size_t max_count = std::numeric_limits<size_t>::max() / sizeof(NodeType);
    return cap_ > max_count / 2 ? max_count : cap_ * 2;
  }

  sonic_force_inline bool reserveStack(size_t new_cap) {
    if (new_cap <= cap_) return true;
    if (sonic_unlikely(new_cap >
                       std::numeric_limits<size_t>::max() / sizeof(NodeType))) {
      setOom();
      return false;
    }
    NodeType* new_st =
        static_cast<NodeType*>(std::malloc(sizeof(NodeType) * new_cap));
    if (sonic_unlikely(new_st == nullptr)) {
      setOom();
      return false;
    }
    for (size_t i = 0; i < np_; ++i) {
      new (&new_st[i]) NodeType(std::move(st_[i]));
      st_[i].~NodeType();
    }
    std::free(st_);
    st_ = new_st;
    cap_ = new_cap;
    return true;
  }

  sonic_force_inline void setOom() noexcept {
    oom_ = true;
    error_ = kErrorNoMem;
  }

 public:
  // allocator for node stack and string buffers
  Allocator* alloc_{nullptr};
  NodeType* st_{nullptr};
  size_t np_{0};
  size_t cap_{0};
  bool oom_{false};
  SonicError error_{kErrorNone};
};

}  // namespace sonic_json
