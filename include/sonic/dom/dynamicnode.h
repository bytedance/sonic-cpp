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

#include <map>
#include <type_traits>
#include <utility>

#include "sonic/allocator.h"
#include "sonic/dom/genericnode.h"
#include "sonic/dom/handler.h"
#include "sonic/dom/serialize.h"
#include "sonic/dom/type.h"
#include "sonic/error.h"
#include "sonic/internal/ftoa.h"
#include "sonic/internal/itoa.h"
#include "sonic/internal/quote.h"
#include "sonic/writebuffer.h"

namespace sonic_json {

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

  friend BaseNode;
  template <typename>
  friend class DNode;
  template <unsigned serializeFlags, typename NodeType>
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
    using rhsNodeType = DNode<SourceAllocator>;
    switch (rhs.getBasicType()) {
      case kObject: {
        size_t count = rhs.Size();
        this->o.len = rhs.getTypeAndLen();  // Copy size and type.
        if (count > 0) {
          void* mem = containerMalloc<MemberNode>(count, alloc);
          rhsNodeType* rn = rhs.getObjChildrenFirst();
          DNode* ln = (DNode*)((char*)mem + sizeof(MetaNode));
          for (size_t i = 0; i < count * 2; i += 2) {
            new (ln + i) DNode(*(rn + i), alloc, copyString);
            new (ln + i + 1) DNode(*(rn + i + 1), alloc, copyString);
          }
          setChildren(mem);
        } else {
          setChildren(nullptr);
        }
        break;
      }
      case kArray: {
        size_t a_size = rhs.Size();
        this->a.len = rhs.getTypeAndLen();  // Copy size and type.
        if (a_size > 0) {
          rhsNodeType* rn = rhs.getArrChildrenFirst();
          void* mem = containerMalloc<DNode>(a_size, alloc);
          DNode* ln = (DNode*)((char*)mem + sizeof(MetaNode));
          for (size_t i = 0; i < a_size; ++i) {
            new (ln + i) DNode(*(rn + i), alloc, copyString);
          }
          setChildren(mem);
          setCapacity(a_size);
        } else {
          setChildren(nullptr);
        }
        break;
      }
      case kString: {
        this->sv.len = rhs.getTypeAndLen();  // Copy size and type.
        if (rhs.GetType() != kStringConst || copyString) {
          this->StringCopy(rhs.GetStringView().data(), rhs.Size(), alloc);
        } else {
          this->sv.p = rhs.GetStringView().data();
        }
        break;
      }
      default:
        std::memcpy(&(this->data), &rhs, sizeof(this->data));
        break;
    }
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
      // could be used after free if it's an sub-node of "this",
      // hence the temporary danse.
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
    // if (this->Size() == 0) return false;
    if (nullptr == children()) {
      this->memberReserveImpl(16, alloc);
    }
    if (getMapUnsfe()) return true;
    map_type* map = static_cast<map_type*>(alloc.Malloc(sizeof(map_type)));
    new (map) map_type(MAType(&alloc));
    // SetMap(map);
    MemberNode* m = (MemberNode*)getObjChildrenFirstUnsafe();
    for (size_t i = 0; i < this->Size(); ++i) {
      map->emplace(std::make_pair((m + i)->name.GetStringView(), i));
    }
    setMap(map);
    return true;
  }

  /**
   * @brief Destory the created map. This means that you don't want maintain the
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
    this->destroy();
    new (this) DNode(rhs, alloc, copyString);
    return *this;
  }

  /**
   * @brief move another node to this.
   * @param rhs source node
   */

 private:
  using MSType = StringView;
  using MAType = MapAllocator<std::pair<const MSType, size_t>, Allocator>;
  using map_type = std::multimap<MSType, size_t, std::less<MSType>, MAType>;

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
    new (this) BaseNode(kNull);
    return *this;
  }

  DNode& setBoolImpl(bool b) {
    this->destroy();
    new (this) BaseNode(b);
    return *this;
  }

  DNode& setObjectImpl() {
    this->destroy();
    new (this) BaseNode(kObject);
    setChildren(nullptr);
    return *this;
  }

  DNode& setArrayImpl() {
    this->destroy();
    new (this) BaseNode(kArray);
    setChildren(nullptr);
    return *this;
  }

  DNode& setIntImpl(int i) {
    this->destroy();
    new (this) BaseNode(i);
    return *this;
  }

  DNode& setUintImpl(unsigned int i) {
    this->destroy();
    new (this) BaseNode(i);
    return *this;
  }

  DNode& setInt64Impl(int64_t i) {
    this->destroy();
    new (this) BaseNode(i);
    return *this;
  }

  DNode& setUint64Impl(uint64_t i) {
    this->destroy();
    new (this) BaseNode(i);
    return *this;
  }

  DNode& setDoubleImpl(double d) {
    this->destroy();
    new (this) BaseNode(d);
    return *this;
  }

  DNode& setStringImpl(const char* s, size_t len) {
    this->destroy();
    new (this) BaseNode(s, len);
    return *this;
  }

  DNode& setStringImpl(const char* s, size_t len, Allocator& alloc) {
    this->destroy();
    new (this) BaseNode(s, len, alloc);
    return *this;
  }

  DNode& setRawImpl(const char* s, size_t len) {
    this->destroy();
    this->raw.p = s;
    this->setLength(len, kRaw);
    return *this;
  }

  DNode& popBackImpl() {
    getArrChildrenFirstUnsafe()[this->Size() - 1].~DNode();
    this->subLength(1);
    return *this;
  }

  DNode& reserveImpl(size_t new_cap, Allocator& alloc) {
    if (new_cap > this->Capacity()) {
      setChildren(containerRealloc<DNode>(children(), this->Capacity(), new_cap,
                                          alloc));
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

  DNode& memberReserveImpl(size_t new_cap, Allocator& alloc) {
    if (new_cap > this->Capacity()) {
      void* old_ptr = children();
      size_t old_cap = this->Capacity();
      setChildren(
          containerRealloc<MemberNode>(old_ptr, old_cap, new_cap, alloc));
      if (old_cap == 0) {
        setMap(nullptr);  // Set map as nullptr when first alloc memory.
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
    size_t alloc_size = cap * sizeof(T) + sizeof(MetaNode);
    void* mem = alloc.Malloc(alloc_size);
    // init Metanode
    MetaNode* meta = static_cast<MetaNode*>(mem);
    new (meta) MetaNode(cap);

    return mem;
  }

  template <typename T>
  sonic_force_inline void* containerRealloc(void* old_ptr, size_t old_cap,
                                            size_t new_cap, Allocator& alloc) {
    size_t old_size = old_cap * sizeof(T) + sizeof(MetaNode);
    size_t new_size = new_cap * sizeof(T) + sizeof(MetaNode);
    void* mem = alloc.Realloc(old_ptr, old_size, new_size);
    // init Metanode
    MetaNode* meta = static_cast<MetaNode*>(mem);
    meta->SetMetaCap(new_cap);

    return mem;
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

  sonic_force_inline DNode* getObjChildrenFirst() const {
    sonic_assert(this->IsObject());
    if (nullptr == children()) {
      return nullptr;
    }
    return (DNode*)((char*)this->a.next.children +
                    sizeof(MetaNode) / sizeof(char));
  }

  sonic_force_inline DNode* getObjChildrenFirstUnsafe() const {
    sonic_assert(this->IsObject());
    return (DNode*)((char*)this->a.next.children +
                    sizeof(MetaNode) / sizeof(char));
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

  sonic_force_inline map_type* getMapUnsfe() const {
    sonic_assert(this->IsObject());
    return ((MetaNode*)(this->o.next.children))->map;
  }

  sonic_force_inline MemberIterator findMemberImpl(StringView key) const {
    if (nullptr != getMap()) {
      auto it = getMap()->find(MSType(key.data(), key.size()));
      if (it != getMap()->end()) {
        return memberBeginUnsafe() + it->second;
      }
      return memberEndUnsafe();
    }
    auto it = this->MemberBegin();
    for (auto e = this->MemberEnd(); it != e; ++it) {
      if (it->name.GetStringView() == key) {
        break;
      }
    }
    return const_cast<MemberIterator>(it);
  }

  sonic_force_inline DNode& findValueImpl(StringView key) const noexcept {
    auto m = findMemberImpl(key);
    if (m != this->MemberEnd()) {
      return m->value;
    }
    static DNode tmp{};
    tmp.SetNull();
    return tmp;
  }

  DNode& findValueImpl(size_t idx) const noexcept {
    return *(getArrChildrenFirst() + idx);
  }

  MemberIterator addMemberImpl(StringView key, DNode& value, Allocator& alloc,
                               bool copyKey) {
    constexpr size_t k_default_obj_cap = 16;
    size_t count = this->Size();
    if (count >= this->Capacity()) {
      if (this->Capacity() == 0) {
        setChildren(containerMalloc<MemberNode>(k_default_obj_cap, alloc));
      } else {
        size_t cap = this->Capacity();
        cap += (cap + 1) / 2;  // grow by factor 1.5
        void* old_ptr = children();
        setChildren(containerRealloc<MemberNode>(old_ptr, this->Capacity(), cap,
                                                 alloc));
      }
    }

    // add member to the last pos
    DNode name;
    if (copyKey) {
      name.SetString(key, alloc);
    } else {
      name.SetString(key);
    }
    DNode* last = this->getObjChildrenFirst() + count * 2;
    last->rawAssign(name);         // MemberEnd()->name
    (last + 1)->rawAssign(value);  // MemberEnd()->value
    this->addLength(1);

    // maintain map
    if (nullptr != getMap()) {
      // If key exists, it will be still keeped in vector but replaced in map.
      getMap()->emplace(std::make_pair(last->GetStringView(), count));
    }
    return (MemberIterator)last;
  }

  sonic_force_inline bool removeMemberImpl(StringView key) {
    MemberIterator m;
    if (nullptr == children()) {
      goto not_find;
    }
    if (getMapUnsfe()) {
      auto it = getMapUnsfe()->find(MSType(key.data(), key.size()));
      if (it != getMapUnsfe()->end()) {
        m = memberBeginUnsafe() + it->second;
        getMapUnsfe()->erase(it);
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
    // TODO: destroy() then memcpy.
    if (m != m_tail) {
      DNode* m_name = (DNode*)(&(m->name));
      DNode* tail_name = (DNode*)(&(m_tail->name));
      *m_name = std::move(*tail_name);
      m->value = std::move(m_tail->value);
      // maintain map
      map_type* map = getMap();
      if (map) {
        size_t pos = m - memberBeginUnsafe();
        // erase tail
        auto range =
            map->equal_range(m->name.GetStringView());  // already moved.
        for (auto i = range.first; i != range.second; ++i) {
          if (i->second == this->Size() - 1) {
            map->erase(i);
            break;  // only one erase.
          }
        }
        map->emplace(std::make_pair(m->name.GetStringView(), pos));
      }
    } else {
      m->name.~DNode();
      m->value.~DNode();
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
    MemberIterator end = this->MemberEnd();
    if (size_t(last - first) >= size) {
      destroy();
      setChildren(nullptr);
      this->subLength(size);
      return this->MemberEnd();
    }
    for (MemberIterator it = first; it != last; ++it) {
      it->name.~DNode();
      it->value.~DNode();
    }
    if (first != last || last != end) {
      std::memmove(static_cast<void*>(&(*first)), static_cast<void*>(&(*last)),
                   sizeof(MemberNode) * (end - last));
    }
    this->subLength(last - first);
    return first;
  }

  DNode& pushBackImpl(DNode& value, Allocator& alloc) {
    constexpr size_t k_default_array_cap = 16;
    sonic_assert(this->IsArray());
    // reseve capacity
    size_t cap = this->Capacity();
    if (this->Size() >= cap) {
      size_t new_cap = cap ? cap + (cap + 1) / 2 : k_default_array_cap;
      void* old_ptr = this->a.next.children;
      DNode* new_child =
          (DNode*)containerRealloc<DNode>(old_ptr, cap, new_cap, alloc);
      this->a.next.children = new_child;
    }
    // add value to the last pos
    DNode& last = *(this->End());
    last.rawAssign(value);
    this->addLength(1);
    return *this;
  }

  ValueIterator eraseImpl(ValueIterator start, ValueIterator end) {
    sonic_assert(this->IsArray());
    sonic_assert(start <= end);
    sonic_assert(start >= this->Begin());
    sonic_assert(end <= this->End());

    ValueIterator pos = this->Begin() + (start - this->Begin());
    for (ValueIterator it = pos; it != end; ++it) it->~DNode();
    std::memmove(static_cast<void*>(pos), end,
                 (this->End() - end) * sizeof(DNode));
    this->subLength(end - start);
    return start;
  }

  template <unsigned serializeFlags = kSerializeDefault>
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
    switch (this->GetType()) {
      case kObject: {
        if (children()) {
          DNode* node = getObjChildrenFirstUnsafe();
          DNode* e = node + this->Size() * 2;
          for (; node < e; node += 2) {
            node->destroy();
            (node + 1)->destroy();
          }
          static_cast<MetaNode*>(children())->~MetaNode();
        }
        Allocator::Free(children());
        break;
      }
      case kArray: {
        for (auto it = this->Begin(), e = this->End(); it != e; ++it) {
          it->destroy();
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
