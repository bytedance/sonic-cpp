#pragma once

#include <map>
#include <type_traits>
#include <utility>

#include "sonic/allocator.h"
#include "sonic/dom/genericnode.h"
#include "sonic/dom/handler.h"
#include "sonic/dom/parser.h"
#include "sonic/dom/serialize.h"
#include "sonic/dom/type.h"
#include "sonic/error.h"
#include "sonic/internal/ftoa.h"
#include "sonic/internal/itoa.h"
#include "sonic/internal/quote.h"
#include "sonic/writebuffer.h"

namespace sonic_json {

// LazyNode is a lazy parse node, it will not parse the json string until you
// access the node.
// Note: Lazy is not thread safe and not reentrant, even if you use Get APIs.
template <typename Allocator = SONIC_DEFAULT_ALLOCATOR>
class LazyNode : public GenericNode<LazyNode<Allocator>> {
 private:
  Allocator* alloc_ = nullptr;

 public:
  using NodeType = LazyNode;
  using BaseNode = GenericNode<LazyNode<Allocator>>;
  using AllocatorType = Allocator;
  using MemberNode = typename NodeTraits<LazyNode>::MemberNode;
  using MemberIterator = typename NodeTraits<LazyNode>::MemberIterator;
  using ConstMemberIterator =
      typename NodeTraits<LazyNode>::ConstMemberIterator;
  using ValueIterator = typename NodeTraits<LazyNode>::ValueIterator;
  using ConstValueIterator = typename NodeTraits<LazyNode>::ConstValueIterator;

  friend class SAXHandler<LazyNode>;
  friend class LazySAXHandler<LazyNode>;

  friend BaseNode;
  template <typename>
  friend class LazyNode;
  template <unsigned serializeFlags, typename NodeType>
  friend SonicError internal::SerializeImpl(const NodeType*, WriteBuffer&);

  // constructor
  using BaseNode::BaseNode;
  /**
   * @brief move constructor
   * @param rhs moved value, must be a rvalue reference
   */
  LazyNode() noexcept : BaseNode() {}
  LazyNode(LazyNode&& rhs) noexcept : BaseNode() { rawAssign(rhs); }
  LazyNode(const LazyNode& rhs) = delete;

  /**
   * @brief copy constructor
   * @tparam rhs class Allocator type
   * @param rhs copied value reference
   * @param alloc allocator reference that maintain this node memory
   * @param copyString false defautlly, copy const string or not.
   */
  template <typename SourceAllocator>
  LazyNode(const LazyNode<SourceAllocator>& rhs, Allocator& alloc,
           bool copyString = false)
      : BaseNode() {
    using rhsNodeType = LazyNode<SourceAllocator>;
    alloc_ = &alloc;
    switch (rhs.getBasicType()) {
      case kObject: {
        size_t count = rhs.Size();
        this->o.len = rhs.getTypeAndLen();  // Copy size and type.
        if (count > 0) {
          void* mem = containerMalloc<MemberNode>(count, alloc);
          rhsNodeType* rn = rhs.getObjChildrenFirst();
          LazyNode* ln = (LazyNode*)((char*)mem + sizeof(MetaNode));
          for (size_t i = 0; i < count * 2; i += 2) {
            new (ln + i) LazyNode(*(rn + i), alloc, copyString);
            new (ln + i + 1) LazyNode(*(rn + i + 1), alloc, copyString);
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
          void* mem = containerMalloc<LazyNode>(a_size, alloc);
          LazyNode* ln = (LazyNode*)((char*)mem + sizeof(MetaNode));
          for (size_t i = 0; i < a_size; ++i) {
            new (ln + i) LazyNode(*(rn + i), alloc, copyString);
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
      case kRaw: {
        this->raw.len = rhs.getTypeAndLen();  // Copy size and type.
        if (rhs.GetType() != kRawConst || copyString) {
          this->RawJsonCopy(rhs.GetRaw().data(), rhs.RawJsonLength(), alloc);
        } else {
          this->raw.p = rhs.GetRaw().data();
        }
        break;
      }
      default:
        // NOTE: not copy the alloc_ pointer here.
        std::memcpy(&(this->data), &rhs, sizeof(BaseNode));
        break;
    }
  }

  /**
   * @brief destructor
   */
  ~LazyNode() {
    if (!Allocator::kNeedFree) {
      return;
    }
    destroy();
  }

  // Check APIs
#define LAZY_CONST_API(name, bool)       \
  sonic_force_inline bool name() const { \
    checkOrLoad();                       \
    return BaseNode::name();             \
  }

#define LAZY_API(name, bool)       \
  sonic_force_inline bool name() { \
    checkOrLoad();                 \
    return BaseNode::name();       \
  }

  LAZY_CONST_API(IsNull, bool);
  LAZY_CONST_API(IsBool, bool);
  LAZY_CONST_API(IsString, bool);
  LAZY_CONST_API(IsNumber, bool);
  LAZY_CONST_API(IsTrue, bool);
  LAZY_CONST_API(IsFalse, bool);
  LAZY_CONST_API(IsDouble, bool);
  LAZY_CONST_API(IsInt64, bool);
  LAZY_CONST_API(IsUint64, bool);
  LAZY_CONST_API(Empty, bool);
  LAZY_CONST_API(IsObject, bool);
  LAZY_CONST_API(IsArray, bool);

  // sonic_force_inline bool IsObject() const noexcept {
  //   if (this->IsRaw() && first() == '{') {
  //     return true;
  //   }
  //   return BaseNode::IsObject();
  // }

  // sonic_force_inline bool IsArray() const noexcept {
  //   if (this->IsRaw() && first() == '[') {
  //     return true;
  //   }
  //   return BaseNode::IsArray();
  // }

  // Get APIs
  LAZY_CONST_API(GetString, std::string);
  LAZY_CONST_API(GetStringView, StringView);
  LAZY_CONST_API(GetInt64, int64_t);
  LAZY_CONST_API(GetUint64, uint64_t);
  LAZY_CONST_API(GetDouble, double);

  size_t Size() const {
    checkOrLoad();
    return BaseNode::Size();
  }

  LazyNode& operator=(const LazyNode& rhs) = delete;
  LazyNode& operator=(LazyNode& rhs) = delete;
  /**
   * @brief move assignment
   * @param rhs rvalue reference to right hand side
   * @return LazyNode& this reference
   */
  LazyNode& operator=(LazyNode&& rhs) {
    if (sonic_likely(this != &rhs)) {
      // Can't destroy "this" before assigning "rhs", otherwise "rhs"
      // could be used after free if it's an sub-node of "this",
      // hence the temporary danse.
      // Copied from RapidJSON.
      LazyNode temp;
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
  bool operator==(const LazyNode<SourceAllocator>& rhs) const noexcept {
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

      case kRawCopy:
      case kRawFree:
      case kRawConst:
        return this->GetRaw() == rhs.GetRaw();

      case kReal:
      case kSint:
      case kUint:
        if (this->GetType() != rhs.GetType()) {
          return false;
        }
        // NOTE: memcmp the basenode.
        return !std::memcmp(this, &rhs, sizeof(BaseNode));

      default:
        return this->GetType() == rhs.GetType();
    }
  }

  using BaseNode::operator!=;
  /**
   * @brief operator!=
   */
  template <typename SourceAllocator>
  bool operator!=(const LazyNode<SourceAllocator>& rhs) const noexcept {
    return !(*this == rhs);
  }

  // Object APIs

  using BaseNode::operator[];
  using BaseNode::FindMember;
  using BaseNode::HasMember;

  LAZY_CONST_API(MemberBegin, ConstMemberIterator);
  LAZY_CONST_API(MemberEnd, ConstMemberIterator);
  LAZY_CONST_API(CMemberBegin, ConstMemberIterator);
  LAZY_CONST_API(CMemberEnd, ConstMemberIterator);
  LAZY_API(MemberBegin, MemberIterator);
  LAZY_API(MemberEnd, MemberIterator);

  LAZY_CONST_API(Begin, ConstValueIterator);
  LAZY_CONST_API(End, ConstValueIterator);
  LAZY_CONST_API(CBegin, ConstValueIterator);
  LAZY_CONST_API(CEnd, ConstValueIterator);
  LAZY_API(Begin, ValueIterator);
  LAZY_API(End, ValueIterator);

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
   * @return LazyNode& reference to this node to support streaming APIs
   * @node This function will recursively. If json-tree is too depth, this
   * function maybe cause stackoveflow.
   */
  template <typename SourceAllocator>
  LazyNode& CopyFrom(const LazyNode<SourceAllocator>& rhs, Allocator& alloc,
                     bool copyString = false) {
    this->destroy();
    new (this) LazyNode(rhs, alloc, copyString);
    return *this;
  }

  void LoadLazy() const {
    LazyNode* self = const_cast<LazyNode*>(this);
    self->LoadLazy();
  }

  void LoadLazy() {
    if (!this->IsRaw()) {
      return;
    }

    // use temp node to make it free after load, if type is kRawFree.
    NodeType temp(std::move(*this));
    StringView raw = temp.GetRaw();
    char c = *raw.data();

    // lazily load the JSON object or array.
    if (c == '[' || c == '{') {
      Parser p;
      LazySAXHandler<NodeType> sax(*alloc_);
      // TODO: maybe add ParseLazy(sax) API.
      ParseResult ret = p.ParseLazy(raw.data(), raw.size(), sax);
      if (ret.Error()) {
        return;
      }
      *this = std::move(*sax.stack_.template Begin<NodeType>());
      return;
    }

    // parse other primitive JSON types.
    // TODO: elimiate the allocation for numbers or literals.
    switch (c) {
      case 't':
        ParseTrue(raw, *this);
        break;
      case 'f':
        ParseFalse(raw, *this);
        break;
      case 'n':
        ParseNull(raw, *this);
        break;
      case '"':
        ParseString(raw, *this, *alloc_);
        break;
      default:
        ParseNumber(raw, *this);
        break;
    }
    return;
  }

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
  LazyNode& setNullImpl() {
    this->destroy();
    new (this) BaseNode(kNull);
    return *this;
  }

  LazyNode& setBoolImpl(bool b) {
    this->destroy();
    new (this) BaseNode(b);
    return *this;
  }

  LazyNode& setObjectImpl() {
    this->destroy();
    new (this) BaseNode(kObject);
    setChildren(nullptr);
    return *this;
  }

  LazyNode& setArrayImpl() {
    this->destroy();
    new (this) BaseNode(kArray);
    setChildren(nullptr);
    return *this;
  }

  LazyNode& setIntImpl(int i) {
    this->destroy();
    new (this) BaseNode(i);
    return *this;
  }

  LazyNode& setUintImpl(unsigned int i) {
    this->destroy();
    new (this) BaseNode(i);
    return *this;
  }

  LazyNode& setInt64Impl(int64_t i) {
    this->destroy();
    new (this) BaseNode(i);
    return *this;
  }

  LazyNode& setUint64Impl(uint64_t i) {
    this->destroy();
    new (this) BaseNode(i);
    return *this;
  }

  LazyNode& setDoubleImpl(double d) {
    this->destroy();
    new (this) BaseNode(d);
    return *this;
  }

  LazyNode& setStringImpl(const char* s, size_t len) {
    this->destroy();
    new (this) BaseNode(s, len);
    return *this;
  }

  LazyNode& setStringImpl(const char* s, size_t len, Allocator& alloc) {
    this->destroy();
    new (this) BaseNode(s, len, alloc);
    return *this;
  }

  LazyNode& setRawImpl(const char* s, size_t len, Allocator* alloc) {
    this->destroy();
    this->raw.p = s;
    this->setLength(len, kRawCopy);
    alloc_ = alloc;
    return *this;
  }

  LazyNode& popBackImpl() {
    getArrChildrenFirstUnsafe()[this->Size() - 1].~LazyNode();
    this->subLength(1);
    return *this;
  }

  LazyNode& reserveImpl(size_t new_cap, Allocator& alloc) {
    checkOrLoad();
    if (new_cap > this->Capacity()) {
      setChildren(containerRealloc<LazyNode>(children(), this->Capacity(),
                                             new_cap, alloc));
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

  LazyNode& backImpl() const noexcept {
    return *(getArrChildrenFirst() + this->Size() - 1);
  }

  size_t capacityImpl() const noexcept {
    checkOrLoad();
    return children() != nullptr ? meta()->cap : 0;
  }

  LazyNode& memberReserveImpl(size_t new_cap, Allocator& alloc) {
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

  sonic_force_inline LazyNode* getArrChildrenFirst() const {
    sonic_assert(this->IsArray());
    if (nullptr == children()) {
      return nullptr;
    }
    return (LazyNode*)((char*)this->a.next.children +
                       sizeof(MetaNode) / sizeof(char));
  }

  sonic_force_inline LazyNode* getArrChildrenFirstUnsafe() const {
    sonic_assert(this->IsArray());
    return (LazyNode*)((char*)this->a.next.children +
                       sizeof(MetaNode) / sizeof(char));
  }

  sonic_force_inline LazyNode* getObjChildrenFirst() const {
    sonic_assert(this->IsObject());
    if (nullptr == children()) {
      return nullptr;
    }
    return (LazyNode*)((char*)this->a.next.children +
                       sizeof(MetaNode) / sizeof(char));
  }

  sonic_force_inline LazyNode* getObjChildrenFirstUnsafe() const {
    sonic_assert(this->IsObject());
    return (LazyNode*)((char*)this->a.next.children +
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

  sonic_force_inline LazyNode& findValueImpl(StringView key) const noexcept {
    auto m = findMemberImpl(key);
    if (m != this->MemberEnd()) {
      return m->value;
    }
    static LazyNode tmp{};
    tmp.SetNull();
    return tmp;
  }

  LazyNode& findValueImpl(size_t idx) const noexcept {
    checkOrLoad();
    return *(getArrChildrenFirst() + idx);
  }

  MemberIterator addMemberImpl(StringView key, LazyNode& value,
                               Allocator& alloc, bool copyKey) {
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
    LazyNode name;
    if (copyKey) {
      name.SetString(key, alloc);
    } else {
      name.SetString(key);
    }
    LazyNode* last = this->getObjChildrenFirst() + count * 2;
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
      LazyNode* m_name = (LazyNode*)(&(m->name));
      LazyNode* tail_name = (LazyNode*)(&(m_tail->name));
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
      m->name.~LazyNode();
      m->value.~LazyNode();
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
      it->name.~LazyNode();
      it->value.~LazyNode();
    }
    if (first != last || last != end) {
      std::memmove(static_cast<void*>(&(*first)), static_cast<void*>(&(*last)),
                   sizeof(MemberNode) * (end - last));
    }
    this->subLength(last - first);
    return first;
  }

  LazyNode& pushBackImpl(LazyNode& value, Allocator& alloc) {
    constexpr size_t k_default_array_cap = 16;
    sonic_assert(this->IsArray());
    // reseve capacity
    size_t cap = this->Capacity();
    if (this->Size() >= cap) {
      size_t new_cap = cap ? cap + (cap + 1) / 2 : k_default_array_cap;
      void* old_ptr = this->a.next.children;
      LazyNode* new_child =
          (LazyNode*)containerRealloc<LazyNode>(old_ptr, cap, new_cap, alloc);
      this->a.next.children = new_child;
    }
    // add value to the last pos
    LazyNode& last = *(this->End());
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
    for (ValueIterator it = pos; it != end; ++it) it->~LazyNode();
    std::memmove(static_cast<void*>(pos), end,
                 (this->End() - end) * sizeof(LazyNode));
    this->subLength(end - start);
    return start;
  }

  template <unsigned serializeFlags = kSerializeDefault>
  SonicError serializeImpl(WriteBuffer& wb) const {
    return internal::SerializeImpl<serializeFlags>(this, wb);
  }

  sonic_force_inline LazyNode* nextImpl() { return this + 1; }

  sonic_force_inline const LazyNode* cnextImpl() const { return this + 1; }

  // delete LazyNode's children or copied string
  void destroy() {
    if (!Allocator::kNeedFree) {
      return;
    }
    switch (this->GetType()) {
      case kObject: {
        if (children()) {
          LazyNode* node = getObjChildrenFirstUnsafe();
          LazyNode* e = node + this->Size() * 2;
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
      case kRawFree:
        Allocator::Free((void*)(this->raw.p));
        break;
      default:
        break;
    }
  }

  sonic_force_inline void rawAssign(LazyNode& rhs) {
    this->data = rhs.data;
    this->alloc_ = rhs.alloc_;
    rhs.setType(kNull);
  }

  LazyNode& clearImpl() {
    this->destroy();
    this->setLength(0);
    setChildren(nullptr);
    return *this;
  }
  sonic_force_inline uint64_t getTypeAndLen() const { return this->sv.len; }

  sonic_force_inline void checkOrLoad() const {
    if (this->IsRaw()) {
      this->LoadLazy();
    }
  }

  sonic_force_inline char first() const {
    sonic_assert(this->IsRaw());
    return this->GetRaw().data()[0];
  }
};

template <typename Allocator>
struct NodeTraits<LazyNode<Allocator>> {
  using alloc_type = Allocator;
  using NodeType = LazyNode<Allocator>;
  using MemberNode = MemberNodeT<NodeType>;
  using MemberIterator = MemberNode*;
  using ConstMemberIterator = const MemberNode*;
  using ValueIterator = NodeType*;
  using ConstValueIterator = const NodeType*;
};

}  // namespace sonic_json