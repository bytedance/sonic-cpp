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

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <functional>
#include <limits>
#include <new>
#include <type_traits>
#include <utility>

#include "sonic/allocator.h"
#include "sonic/dom/genericnode.h"
#include "sonic/dom/handler.h"
#include "sonic/dom/schema_handler.h"
#include "sonic/dom/serialize.h"
#include "sonic/dom/type.h"
#include "sonic/error.h"
#include "sonic/internal/ftoa.h"
#include "sonic/writebuffer.h"

namespace sonic_json {

// Legacy mutating operations (Reserve, AddMember, PushBack, ...) keep their
// source-compatible return types and leave the node unchanged on allocation
// failure. Prefer Try* APIs when callers need full-chain status propagation.
template <typename Allocator = SONIC_DEFAULT_ALLOCATOR>
class DNode : public GenericNode<DNode<Allocator>> {
 public:
  using NodeType = DNode;
  using BaseNode = GenericNode<DNode<Allocator>>;
  using AllocatorType = Allocator;
  using MemberNode = typename NodeTraits<DNode>::MemberNode;
  using MemberIterator = typename NodeTraits<DNode>::MemberIterator;
  using ConstMemberIterator = typename NodeTraits<DNode>::ConstMemberIterator;
  using ValueIterator = typename NodeTraits<DNode>::ValueIterator;
  using ConstValueIterator = typename NodeTraits<DNode>::ConstValueIterator;

  friend class SAXHandler<DNode>;
  friend class LazySAXHandler<DNode>;
  friend class SchemaHandler<DNode>;
  template <typename>
  friend class GenericDocument;

  friend BaseNode;
  template <typename>
  friend class DNode;
  template <SerializeFlags serializeFlags, typename NodeType>
  friend SonicError internal::SerializeImpl(const NodeType*, WriteBuffer&);

  // constructor
  using BaseNode::BaseNode;
  /**
   * @brief move constructor
   * @param rhs moved value, must be a rvalue reference
   */
  DNode() noexcept : BaseNode() {}
  DNode(DNode&& rhs) noexcept : BaseNode() { rawAssign(rhs); }
  DNode(const DNode& rhs) = delete;

  /**
   * @brief copy constructor
   * @tparam rhs class Allocator type
   * @param rhs copied value reference
   * @param alloc allocator reference that maintain this node memory
   * @param copyString false defautlly, copy const string or not.
   */
  template <typename SourceAllocator>
  DNode(const DNode<SourceAllocator>& rhs, Allocator& alloc,
        bool copyString = false)
      : BaseNode() {
    (void)initCopyFrom(rhs, alloc, copyString);
  }

  /**
   * @brief destructor
   */
  ~DNode() {
    if (!Allocator::kNeedFree) {
      return;
    }
    destroy();
  }

  DNode& operator=(const DNode& rhs) = delete;
  DNode& operator=(DNode& rhs) = delete;
  /**
   * @brief move assignment
   * @param rhs rvalue reference to right hand side
   * @return DNode& this reference
   */
  DNode& operator=(DNode&& rhs) {
    if (sonic_likely(this != &rhs)) {
      // Can't destroy "this" before assigning "rhs", otherwise "rhs"
      // could be used after free if it's a sub-node of "this",
      // hence the temporary dance.
      // Copied from RapidJSON.
      DNode temp;
      temp.rawAssign(rhs);
      this->destroy();
      rawAssign(temp);
    }
    return *this;
  }

  using BaseNode::operator==;
  /**
   * @brief compare with another node
   * @tparam SourceAllocator Allocator type of rhs
   * @param rhs right hand side
   * @retval true equals to
   * @retval false not equals to
   */
  template <typename SourceAllocator>
  bool operator==(const DNode<SourceAllocator>& rhs) const noexcept {
    if (this->getBasicType() != rhs.getBasicType()) {
      return false;
    }
    switch (this->GetType()) {
      case kObject: {
        if (this->Size() != rhs.Size()) {
          return false;
        }
        auto rhs_e = rhs.MemberEnd();
        for (auto lhs_it = this->MemberBegin(), lhs_e = this->MemberEnd();
             lhs_it != lhs_e; ++lhs_it) {
          // TODO: Define as a certain type
          auto rhs_it = rhs.FindMember(lhs_it->name.GetStringView());
          if (rhs_it == rhs_e || lhs_it->value != rhs_it->value) {
            return false;
          }
        }
        return true;
      }
      case kArray: {
        if (this->Size() != rhs.Size()) {
          return false;
        }
        auto rhs_it = rhs.Begin();
        for (auto lhs_it = this->Begin(), lhs_e = this->End(); lhs_it != lhs_e;
             ++rhs_it, ++lhs_it) {
          if (*lhs_it != *rhs_it) {
            return false;
          }
        }
        return true;
      }

      case kStringCopy:
      case kStringFree:
      case kStringConst:
      case kNumStr:
      case kRaw:
        return this->GetStringView() == rhs.GetStringView();

      case kReal:
      case kSint:
      case kUint:
        if (this->GetType() != rhs.GetType()) {
          return false;
        }
        // Exactly equal for double.
        return !std::memcmp(this, &rhs, sizeof(rhs));

      default:
        return this->GetType() == rhs.GetType();
    }
  }

  using BaseNode::operator!=;
  /**
   * @brief operator!=
   */
  template <typename SourceAllocator>
  bool operator!=(const DNode<SourceAllocator>& rhs) const noexcept {
    return !(*this == rhs);
  }

  // Object APIs

  using BaseNode::operator[];
  using BaseNode::FindMember;

  /**
   * @brief Find a specific member in an object. A member is a pair node of key
   * and name.
   * @param key target name pointer
   * @param len target name length
   * @retval MemberEnd() not found
   * @retval others iterator for found member
   * @note If target name is a literal string, string_view can be optimized by
   * compiler. This function will provide a better memcmp implementation than
   * std::memcmp while length is not too large.
   */
  sonic_force_inline MemberIterator FindMember(const char* key,
                                               size_t len) noexcept {
    return findMemberImpl(key, len);
  }

  /**
   * @brief Find a specific member in an object. A member is a pair node of key
   * and name.
   * @param key target name pointer
   * @param len target name length
   * @retval MemberEnd() not found
   * @retval others iterator for found member
   * @note If target name is a literal string, string_view can be optimized by
   * compiler. This function will provide a better memcmp implementation than
   * std::memcmp while length is not too large.
   */
  sonic_force_inline ConstMemberIterator FindMember(const char* key,
                                                    size_t len) const noexcept {
    return findMemberImpl(key, len);
  }

  using BaseNode::HasMember;

  /**
   * @brief Create a map to trace all members of this object.
   * @param alloc allocator that maintain this node's memory
   * @retval true successful
   * @retval false failed, which means that no memory can be allocated by
   * allocator.
   */
  bool CreateMap(Allocator& alloc) {
    sonic_assert(this->IsObject());
    sonic_assert(this->Capacity() >= this->Size());
    // Empty object: reserve meta storage first so children() is non-null.
    // If the reserve OOMs, children() stays null and we bail instead of
    // dereferencing it via getMapUnsafe() / setMap.
    if (nullptr == children()) {
      this->memberReserveImpl(16, alloc);
      if (nullptr == children()) return false;
    }
    if (getMapUnsafe()) return true;
    map_type* map = static_cast<map_type*>(alloc.Malloc(sizeof(map_type)));
    if (nullptr == map) return false;
    new (map) map_type(&alloc);
    MemberNode* m = (MemberNode*)getObjChildrenFirstUnsafe();
    if (!map->BuildFromMembers(m, this->Size())) {
      map->~map_type();
      alloc.Free(map);
      return false;
    }
    setMap(map);
    return true;
  }

  bool atJsonPathImpl(const internal::JsonPath& path, size_t index,
                      std::vector<DNode*>& res) {
    return atJsonPathImplCommon<DNode*>(this, path, index, res);
  }

  bool atJsonPathImpl(const internal::JsonPath& path, size_t index,
                      std::vector<const DNode*>& res) const {
    return atJsonPathImplCommon<const DNode*>(this, path, index, res);
  }

  /**
   * @brief Destroy the created map. This means that you don't want maintain the
   * map anymore.
   */
  void DestroyMap() {
    sonic_assert(this->IsObject());
    if (getMap()) {
      getMap()->~map_type();
      Allocator::Free(getMap());
      setMap(nullptr);
    }
  }

  using BaseNode::RemoveMember;

  /**
   * @brief Copy another node depthly.
   * @tparam SourceAllocator another-node's allocator type
   * @param rhs source node.
   * @param alloc reference of allocator which maintains this node's memory
   * @param copyString whether copy const string in source node, default is not.
   * @return DNode& reference to this node to support streaming APIs
   * @node This function will recursively. If json-tree is too depth, this
   * function maybe cause stackoveflow.
   */
  template <typename SourceAllocator>
  DNode& CopyFrom(const DNode<SourceAllocator>& rhs, Allocator& alloc,
                  bool copyString = false) {
    (void)TryCopyFrom(rhs, alloc, copyString);
    return *this;
  }

  template <typename SourceAllocator>
  bool TryCopyFrom(const DNode<SourceAllocator>& rhs, Allocator& alloc,
                   bool copyString = false) {
    if (sonic_unlikely(reinterpret_cast<const void*>(this) ==
                       reinterpret_cast<const void*>(&rhs))) {
      return true;
    }
    DNode temp;
    if (sonic_unlikely(!temp.initCopyFrom(rhs, alloc, copyString))) {
      return false;
    }
    this->destroy();
    rawAssign(temp);
    return true;
  }

  /**
   * @brief move another node to this.
   * @param rhs source node
   */

 private:
  static void destroyMembers(MemberNode* members, size_t count) {
    for (size_t i = 0; i < count; ++i) {
      members[i].~MemberNode();
    }
  }

  static void destroyElements(DNode* elements, size_t count) {
    for (size_t i = 0; i < count; ++i) {
      elements[i].~DNode();
    }
  }

  template <typename SourceAllocator>
  bool initCopyFrom(const DNode<SourceAllocator>& rhs, Allocator& alloc,
                    bool copyString) {
    switch (rhs.getBasicType()) {
      case kObject: {
        size_t count = rhs.Size();
        this->o.len = rhs.getTypeAndLen();
        if (count == 0) {
          setChildren(nullptr);
          return true;
        }
        void* mem = containerMalloc<MemberNode>(count, alloc);
        if (sonic_unlikely(mem == nullptr)) {
          this->setLength(0, kObject);
          setChildren(nullptr);
          return false;
        }
        MemberNode* members = reinterpret_cast<MemberNode*>(
            static_cast<char*>(mem) + sizeof(MetaNode));
        auto rhs_member = rhs.MemberBegin();
        size_t constructed = 0;
        for (size_t i = 0; i < count; ++i, ++rhs_member) {
          DNode copied_name;
          if (sonic_unlikely(!copied_name.initCopyFrom(rhs_member->name, alloc,
                                                       copyString))) {
            destroyMembers(members, constructed);
            Allocator::Free(mem);
            this->setLength(0, kObject);
            setChildren(nullptr);
            return false;
          }
          DNode copied_value;
          if (sonic_unlikely(!copied_value.initCopyFrom(rhs_member->value,
                                                        alloc, copyString))) {
            destroyMembers(members, constructed);
            Allocator::Free(mem);
            this->setLength(0, kObject);
            setChildren(nullptr);
            return false;
          }
          new (&members[i])
              MemberNode(std::move(copied_name), std::move(copied_value));
          ++constructed;
        }
        setChildren(mem);
        return true;
      }
      case kArray: {
        size_t count = rhs.Size();
        this->a.len = rhs.getTypeAndLen();
        if (count == 0) {
          setChildren(nullptr);
          return true;
        }
        void* mem = containerMalloc<DNode>(count, alloc);
        if (sonic_unlikely(mem == nullptr)) {
          this->setLength(0, kArray);
          setChildren(nullptr);
          return false;
        }
        DNode* elements = reinterpret_cast<DNode*>(static_cast<char*>(mem) +
                                                   sizeof(MetaNode));
        auto rhs_it = rhs.Begin();
        size_t constructed = 0;
        for (size_t i = 0; i < count; ++i, ++rhs_it) {
          new (&elements[i]) DNode();
          if (sonic_unlikely(
                  !elements[i].initCopyFrom(*rhs_it, alloc, copyString))) {
            elements[i].~DNode();
            destroyElements(elements, constructed);
            Allocator::Free(mem);
            this->setLength(0, kArray);
            setChildren(nullptr);
            return false;
          }
          ++constructed;
        }
        setChildren(mem);
        setCapacity(count);
        return true;
      }
      case kString: {
        this->sv.len = rhs.getTypeAndLen();
        if (rhs.GetType() != kStringConst || copyString) {
          return this->StringCopy(rhs.GetStringView().data(), rhs.Size(),
                                  alloc);
        }
        this->sv.p = rhs.GetStringView().data();
        return true;
      }
      case kNumber: {
        if (rhs.GetType() != kNumStr) {
          std::memcpy(&(this->data), &rhs, sizeof(this->data));
          return true;
        }
        [[fallthrough]];
      }
      case kRaw: {
        size_t len = rhs.Size();
        if (sonic_unlikely(len == std::numeric_limits<size_t>::max())) {
          this->sv.p = "";
          this->setLength(0, rhs.GetType());
          return false;
        }
        char* p = static_cast<char*>(alloc.Malloc(len + 1));
        if (sonic_unlikely(p == nullptr)) {
          this->sv.p = "";
          this->setLength(0, rhs.GetType());
          return false;
        }
        this->sv.len = rhs.getTypeAndLen() | kOwnedStringMask;
        this->sv.p = p;
        std::memcpy(p, rhs.GetStringView().data(), len);
        p[len] = '\0';
        return true;
      }
      default:
        std::memcpy(&(this->data), &rhs, sizeof(this->data));
        return true;
    }
  }

  using MSType = StringView;
#if defined(SONIC_STATIC_DISPATCH)
  struct Less {
    bool operator()(MSType s1, MSType s2) const {
      size_t n1 = s1.size(), n2 = s2.size();
      const size_t len = std::min(n1, n2);
      int cmp = internal::InlinedMemcmp(s1.data(), s2.data(), len);
      return cmp < 0 || (cmp == 0 && n1 < n2);
    }
  };
#else
  using Less = std::less<MSType>;
#endif

  struct map_type {
    struct Entry {
      MSType key;
      size_t index;
    };

    explicit map_type(Allocator* alloc) : alloc_{alloc} {}
    ~map_type() { Allocator::Free(entries_); }

    bool Reserve(size_t new_cap) {
      if (new_cap <= cap_) return true;
      if (sonic_unlikely(new_cap >
                         std::numeric_limits<size_t>::max() / sizeof(Entry))) {
        MarkOom(alloc_);
        return false;
      }
      void* mem = alloc_->Realloc(entries_, cap_ * sizeof(Entry),
                                  new_cap * sizeof(Entry));
      if (sonic_unlikely(mem == nullptr)) return false;
      entries_ = static_cast<Entry*>(mem);
      cap_ = new_cap;
      return true;
    }

    bool Insert(MSType key, size_t index) {
      if (sonic_unlikely(!ReserveForInsert())) return false;
      InsertNoGrow(key, index);
      return true;
    }

    bool ReserveForInsert() {
      if (size_ < cap_) return true;
      if (cap_ == 0) return Reserve(16);
      size_t inc = cap_ / 2 + (cap_ & 1);
      if (sonic_unlikely(cap_ > std::numeric_limits<size_t>::max() - inc)) {
        MarkOom(alloc_);
        return false;
      }
      return Reserve(cap_ + inc);
    }

    void InsertAfterReserve(MSType key, size_t index) {
      sonic_assert(size_ < cap_);
      InsertNoGrow(key, index);
    }

    bool BuildFromMembers(MemberNode* members, size_t count) {
      if (sonic_unlikely(!Reserve(count))) return false;
      size_ = 0;
      if (count == 0) return true;
      for (size_t i = 0; i < count; ++i) {
        entries_[i] = Entry{(members + i)->name.GetStringView(), i};
      }
      size_ = count;
      std::sort(entries_, entries_ + size_,
                [this](const Entry& lhs, const Entry& rhs) {
                  if (KeyLess(lhs.key, rhs.key)) return true;
                  if (KeyLess(rhs.key, lhs.key)) return false;
                  return lhs.index < rhs.index;
                });
      return true;
    }

    Entry* Find(MSType key) const {
      size_t pos = LowerBound(key);
      if (pos < size_ && KeyEqual(entries_[pos].key, key)) {
        return entries_ + pos;
      }
      return nullptr;
    }

    void Erase(Entry* entry) {
      size_t pos = static_cast<size_t>(entry - entries_);
      sonic_assert(pos < size_);
      if (pos + 1 < size_) {
        std::memmove(entries_ + pos, entries_ + pos + 1,
                     (size_ - pos - 1) * sizeof(Entry));
      }
      --size_;
    }

    bool ReplaceIndex(size_t old_index, MSType key, size_t new_index) {
      Entry* entry = FindByIndex(old_index);
      if (sonic_unlikely(entry == nullptr)) return false;
      Erase(entry);
      sonic_assert(size_ < cap_);
      InsertNoGrow(key, new_index);
      return true;
    }

   private:
    bool KeyLess(MSType lhs, MSType rhs) const { return Less{}(lhs, rhs); }
    bool KeyEqual(MSType lhs, MSType rhs) const {
      return !KeyLess(lhs, rhs) && !KeyLess(rhs, lhs);
    }

    size_t LowerBound(MSType key) const {
      size_t first = 0;
      size_t count = size_;
      while (count > 0) {
        size_t step = count / 2;
        size_t mid = first + step;
        if (KeyLess(entries_[mid].key, key)) {
          first = mid + 1;
          count -= step + 1;
        } else {
          count = step;
        }
      }
      return first;
    }

    size_t UpperBound(MSType key) const {
      size_t first = 0;
      size_t count = size_;
      while (count > 0) {
        size_t step = count / 2;
        size_t mid = first + step;
        if (!KeyLess(key, entries_[mid].key)) {
          first = mid + 1;
          count -= step + 1;
        } else {
          count = step;
        }
      }
      return first;
    }

    Entry* FindByIndex(size_t index) const {
      for (size_t i = 0; i < size_; ++i) {
        if (entries_[i].index == index) return entries_ + i;
      }
      return nullptr;
    }

    void InsertNoGrow(MSType key, size_t index) {
      size_t pos = LowerBound(Entry{key, index});
      if (pos < size_) {
        std::memmove(entries_ + pos + 1, entries_ + pos,
                     (size_ - pos) * sizeof(Entry));
      }
      entries_[pos] = Entry{key, index};
      ++size_;
    }

    bool EntryLess(const Entry& lhs, const Entry& rhs) const {
      if (KeyLess(lhs.key, rhs.key)) return true;
      if (KeyLess(rhs.key, lhs.key)) return false;
      return lhs.index < rhs.index;
    }

    size_t LowerBound(const Entry& entry) const {
      size_t first = 0;
      size_t count = size_;
      while (count > 0) {
        size_t step = count / 2;
        size_t mid = first + step;
        if (EntryLess(entries_[mid], entry)) {
          first = mid + 1;
          count -= step + 1;
        } else {
          count = step;
        }
      }
      return first;
    }

    template <typename A>
    static auto MarkOom(A* alloc, int = 0)
        -> decltype(alloc->MarkOom(), void()) {
      if (alloc) alloc->MarkOom();
    }
    static void MarkOom(...) {}

    Allocator* alloc_{nullptr};
    Entry* entries_{nullptr};
    size_t size_{0};
    size_t cap_{0};
  };

  struct MetaNode {
    size_t cap;
    map_type* map;

    ~MetaNode() {
      if (map) {
        map->~map_type();
        Allocator::Free(map);
      }
    }
    MetaNode() : cap{0}, map{nullptr} {}
    MetaNode(size_t n) : cap{n}, map{nullptr} {}
    void SetMetaCap(size_t n) { cap = n; }
  };

  // Set APIs
  DNode& setNullImpl() {
    this->destroy();
    this->setType(kNull);
    return *this;
  }

  DNode& setBoolImpl(bool b) {
    this->destroy();
    this->setType(b ? kTrue : kFalse);
    return *this;
  }

  DNode& setObjectImpl() {
    this->destroy();
    this->setType(kObject);
    setChildren(nullptr);
    return *this;
  }

  DNode& setArrayImpl() {
    this->destroy();
    this->setType(kArray);
    setChildren(nullptr);
    return *this;
  }

  DNode& setIntImpl(int i) {
    this->destroy();
    this->setType(i >= 0 ? kUint : kSint);
    this->n.i64 = i;
    return *this;
  }

  DNode& setUintImpl(unsigned int i) {
    this->destroy();
    this->setType(kUint);
    this->n.u64 = i;
    return *this;
  }

  DNode& setInt64Impl(int64_t i) {
    this->destroy();
    this->setType(i >= 0 ? kUint : kSint);
    this->n.i64 = i;
    return *this;
  }

  DNode& setUint64Impl(uint64_t i) {
    this->destroy();
    this->setType(kUint);
    this->n.u64 = i;
    return *this;
  }

  DNode& setDoubleImpl(double d) {
    this->destroy();
    this->setType(kReal);
    this->n.f64 = d;
    return *this;
  }

  DNode& setStringImpl(const char* s, size_t len) {
    this->destroy();
    if (sonic_likely(this->setLengthChecked(len, kStringConst))) {
      this->sv.p = s;
    } else {
      this->sv.p = "";
    }
    return *this;
  }

  DNode& setStringImpl(const char* s, size_t len, Allocator& alloc) {
    (void)trySetStringImpl(s, len, alloc);
    return *this;
  }

  bool trySetStringImpl(const char* s, size_t len, Allocator& alloc) {
    if (sonic_unlikely(len > BaseNode::kMaxStoredLength)) return false;
    char* p = static_cast<char*>(alloc.Malloc(len + 1));
    if (sonic_unlikely(p == nullptr)) return false;
    std::memcpy(p, s, len);
    p[len] = '\0';
    this->destroy();
    this->sv.p = p;
    this->setLength(len, kStringFree);
    return true;
  }

  DNode& setRawImpl(StringView s) { return setRawLikeImpl(s, kRaw); }

  DNode& setRawImpl(StringView s, Allocator& alloc) {
    return setRawLikeImpl(s, kRaw, alloc);
  }

  DNode& setStringNumberImpl(StringView s) {
    return setRawLikeImpl(s, kNumStr);
  }

  DNode& setStringNumberImpl(StringView s, Allocator& alloc) {
    return setRawLikeImpl(s, kNumStr, alloc);
  }

  bool trySetStringNumberImpl(StringView s, Allocator& alloc) {
    return trySetRawLikeImpl(s, kNumStr, alloc);
  }

  DNode& setRawLikeImpl(StringView s, TypeFlag typ) {
    this->destroy();
    this->raw.p = s.data();
    if (sonic_unlikely(!this->setLengthChecked(s.size(), typ))) {
      this->raw.p = "";
    }
    return *this;
  }

  DNode& setRawLikeImpl(StringView s, TypeFlag typ, Allocator& alloc) {
    (void)trySetRawLikeImpl(s, typ, alloc);
    return *this;
  }

  bool trySetRawLikeImpl(StringView s, TypeFlag typ, Allocator& alloc) {
    if (sonic_unlikely(s.size() > BaseNode::kMaxStoredLength)) return false;
    size_t len = s.size();
    char* p = static_cast<char*>(alloc.Malloc(len + 1));
    if (sonic_unlikely(p == nullptr)) return false;
    std::memcpy(p, s.data(), len);
    p[len] = '\0';
    this->destroy();
    this->raw.p = p;
    // Mark buffer as owned so destroy() will free it for kNeedFree alloc.
    this->setLength(len, static_cast<TypeFlag>(static_cast<uint64_t>(typ) |
                                               kOwnedStringMask));
    return true;
  }

  DNode& popBackImpl() {
    getArrChildrenFirstUnsafe()[this->Size() - 1].~DNode();
    this->subLength(1);
    return *this;
  }

  DNode& reserveImpl(size_t new_cap, Allocator& alloc) {
    if (new_cap > this->Capacity()) {
      void* mem = containerRealloc<DNode>(children(), this->Capacity(), new_cap,
                                          this->Size(), alloc);
      if (sonic_likely(mem != nullptr)) setChildren(mem);
    }
    return *this;
  }

  ValueIterator beginImpl() noexcept {
    return ValueIterator(getArrChildrenFirst());
  }

  ConstValueIterator cbeginImpl() const noexcept {
    return ConstValueIterator(getArrChildrenFirst());
  }

  ValueIterator endImpl() noexcept {
    return ValueIterator(getArrChildrenFirst()) + this->Size();
  }

  ConstValueIterator cendImpl() const noexcept {
    return ConstValueIterator(getArrChildrenFirst()) + this->Size();
  }

  DNode& backImpl() const noexcept {
    return *(getArrChildrenFirst() + this->Size() - 1);
  }

  size_t capacityImpl() const noexcept {
    return children() != nullptr ? meta()->cap : 0;
  }

  template <typename ResPtr, typename SelfPtr>
  static bool atJsonPathImplCommon(SelfPtr self, const internal::JsonPath& path,
                                   size_t index, std::vector<ResPtr>& res) {
    static_assert(std::is_pointer<ResPtr>::value,
                  "ResPtr must be a pointer type");
    if (index >= path.size()) {
      res.push_back(reinterpret_cast<ResPtr>(self));
      return true;
    }

    if (path[index].is_wildcard()) {
      // select nothing from the primitive JSON value
      if (!self->IsObject() && !self->IsArray()) {
        return true;
      }
      if (self->IsObject()) {
        auto it = self->MemberBegin();
        for (size_t i = 0; i < self->Size(); ++i, ++it) {
          auto* cur = reinterpret_cast<ResPtr>(&it->value);
          atJsonPathImplCommon<ResPtr>(cur, path, index + 1, res);
        }
      } else {
        auto it = self->Begin();
        for (size_t i = 0; i < self->Size(); ++i, ++it) {
          auto* cur = reinterpret_cast<ResPtr>(&*it);
          atJsonPathImplCommon<ResPtr>(cur, path, index + 1, res);
        }
      }
      return true;
    }

    if (path[index].is_key()) {
      if (!self->IsObject()) {
        return false;
      }
      auto m = self->FindMember(path[index].key());
      if (m != self->MemberEnd()) {
        auto* child =
            reinterpret_cast<std::remove_pointer_t<ResPtr>*>(&m->value);
        return atJsonPathImplCommon<ResPtr>(child, path, index + 1, res);
      }
      return false;
    }

    if (path[index].is_index()) {
      if (!self->IsArray()) {
        return false;
      }

      // index maybe negative
      int64_t idx = path[index].index();
      if (idx < 0) {
        idx = self->Size() + idx;
      }

      if (idx >= int64_t(self->Size()) || idx < 0) {
        return false;
      }
      auto& child_ref = self->findValueImpl(size_t(idx));
      auto* child =
          reinterpret_cast<std::remove_pointer_t<ResPtr>*>(&child_ref);
      return atJsonPathImplCommon<ResPtr>(child, path, index + 1, res);
    }
    return false;
  }

  DNode& memberReserveImpl(size_t new_cap, Allocator& alloc) {
    if (new_cap > this->Capacity()) {
      void* old_ptr = children();
      size_t old_cap = this->Capacity();
      void* mem = containerRealloc<MemberNode>(old_ptr, old_cap, new_cap,
                                               this->Size(), alloc);
      if (sonic_likely(mem != nullptr)) {
        setChildren(mem);
        if (old_cap == 0) {
          setMap(nullptr);  // Set map as nullptr when first alloc memory.
        }
      }
    }
    return *this;
  }

  MemberIterator memberBeginImpl() noexcept {
    return MemberIterator(getObjChildrenFirst());
  }

  ConstMemberIterator cmemberBeginImpl() const noexcept {
    return ConstMemberIterator(getObjChildrenFirst());
  }

  MemberIterator memberEndImpl() noexcept {
    return MemberIterator(getObjChildrenFirst()) + this->Size();
  }

  ConstMemberIterator cmemberEndImpl() const noexcept {
    return ConstMemberIterator(getObjChildrenFirst()) + this->Size();
  }

  MemberIterator memberBeginUnsafe() const {
    sonic_assert(this->IsObject());
    return (MemberNode*)getObjChildrenFirstUnsafe();
  }

  MemberIterator memberEndUnsafe() const {
    sonic_assert(this->IsObject());
    return (MemberNode*)getObjChildrenFirstUnsafe() + this->Size();
  }

  template <typename T>
  sonic_force_inline void* containerMalloc(size_t cap, Allocator& alloc) {
    if (sonic_unlikely(cap >
                       (std::numeric_limits<size_t>::max() - sizeof(MetaNode)) /
                           sizeof(T))) {
      markAllocatorOom(alloc);
      return nullptr;
    }
    size_t alloc_size = cap * sizeof(T) + sizeof(MetaNode);
    void* mem = alloc.Malloc(alloc_size);
    if (sonic_likely(mem != nullptr)) {
      new (static_cast<MetaNode*>(mem)) MetaNode(cap);
    }
    return mem;
  }

  template <typename T>
  sonic_force_inline void* containerRealloc(void* old_ptr, size_t old_cap,
                                            size_t new_cap, size_t count,
                                            Allocator& alloc) {
    if (sonic_unlikely(
            old_cap > (std::numeric_limits<size_t>::max() - sizeof(MetaNode)) /
                          sizeof(T) ||
            new_cap > (std::numeric_limits<size_t>::max() - sizeof(MetaNode)) /
                          sizeof(T))) {
      markAllocatorOom(alloc);
      return nullptr;
    }
    size_t new_size = new_cap * sizeof(T) + sizeof(MetaNode);
    void* mem = alloc.Malloc(new_size);
    if (sonic_unlikely(mem == nullptr)) return nullptr;
    auto* new_meta = static_cast<MetaNode*>(mem);
    new (new_meta) MetaNode(new_cap);
    if (old_ptr != nullptr) {
      auto* old_meta = static_cast<MetaNode*>(old_ptr);
      new_meta->map = old_meta->map;
      old_meta->map = nullptr;
      relocateContainer<T>(
          reinterpret_cast<T*>(reinterpret_cast<char*>(mem) + sizeof(MetaNode)),
          reinterpret_cast<T*>(reinterpret_cast<char*>(old_ptr) +
                               sizeof(MetaNode)),
          count);
      old_meta->~MetaNode();
      Allocator::Free(old_ptr);
    }
    return mem;
  }

  template <typename T>
  static void relocateContainer(T* dst, T* src, size_t count) {
    if constexpr (std::is_same<T, MemberNode>::value) {
      for (size_t i = 0; i < count; ++i) {
        new (&dst[i]) MemberNode(std::move(src[i].mutableName()),
                                 std::move(src[i].value));
        src[i].~MemberNode();
      }
    } else {
      for (size_t i = 0; i < count; ++i) {
        new (&dst[i]) DNode(std::move(src[i]));
        src[i].~DNode();
      }
    }
  }

  sonic_force_inline void* children() const {
    sonic_assert(this->IsContainer());
    return this->a.next.children;
  }

  sonic_force_inline MetaNode* meta() const {
    sonic_assert(this->IsContainer());
    return (MetaNode*)(this->a.next.children);
  }

  sonic_force_inline DNode* getArrChildrenFirst() const {
    sonic_assert(this->IsArray());
    if (nullptr == children()) {
      return nullptr;
    }
    return (DNode*)((char*)this->a.next.children +
                    sizeof(MetaNode) / sizeof(char));
  }

  sonic_force_inline DNode* getArrChildrenFirstUnsafe() const {
    sonic_assert(this->IsArray());
    return (DNode*)((char*)this->a.next.children +
                    sizeof(MetaNode) / sizeof(char));
  }

  sonic_force_inline DNode* getChildrenFirstUnsafe() const {
    return (DNode*)((char*)this->a.next.children +
                    sizeof(MetaNode) / sizeof(char));
  }

  sonic_force_inline bool pointsIntoChildren(const DNode* p) const {
    if (sonic_unlikely(!this->IsContainer() || children() == nullptr)) {
      return false;
    }
    const uintptr_t addr = reinterpret_cast<uintptr_t>(p);
    const uintptr_t first = reinterpret_cast<uintptr_t>(
        this->IsObject() ? static_cast<void*>(getObjChildrenFirstUnsafe())
                         : static_cast<void*>(getArrChildrenFirstUnsafe()));
    const size_t bytes = this->IsObject() ? this->Size() * sizeof(MemberNode)
                                          : this->Size() * sizeof(DNode);
    const uintptr_t last = first + bytes;
    return addr >= first && addr < last;
  }

  sonic_force_inline MemberNode* getObjChildrenFirst() const {
    sonic_assert(this->IsObject());
    if (nullptr == children()) {
      return nullptr;
    }
    return reinterpret_cast<MemberNode*>(
        reinterpret_cast<char*>(this->a.next.children) + sizeof(MetaNode));
  }

  sonic_force_inline MemberNode* getObjChildrenFirstUnsafe() const {
    sonic_assert(this->IsObject());
    return reinterpret_cast<MemberNode*>(
        reinterpret_cast<char*>(this->a.next.children) + sizeof(MetaNode));
  }

  sonic_force_inline void setChildren(void* new_child) {
    sonic_assert(this->IsContainer());
    this->o.next.children = new_child;
    return;
  }

  sonic_force_inline void setCapacity(size_t new_cap) {
    sonic_assert(this->IsContainer());
    sonic_assert(this->o.next.children != nullptr);
    // first node is meta node
    ((MetaNode*)(this->o.next.children))->cap = new_cap;
  }

  template <typename A>
  static auto markAllocatorOom(A& alloc, int = 0)
      -> decltype(alloc.MarkOom(), void()) {
    alloc.MarkOom();
  }
  static void markAllocatorOom(...) {}

  sonic_force_inline void setMap(map_type* new_map) {
    sonic_assert(this->IsObject());
    sonic_assert(this->o.next.children != nullptr);
    ((MetaNode*)(this->o.next.children))->map = new_map;
  }

  sonic_force_inline map_type* getMap() const {
    sonic_assert(this->IsObject());
    if (nullptr == children()) return nullptr;
    return ((MetaNode*)(this->o.next.children))->map;
  }

  sonic_force_inline map_type* getMapUnsafe() const {
    sonic_assert(this->IsObject());
    return ((MetaNode*)(this->o.next.children))->map;
  }

  sonic_force_inline MemberIterator findFromMap(StringView key) {
    auto* it = getMap()->Find(MSType(key.data(), key.size()));
    if (it != nullptr) {
      return memberBeginUnsafe() + it->index;
    }
    return memberEndUnsafe();
  }

  sonic_force_inline ConstMemberIterator findFromMap(StringView key) const {
    auto* it = getMap()->Find(MSType(key.data(), key.size()));
    if (it != nullptr) {
      return cmemberBeginImpl() + it->index;
    }
    return cmemberEndImpl();
  }

  sonic_force_inline MemberIterator findMemberImpl(StringView key) {
    if (nullptr != getMap()) {
      return findFromMap(key);
    }
    auto it = this->MemberBegin();
    for (auto e = this->MemberEnd(); it != e; ++it) {
      if (it->name.GetStringView() == key) {
        break;
      }
    }
    return it;
  }

  sonic_force_inline ConstMemberIterator findMemberImpl(StringView key) const {
    if (nullptr != getMap()) {
      return findFromMap(key);
    }
    auto it = this->MemberBegin();
    for (auto e = this->MemberEnd(); it != e; ++it) {
      if (it->name.GetStringView() == key) {
        break;
      }
    }
    return it;
  }

  sonic_force_inline MemberIterator findMemberImpl(const char* key,
                                                   size_t len) {
    /**************************************************
     * Only calling internal memcmp when static dispatch.
     * Dynamic dispatch will have indirect call.
     **************************************************/
#if defined(SONIC_STATIC_DISPATCH)
    if (nullptr != getMap()) {
      return findFromMap(StringView(key, len));
    }
    auto it = this->MemberBegin();
    for (auto e = this->MemberEnd(); it != e; ++it) {
      auto name_sv = it->name.GetStringView();
      if (name_sv.size() == len &&
          internal::InlinedMemcmpEq(name_sv.data(), key, len)) {
        break;
      }
    }
    return it;
#else
    return findMemberImpl(StringView(key, len));
#endif
  }

  sonic_force_inline ConstMemberIterator findMemberImpl(const char* key,
                                                        size_t len) const {
#if defined(SONIC_STATIC_DISPATCH)
    if (nullptr != getMap()) {
      return findFromMap(StringView(key, len));
    }
    auto it = this->MemberBegin();
    for (auto e = this->MemberEnd(); it != e; ++it) {
      auto name_sv = it->name.GetStringView();
      if (name_sv.size() == len &&
          internal::InlinedMemcmpEq(name_sv.data(), key, len)) {
        break;
      }
    }
    return it;
#else
    return findMemberImpl(StringView(key, len));
#endif
  }

  sonic_force_inline DNode& findValueImpl(StringView key) noexcept {
    auto m = findMemberImpl(key);
    if (m != this->MemberEnd()) {
      return m->value;
    }
    static thread_local DNode tmp{};
    tmp.SetNull();
    return tmp;
  }

  sonic_force_inline const DNode& findValueImpl(StringView key) const noexcept {
    auto m = findMemberImpl(key);
    if (m != this->MemberEnd()) {
      return m->value;
    }
    static const DNode tmp{};
    return tmp;
  }

  DNode& findValueImpl(size_t idx) const noexcept {
    return *(getArrChildrenFirst() + idx);
  }

  MemberIterator addMemberImpl(StringView key, DNode& value, Allocator& alloc,
                               bool copyKey) {
    constexpr size_t k_default_obj_cap = 16;
    size_t count = this->Size();
    DNode name;
    if (copyKey) {
      if (sonic_unlikely(!name.StringCopy(key.data(), key.size(), alloc))) {
        return this->MemberEnd();
      }
    } else {
      name.SetString(key);
      if (sonic_unlikely(name.IsNull())) {
        return this->MemberEnd();
      }
    }
    map_type* map = getMap();
    if (map && sonic_unlikely(!map->ReserveForInsert())) {
      return this->MemberEnd();
    }

    DNode moved_value;
    DNode* value_to_add = &value;
    bool moved_alias = false;
    const bool need_grow = count >= this->Capacity();
    if (need_grow && pointsIntoChildren(&value)) {
      moved_alias = true;
      moved_value.rawAssign(value);
      value_to_add = &moved_value;
    }
    if (count >= this->Capacity()) {
      if (this->Capacity() == 0) {
        void* mem = containerMalloc<MemberNode>(k_default_obj_cap, alloc);
        if (sonic_unlikely(mem == nullptr)) {
          if (moved_alias) value.rawAssign(moved_value);
          return this->MemberEnd();
        }
        setChildren(mem);
      } else {
        size_t cap = this->Capacity();
        size_t inc = cap / 2 + (cap & 1);
        if (sonic_unlikely(cap > std::numeric_limits<size_t>::max() - inc)) {
          markAllocatorOom(alloc);
          if (moved_alias) value.rawAssign(moved_value);
          return this->MemberEnd();
        }
        cap += inc;  // grow by factor 1.5
        void* old_ptr = children();
        void* mem = containerRealloc<MemberNode>(old_ptr, this->Capacity(), cap,
                                                 count, alloc);
        if (sonic_unlikely(mem == nullptr)) {
          if (moved_alias) value.rawAssign(moved_value);
          return this->MemberEnd();
        }
        setChildren(mem);
      }
    }
    if (map) map->InsertAfterReserve(name.GetStringView(), count);
    MemberNode* last = memberBeginUnsafe() + count;
    DNode member_name;
    member_name.rawAssign(name);
    DNode member_value;
    member_value.rawAssign(*value_to_add);
    new (last) MemberNode(std::move(member_name), std::move(member_value));
    this->addLength(1);
    return last;
  }

  sonic_force_inline bool removeMemberImpl(StringView key) {
    MemberIterator m;
    typename map_type::Entry* map_entry = nullptr;
    if (nullptr == children()) {
      goto not_find;
    }
    if (getMapUnsafe()) {
      map_entry = getMapUnsafe()->Find(MSType(key.data(), key.size()));
      if (map_entry != nullptr) {
        m = memberBeginUnsafe() + map_entry->index;
        goto find;
      }

      goto not_find;
    } else {
      m = memberBeginUnsafe();
      for (; m != memberEndUnsafe(); ++m)  // {
        if (m->name.GetStringView() == key) goto find;

      goto not_find;
    }
  find : {
    MemberIterator m_tail = memberBeginUnsafe() + (this->Size() - 1);
    if (m != m_tail) {
      // maintain map
      map_type* map = getMap();
      if (map) {
        map->Erase(map_entry);
      }
      m->~MemberNode();
      new (m) MemberNode(std::move(m_tail->mutableName()),
                         std::move(m_tail->value));
      if (map && !map->ReplaceIndex(this->Size() - 1, m->name.GetStringView(),
                                    size_t(m - memberBeginUnsafe()))) {
        DestroyMap();
      }
      m_tail->~MemberNode();
    } else {
      if (map_entry != nullptr) getMapUnsafe()->Erase(map_entry);
      m->~MemberNode();
    }

    this->subLength(1);
    return true;
  }
  not_find:
    return false;
  }

  MemberIterator eraseMemberImpl(MemberIterator first, MemberIterator last) {
    // Destroy map before removing members.
    DestroyMap();
    size_t size = this->Size();
    if (first == last) return first;
    MemberIterator end = this->MemberEnd();
    if (size_t(last - first) >= size) {
      destroy();
      setChildren(nullptr);
      this->subLength(size);
      return this->MemberEnd();
    }
    for (MemberIterator it = first; it != last; ++it) {
      it->~MemberNode();
    }
    MemberIterator dst = first;
    for (MemberIterator src = last; src != end; ++src, ++dst) {
      new (dst)
          MemberNode(std::move(src->mutableName()), std::move(src->value));
      src->~MemberNode();
    }
    this->subLength(last - first);
    return first;
  }

  DNode& pushBackImpl(DNode& value, Allocator& alloc) {
    constexpr size_t k_default_array_cap = 16;
    sonic_assert(this->IsArray());
    // reserve capacity
    size_t cap = this->Capacity();
    DNode moved_value;
    DNode* value_to_add = &value;
    if (this->Size() >= cap && pointsIntoChildren(&value)) {
      moved_value.rawAssign(value);
      value_to_add = &moved_value;
    }
    if (this->Size() >= cap) {
      size_t new_cap = k_default_array_cap;
      if (cap) {
        size_t inc = cap / 2 + (cap & 1);
        if (sonic_unlikely(cap > std::numeric_limits<size_t>::max() - inc)) {
          markAllocatorOom(alloc);
          if (value_to_add == &moved_value) value.rawAssign(moved_value);
          return *this;
        }
        new_cap = cap + inc;
      }
      void* old_ptr = this->a.next.children;
      void* new_ptr =
          containerRealloc<DNode>(old_ptr, cap, new_cap, this->Size(), alloc);
      if (sonic_unlikely(new_ptr == nullptr)) {
        if (value_to_add == &moved_value) value.rawAssign(moved_value);
        return *this;
      }
      this->a.next.children = new_ptr;
    }
    // add value to the last pos
    DNode* last = this->End();
    new (last) DNode();
    last->rawAssign(*value_to_add);
    this->addLength(1);
    return *this;
  }

  ValueIterator eraseImpl(ValueIterator start, ValueIterator end) {
    if (start == end) return start;
    sonic_assert(this->IsArray());
    sonic_assert(start <= end);
    sonic_assert(start >= this->Begin());
    sonic_assert(end <= this->End());

    ValueIterator pos = this->Begin() + (start - this->Begin());
    for (ValueIterator it = pos; it != end; ++it) it->~DNode();
    ValueIterator dst = pos;
    ValueIterator old_end = this->End();
    for (ValueIterator src = end; src != old_end; ++src, ++dst) {
      new (dst) DNode(std::move(*src));
      src->~DNode();
    }
    this->subLength(end - start);
    return start;
  }

  template <SerializeFlags serializeFlags = SerializeFlags::kSerializeDefault>
  SonicError serializeImpl(WriteBuffer& wb) const {
    return internal::SerializeImpl<serializeFlags>(this, wb);
  }

  sonic_force_inline DNode* nextImpl() { return this + 1; }

  sonic_force_inline const DNode* cnextImpl() const { return this + 1; }

  // delete DNode's children or copied string
  void destroy() {
    if (!Allocator::kNeedFree) {
      return;
    }

    // Free owned string buffers for Raw / NumStr.
    // Note: We use the extra ownership bit in the 8-bit type info
    // (kOwnedStringMask) while keeping GetType() stable (it masks by
    // kSubTypeMask).
    const uint8_t info = static_cast<uint8_t>(this->sv.len & kInfoMask);
    if ((info & kOwnedStringMask) != 0) {
      if (this->getBasicType() == kRaw || this->GetType() == kNumStr) {
        Allocator::Free((void*)this->sv.p);
        return;
      }
    }

    switch (this->GetType()) {
      case kObject: {
        if (children()) {
          MemberNode* member =
              reinterpret_cast<MemberNode*>(getObjChildrenFirstUnsafe());
          MemberNode* e = member + this->Size();
          for (; member < e; ++member) {
            member->~MemberNode();
          }
          static_cast<MetaNode*>(children())->~MetaNode();
        }
        Allocator::Free(children());
        break;
      }
      case kArray: {
        if (children()) {
          for (auto it = this->Begin(), e = this->End(); it != e; ++it) {
            it->~DNode();
          }
          static_cast<MetaNode*>(children())->~MetaNode();
        }
        Allocator::Free(children());
        break;
      }
      case kStringFree:
        Allocator::Free((void*)(this->sv.p));
        break;
      default:
        break;
    }
  }

  sonic_force_inline void rawAssign(DNode& rhs) {
    this->data = rhs.data;
    rhs.setType(kNull);
  }

  DNode& clearImpl() {
    this->destroy();
    this->setLength(0);
    setChildren(nullptr);
    return *this;
  }
  sonic_force_inline uint64_t getTypeAndLen() const { return this->sv.len; }
};

using Node = DNode<SONIC_DEFAULT_ALLOCATOR>;

template <typename Allocator>
struct NodeTraits<DNode<Allocator>> {
  using alloc_type = Allocator;
  using NodeType = DNode<Allocator>;
  using MemberNode = MemberNodeT<NodeType>;
  using MemberIterator = MemberNode*;
  using ConstMemberIterator = const MemberNode*;
  using ValueIterator = NodeType*;
  using ConstValueIterator = const NodeType*;
};

}  // namespace sonic_json
