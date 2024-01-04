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
#include "sonic/internal/arch/simd_base.h"
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

  SchemaHandler() = default;
  SchemaHandler(NodeType *root, Allocator &alloc)
      : parent_node_(root), cur_node_(root), alloc_(&alloc) {}

  SchemaHandler(const SchemaHandler &) = delete;
  SchemaHandler &operator=(const SchemaHandler &rhs) = delete;
  SchemaHandler(SchemaHandler &&rhs)
      : st_(rhs.st_),
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
    parent_st_ = std::move(rhs.parent_st_);
    found_count_st_ = std::move(rhs.found_count_st_);
  }

  SchemaHandler &operator=(SchemaHandler &&rhs) {
    TearDown();
    st_ = rhs.st_;
    parent_node_ = rhs.parent_node_;
    cur_node_ = rhs.cur_node_;
    np_ = rhs.np_;
    cap_ = rhs.cap_;
    parent_ = rhs.parent_;
    found_node_count_ = rhs.found_node_count_;
    alloc_ = rhs.alloc_;

    rhs.st_ = nullptr;
    rhs.parent_node_ = nullptr;
    rhs.cur_node_ = nullptr;
    rhs.np_ = 0;
    rhs.cap_ = 0;
    rhs.parent_ = 0;
    rhs.alloc_ = nullptr;
    rhs.found_node_count_ = 0;
    parent_st_ = std::move(rhs.parent_st_);
    found_count_st_ = std::move(rhs.found_count_st_);
    return *this;
  }

  ~SchemaHandler() { TearDown(); }

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
      cur_node_->SetString(s, *alloc_);
    } else {
      return stringImpl(s);
    }
    return true;
  }

  sonic_force_inline bool StartObject() noexcept {
    if (cur_node_) {
      parent_st_.emplace_back(parent_node_);
      parent_node_ = cur_node_;
      cur_node_ = nullptr;
      if (!parent_node_->IsObject() || parent_node_->Size() == 0) {
        parent_st_.emplace_back(parent_node_);
        parent_node_ = nullptr;
      }

      found_count_st_.emplace_back(found_node_count_);
      found_node_count_ = 0;

      return true;
    }
    SONIC_ADD_NODE();
    NodeType *cur = &st_[np_ - 1];
    cur->o.next.ofs = parent_;
    parent_ = np_ - 1;
    return true;
  }

  sonic_force_inline bool StartArray() noexcept {
    if (cur_node_) {
      parent_st_.emplace_back(parent_node_);
      parent_node_ = cur_node_;
      cur_node_ = nullptr;
    }
    SONIC_ADD_NODE();
    NodeType *cur = &st_[np_ - 1];
    cur->o.next.ofs = parent_;
    parent_ = np_ - 1;
    return true;
  }

  sonic_force_inline bool EndObject(uint32_t pairs) {
    if (parent_node_ && parent_node_->IsObject()) {
      parent_node_ = parent_st_.back();
      parent_st_.pop_back();
      cur_node_ = nullptr;
      found_node_count_ = found_count_st_.back();
      found_count_st_.pop_back();
      return true;
    }
    // all object is need create
    NodeType *obj_ptr;
    void *obj_member_ptr;
    if (parent_ == 0) {
      obj_ptr = parent_st_.back();
      obj_member_ptr = &st_[0];
      parent_st_.pop_back();
      // resotre parent node ptr
      parent_node_ = parent_st_.back();
      parent_st_.pop_back();
      cur_node_ = nullptr;
      np_ = 0;
      parent_ = 0;
    } else {
      obj_ptr = &(st_[parent_]);
      obj_member_ptr = &st_[parent_ + 1];
      np_ = parent_ + 1;
      parent_ = obj_ptr->o.next.ofs;
    }
    NodeType &obj = *obj_ptr;
    obj.setLength(pairs, kObject);
    if (pairs) {
      void *mem = obj.template containerMalloc<MemberType>(pairs, *alloc_);
      obj.setChildren(mem);
      internal::Xmemcpy<sizeof(MemberType)>(
          (void *)obj.getObjChildrenFirstUnsafe(), obj_member_ptr, pairs);
    } else {
      obj.setChildren(nullptr);
    }
    return true;
  }

  sonic_force_inline bool EndArray(uint32_t count) {
    // Assert cur_node != nullptr!!
    NodeType *arr_ptr;
    void *arr_element_ptr;
    if (parent_ == 0) {  //
      arr_ptr = parent_node_;
      arr_element_ptr = &st_[1];
      cur_node_ = parent_node_;
      parent_node_ = parent_st_.back();
      parent_st_.pop_back();
      np_ = 0;
      parent_ = 0;
    } else {
      arr_ptr = &st_[parent_];
      arr_element_ptr = &st_[parent_ + 1];
      np_ = parent_ + 1;
      parent_ = arr_ptr->o.next.ofs;
    }
    NodeType &arr = *arr_ptr;
    arr.setLength(count, kArray);
    if (count) {
      arr.setChildren(arr.template containerMalloc<NodeType>(count, *alloc_));
      internal::Xmemcpy<sizeof(NodeType)>(
          (void *)arr.getArrChildrenFirstUnsafe(), arr_element_ptr, count);
    } else {
      arr.setChildren(nullptr);
    }
    return true;
  }
  static constexpr bool check_key_return = true;

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
  NodeType *parent_node_{nullptr};
  NodeType *cur_node_{nullptr};
  size_t np_{0};
  size_t cap_{0};
  size_t parent_{0};
  size_t found_node_count_{0};
  Allocator *alloc_{nullptr};
  std::vector<NodeType *> parent_st_{16};
  std::vector<size_t> found_count_st_{16};
};

}  // namespace sonic_json
