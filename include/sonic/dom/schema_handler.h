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
#include "sonic/internal/stack.h"
#include "sonic/string_view.h"
#include "sonic/writebuffer.h"

namespace sonic_json {

template <typename NodeType>
class GenericDocument;

template <typename NodeType>
class SchemaHandler {
 public:
  using Allocator = typename NodeType::AllocatorType;
  using MemberType = typename NodeType::MemberNode;

  bool oom_{false};

  SchemaHandler() = default;
  SchemaHandler(NodeType* root, Allocator& alloc)
      : parent_node_(root), cur_node_(root), alloc_(&alloc) {}

  SchemaHandler(const SchemaHandler&) = delete;
  SchemaHandler& operator=(const SchemaHandler& rhs) = delete;
  SchemaHandler(SchemaHandler&& rhs)
      : oom_(rhs.oom_),
        error_(rhs.error_),
        st_(rhs.st_),
        parent_node_(rhs.parent_node_),
        cur_node_(rhs.cur_node_),
        np_(rhs.np_),
        cap_(rhs.cap_),
        parent_(rhs.parent_),
        found_node_count_(rhs.found_node_count_),
        alloc_(rhs.alloc_) {
    rhs.st_ = nullptr;
    rhs.parent_node_ = nullptr;
    rhs.cur_node_ = nullptr;
    rhs.cap_ = 0;
    rhs.np_ = 0;
    rhs.alloc_ = nullptr;
    rhs.found_node_count_ = 0;
    rhs.oom_ = false;
    rhs.error_ = kErrorNone;
    parent_st_ = std::move(rhs.parent_st_);
    found_count_st_ = std::move(rhs.found_count_st_);
  }

  SchemaHandler& operator=(SchemaHandler&& rhs) {
    TearDown();
    st_ = rhs.st_;
    parent_node_ = rhs.parent_node_;
    cur_node_ = rhs.cur_node_;
    np_ = rhs.np_;
    cap_ = rhs.cap_;
    parent_ = rhs.parent_;
    found_node_count_ = rhs.found_node_count_;
    alloc_ = rhs.alloc_;
    oom_ = rhs.oom_;
    error_ = rhs.error_;

    rhs.st_ = nullptr;
    rhs.parent_node_ = nullptr;
    rhs.cur_node_ = nullptr;
    rhs.np_ = 0;
    rhs.cap_ = 0;
    rhs.parent_ = 0;
    rhs.alloc_ = nullptr;
    rhs.found_node_count_ = 0;
    rhs.oom_ = false;
    rhs.error_ = kErrorNone;
    parent_st_ = std::move(rhs.parent_st_);
    found_count_st_ = std::move(rhs.found_count_st_);
    return *this;
  }

  ~SchemaHandler() { TearDown(); }

  sonic_force_inline SonicError GetError() const noexcept { return error_; }

  sonic_force_inline bool SetUp(StringView json) {
    oom_ = false;
    error_ = kErrorNone;
    size_t len = json.size();
    size_t cap = len / 2 + 2;
    if (cap < 16) cap = 16;
    if (sonic_unlikely(!reserveStack(cap))) return false;
    parent_st_.Clear();
    parent_st_.ClearOom();
    found_count_st_.Clear();
    found_count_st_.ClearOom();
    return true;
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
    found_node_count_ = 0;
  }

#define SONIC_ADD_NODE()       \
  do {                         \
    if (!node()) return false; \
  } while (0)

  sonic_force_inline bool Null() noexcept {
    if (cur_node_) {
      cur_node_->SetNull();
      return true;
    }
    SONIC_ADD_NODE();
    new (&st_[np_ - 1]) NodeType(kNull);
    return true;
  }

  sonic_force_inline bool Bool(bool val) noexcept {
    if (cur_node_) {
      cur_node_->SetBool(val);
      return true;
    }
    SONIC_ADD_NODE();
    new (&st_[np_ - 1]) NodeType(val);
    return true;
  }

  sonic_force_inline bool Uint(uint64_t val) noexcept {
    if (cur_node_) {
      cur_node_->SetUint64(val);
      return true;
    }
    SONIC_ADD_NODE();
    new (&st_[np_ - 1]) NodeType(val);
    return true;
  }

  sonic_force_inline bool Int(int64_t val) noexcept {
    if (cur_node_) {
      cur_node_->SetInt64(val);
      return true;
    }
    SONIC_ADD_NODE();
    new (&st_[np_ - 1]) NodeType(val);
    return true;
  }

  sonic_force_inline bool Double(double val) noexcept {
    if (cur_node_) {
      cur_node_->SetDouble(val);
      return true;
    }
    SONIC_ADD_NODE();
    new (&st_[np_ - 1]) NodeType(val);
    return true;
  }

  sonic_force_inline bool Raw(const char* data, size_t len) {
    if (cur_node_) {
      cur_node_->setRaw(StringView(data, len));
      return true;
    }
    SONIC_ADD_NODE();
    new (&st_[np_ - 1]) NodeType();
    st_[np_ - 1].setRaw(StringView(data, len));
    return true;
  }

  sonic_force_inline bool Key(StringView s) {
    if (parent_node_ && parent_node_->IsObject()) {
      if (found_node_count_ >= parent_node_->Size()) {
        cur_node_ = nullptr;
        return false;
      }
      auto m = parent_node_->FindMember(s);
      if (m != parent_node_->MemberEnd()) {
        cur_node_ = &(m->value);
        found_node_count_++;
        return true;
      } else {
        cur_node_ = nullptr;
        return false;
      }
    }
    // parent node ptr doesn't exist, we need save value into a new object.
    // cur_node_ need to be set as nullptr!
    cur_node_ = nullptr;
    return stringImpl(s);
  }

  sonic_force_inline bool String(StringView s) {
    if (cur_node_) {
      if (sonic_unlikely(!cur_node_->TrySetString(s, *alloc_))) {
        setOom();
        return false;
      }
    } else {
      return stringImpl(s);
    }
    return true;
  }

  sonic_force_inline bool StartObject() noexcept {
    if (cur_node_) {
      if (sonic_unlikely(!pushParent(parent_node_))) return false;
      parent_node_ = cur_node_;
      cur_node_ = nullptr;
      if (!parent_node_->IsObject() || parent_node_->Size() == 0) {
        if (sonic_unlikely(!pushParent(parent_node_))) return false;
        parent_node_ = nullptr;
      }

      if (sonic_unlikely(!pushFoundCount(found_node_count_))) return false;
      found_node_count_ = 0;

      return true;
    }
    SONIC_ADD_NODE();
    new (&st_[np_ - 1]) NodeType();
    NodeType* cur = &st_[np_ - 1];
    cur->o.next.ofs = parent_;
    parent_ = np_ - 1;
    return true;
  }

  sonic_force_inline bool StartArray() noexcept {
    if (cur_node_) {
      if (sonic_unlikely(!pushParent(parent_node_))) return false;
      parent_node_ = cur_node_;
      cur_node_ = nullptr;
    }
    SONIC_ADD_NODE();
    new (&st_[np_ - 1]) NodeType();
    NodeType* cur = &st_[np_ - 1];
    cur->o.next.ofs = parent_;
    parent_ = np_ - 1;
    return true;
  }

  sonic_force_inline bool NumStr(StringView s) {
    if (cur_node_) {
      cur_node_->SetStringNumber(s);
      return true;
    }
    SONIC_ADD_NODE();
    new (&st_[np_ - 1]) NodeType();
    st_[np_ - 1].setLength(s.size(), kNumStr);
    st_[np_ - 1].sv.p = s.data();
    return true;
  }

  sonic_force_inline bool EndObject(uint32_t pairs) {
    if (parent_node_ && parent_node_->IsObject()) {
      parent_node_ = popParent();
      cur_node_ = nullptr;
      found_node_count_ = popFoundCount();
      return true;
    }
    // all object is need create
    NodeType* obj_ptr;
    void* obj_member_ptr;
    bool replacing_existing = false;
    if (parent_ == 0) {
      replacing_existing = true;
      obj_ptr = popParent();
      obj_member_ptr = &st_[0];
      // restore parent node ptr
      parent_node_ = popParent();
      cur_node_ = nullptr;
      found_node_count_ = popFoundCount();
      np_ = 0;
      parent_ = 0;
    } else {
      obj_ptr = &(st_[parent_]);
      obj_member_ptr = &st_[parent_ + 1];
      np_ = parent_ + 1;
      parent_ = obj_ptr->o.next.ofs;
    }
    NodeType& obj = *obj_ptr;
    NodeType new_obj;
    NodeType& dst = replacing_existing ? new_obj : obj;
    dst.setLength(pairs, kObject);
    dst.setChildren(nullptr);
    bool ok = true;
    if (pairs) {
      void* mem = dst.template containerMalloc<MemberType>(pairs, *alloc_);
      if (sonic_unlikely(mem == nullptr)) {
        NodeType* children = static_cast<NodeType*>(obj_member_ptr);
        for (size_t i = 0; i < size_t(pairs) * 2; i++) children[i].~NodeType();
        dst.setLength(0, kObject);
        dst.setChildren(nullptr);
        setOom();
        ok = false;
      } else {
        dst.setChildren(mem);
        MemberType* dst_members =
            reinterpret_cast<MemberType*>(dst.getObjChildrenFirstUnsafe());
        NodeType* src = static_cast<NodeType*>(obj_member_ptr);
        for (size_t i = 0; i < pairs; ++i) {
          new (&dst_members[i])
              MemberType(std::move(src[i * 2]), std::move(src[i * 2 + 1]));
          src[i * 2].~NodeType();
          src[i * 2 + 1].~NodeType();
        }
      }
    } else {
      dst.setChildren(nullptr);
    }
    if (ok && replacing_existing) {
      obj.destroy();
      obj.rawAssign(new_obj);
    }
    return ok;
  }

  sonic_force_inline bool EndArray(uint32_t count) {
    // Assert cur_node != nullptr!!
    NodeType* arr_ptr;
    void* arr_element_ptr;
    bool replacing_existing = false;
    if (parent_ == 0) {  //
      replacing_existing = true;
      arr_ptr = parent_node_;
      arr_element_ptr = &st_[1];
      cur_node_ = parent_node_;
      parent_node_ = popParent();
      st_[0].~NodeType();
      np_ = 0;
      parent_ = 0;
    } else {
      arr_ptr = &st_[parent_];
      arr_element_ptr = &st_[parent_ + 1];
      np_ = parent_ + 1;
      parent_ = arr_ptr->o.next.ofs;
    }
    NodeType& arr = *arr_ptr;
    NodeType new_arr;
    NodeType& dst = replacing_existing ? new_arr : arr;
    dst.setLength(count, kArray);
    dst.setChildren(nullptr);
    bool ok = true;
    if (count) {
      void* mem = dst.template containerMalloc<NodeType>(count, *alloc_);
      if (sonic_unlikely(mem == nullptr)) {
        NodeType* children = static_cast<NodeType*>(arr_element_ptr);
        for (size_t i = 0; i < count; i++) children[i].~NodeType();
        dst.setLength(0, kArray);
        dst.setChildren(nullptr);
        setOom();
        ok = false;
      } else {
        dst.setChildren(mem);
        NodeType* dst_elements = dst.getArrChildrenFirstUnsafe();
        NodeType* src = static_cast<NodeType*>(arr_element_ptr);
        for (size_t i = 0; i < count; ++i) {
          new (&dst_elements[i]) NodeType(std::move(src[i]));
          src[i].~NodeType();
        }
      }
    } else {
      dst.setChildren(nullptr);
    }
    if (ok && replacing_existing) {
      arr.destroy();
      arr.rawAssign(new_arr);
    }
    return ok;
  }
  static constexpr bool check_key_return = true;

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
    setOom();
    return false;
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

  sonic_force_inline bool pushParent(NodeType* node) noexcept {
    if (sonic_unlikely(!parent_st_.template Push<NodeType*>(node))) {
      setOom();
      return false;
    }
    return true;
  }

  sonic_force_inline NodeType* popParent() noexcept {
    NodeType* node = *parent_st_.template Top<NodeType*>();
    parent_st_.template Pop<NodeType*>(1);
    return node;
  }

  sonic_force_inline bool pushFoundCount(size_t count) noexcept {
    if (sonic_unlikely(!found_count_st_.template Push<size_t>(count))) {
      setOom();
      return false;
    }
    return true;
  }

  sonic_force_inline size_t popFoundCount() noexcept {
    size_t count = *found_count_st_.template Top<size_t>();
    found_count_st_.template Pop<size_t>(1);
    return count;
  }

  SonicError error_{kErrorNone};
  NodeType* st_{nullptr};
  NodeType* parent_node_{nullptr};
  NodeType* cur_node_{nullptr};
  size_t np_{0};
  size_t cap_{0};
  size_t parent_{0};
  size_t found_node_count_{0};
  Allocator* alloc_{nullptr};
  internal::Stack parent_st_{16 * sizeof(NodeType*)};
  internal::Stack found_count_st_{16 * sizeof(size_t)};
};

}  // namespace sonic_json
