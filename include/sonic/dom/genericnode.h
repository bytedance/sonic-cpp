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

#include <cstddef>
#include <limits>
#include <memory>

#include "sonic/dom/handler.h"
#include "sonic/dom/json_pointer.h"
#include "sonic/dom/serialize.h"
#include "sonic/dom/type.h"
#include "sonic/error.h"
#include "sonic/string_view.h"
#include "sonic/writebuffer.h"

namespace sonic_json {

union ContainerNext {
  size_t ofs;
  void* children;
};  // 8 bytes

template <typename NodeType>
class MemberNodeT {
 public:
  const NodeType name;
  NodeType value;

  MemberNodeT* Next() const { return (MemberNodeT*)(value.Next()); }
};

// Forward Decalaration.
template <typename derived_t>
struct NodeTraits;

/**
 * @brief Basic class represent a json value.
 * @tparam tmpalte <typename> class Node  Derived class.
 * @tparam Allocator  Allocator type for derived class, which allocating memory
 * for container and string.
 */
template <template <typename> class Node, typename Allocator>
class GenericNode {
 public:
  using NodeType = Node<Allocator>;  ///< Derived class type.
  using alloc_type =
      typename NodeTraits<NodeType>::alloc_type;  ///< Dervied class allocator
                                                  ///< type.
  using MemberNode =
      typename NodeTraits<NodeType>::MemberNode;  ///< Derived class key-value
                                                  ///< pair struct.
  using MemberIterator =
      typename NodeTraits<NodeType>::MemberIterator;  ///< Derived class object
                                                      ///< iterator type.
  using ConstMemberIterator = typename NodeTraits<NodeType>::
      ConstMemberIterator;  ///< Derived class object const iterator type.
  using ValueIterator =
      typename NodeTraits<NodeType>::ValueIterator;  ///< Derived class array
                                                     ///< iterator type.
  using ConstValueIterator = typename NodeTraits<NodeType>::
      ConstValueIterator;  ///< Derived class array const iterator type.

  /**
   * @brief Default constructor, which creates null node.
   */
  GenericNode() noexcept {}

  /**
   * @brief Constructor for creating specific type.
   * @param type json value type.
   * @note Data of node is ZERO, except string. String type will keep as "".
   */
  explicit GenericNode(TypeFlag type) noexcept { setType(type); }

  /**
   * @brief Constructor for creating boolean json value.
   * @param b boolean, true/false
   * @note This function rejects converting from others type to boolean when
   *       ovarload resolution.
   */
  template <class T, typename std::enable_if<std::is_same<T, bool>::value,
                                             bool>::type = false>
  explicit GenericNode(T b) noexcept {
    b ? setType(kTrue) : setType(kFalse);
  }

  /**
   * @brief Constructor for creating int node.
   * @param i data of int node.
   */
  explicit GenericNode(int i) noexcept {
    (i >= 0) ? setType(kUint) : setType(kSint);
    n.i64 = i;
  }
  /**
   * @brief Constructor for creating uint node.
   * @param i data of uint node.
   */
  explicit GenericNode(unsigned int i) noexcept {
    setType(kUint);
    n.u64 = i;
  }
  /**
   * @brief Constructor for creating int64 node.
   * @param i data of int64 node.
   */
  explicit GenericNode(int64_t i) noexcept {
    if (i >= 0) {
      setType(kUint);
    } else {
      setType(kSint);
    }

    n.i64 = i;
  }
  /**
   * @brief Constructor for creating uint64 node.
   * @param i data of uint64 node.
   */
  explicit GenericNode(uint64_t i) noexcept {
    setType(kUint);
    n.u64 = i;
  }
  /**
   * @brief Constructor for creating double node.
   * @param d data of double node.
   */
  explicit GenericNode(double d) noexcept {
    setType(kReal);
    n.f64 = d;
  }
  /**
   * @brief Constructor for creating double node.
   * @param f data of double node.
   */
  explicit GenericNode(float f) noexcept {
    setType(kReal);
    n.f64 = f;
  }

  /**
   * @brief Constructor for creating string node. Doesn't COPY string.
   * @param s string pointer
   * @param len string length
   * @note GenericNode doesn't have the ability to manger memory alloced from
   *       heap. This constructor function only copy the pointer.
   */
  GenericNode(const char* s, size_t len) noexcept {
    setLength(len, kStringConst);
    sv.p = s;
  }

  /**
   * @brief Constructor for creating string node. Doesn't COPY string.
   * @param s string_view that contain string pointer and length.
   */
  explicit GenericNode(StringView s) noexcept {
    setLength(s.size(), kStringConst);
    sv.p = s.data();
  }

  /**
   * @brief Constructor for creating string node. Makes a string copy.
   * @param s     string pointer
   * @param len   string length
   * @param alloc Allocator
   */
  GenericNode(const char* s, size_t len, alloc_type& alloc) {
    StringCopy(s, len, alloc);
  }

  /**
   * @brief Constructor for creating string node. Makes a string copy.
   * @param s     string_view that contina string pointer and length.
   * @param alloc Allocator
   */
  GenericNode(StringView s, alloc_type& alloc) {
    StringCopy(s.data(), s.size(), alloc);
  }

  /**
   * @brief Copy string data to memory malloc from alloc, then store pointer and
   * size into node.
   * @param s     string pointer
   * @param len   string length
   * @param alloc Allocator
   * @note: If failed when allocating memory, node will be set as empty string.
   */
  void StringCopy(const char* s, size_t len, alloc_type& alloc) {
    sv.p = (char*)(alloc.Malloc(len + 1));
    if (sv.p) {
      std::memcpy(const_cast<char*>(sv.p), s, len);
      const_cast<char*>(sv.p)[len] = '\0';
      setLength(len, kStringFree);
    } else {
      setEmptyString();
    }
  }

  // Check APIs
  /**
   * @brief         Check this node is null.
   * @return bool   is null.
   */
  sonic_force_inline bool IsNull() const noexcept {
    return getBasicType() == kNull;
  }
  /**
   * @brief         Check this node is boolean.
   * @return bool   is boolean.
   */
  sonic_force_inline bool IsBool() const noexcept {
    return getBasicType() == kBool;
  }
  /**
   * @brief         Check this node is string.
   * @return bool   is string.
   */
  sonic_force_inline bool IsString() const noexcept {
    return getBasicType() == kString;
  }
  /**
   * @brief         Check this node is raw json.
   * @return bool   is raw json.
   */
  sonic_force_inline bool IsRaw() const noexcept {
    return getBasicType() == kRaw;
  }
  /**
   * @brief         Check this node is number.
   * @return bool   is number.
   */
  sonic_force_inline bool IsNumber() const noexcept {
    return getBasicType() == kNumber;
  }
  /**
   * @brief         Check this node is array.
   * @return bool   is array.
   */
  sonic_force_inline bool IsArray() const noexcept {
    return getBasicType() == kArray;
  }
  /**
   * @brief         Check this node is object.
   * @return bool   is object.
   */
  sonic_force_inline bool IsObject() const noexcept {
    return getBasicType() == kObject;
  }

  /**
   * @brief         Check this node is true.
   * @return bool   is true.
   */
  sonic_force_inline bool IsTrue() const noexcept { return GetType() == kTrue; }
  /**
   * @brief         Check this node is false.
   * @return bool   is false.
   */
  sonic_force_inline bool IsFalse() const noexcept {
    return GetType() == kFalse;
  }
  /**
   * @brief         Check this node is double number.
   * @return bool   is double.
   */
  sonic_force_inline bool IsDouble() const noexcept {
    return GetType() == kReal;
  }
  /**
   * @brief         Check this node is in range of int64.
   * @return bool   is int64.
   */
  sonic_force_inline bool IsInt64() const noexcept {
    return (GetType() == kSint) ||
           (GetType() == kUint &&
            n.u64 <= static_cast<uint64_t>(getInt64Max()));
  };
  /**
   * @brief         Check this node is in range of uint64.
   * @return bool   is uint64.
   */
  sonic_force_inline bool IsUint64() const noexcept {
    return GetType() == kUint;
  }
  /**
   * @brief         Check this node is only copy pointer, doesn't maintian
   *                string data in allocator.
   * @return bool   is not string const.
   */
  sonic_force_inline bool IsStringCopy() const noexcept {
    return GetType() != kStringConst;
  }
  /**
   * @brief         Check this node has a string copy in allocator.
   * @return bool   is string const.
   */
  sonic_force_inline bool IsStringConst() const noexcept {
    return GetType() == kStringConst;
  }
  /**
   * @brief         Check this node is object or array.
   * @return bool   is object or array.
   */
  sonic_force_inline bool IsContainer() const noexcept {
    return (t.t & kContainerMask) == static_cast<uint8_t>(kContainerMask);
  }

  /**
   * @brief         Get boolean value of this node.
   * @retval true   this node is true.
   * @retval false  this node if false.
   */
  sonic_force_inline bool GetBool() const noexcept {
    return GetType() == kTrue;
  }

  /**
   * @brief                 Get string of this node.
   * @return std::string    string
   */
  sonic_force_inline std::string GetString() const {
    return std::string(sv.p, Size());
  }

  /**
   * @brief                 Get string view of this node, won't copy string.
   * @return StringView     string view
   */
  sonic_force_inline StringView GetStringView() const noexcept {
    return StringView(sv.p, Size());
  }

  /**
   * @brief                 Get string view of this node, won't copy string.
   * @return StringView     string view
   */
  sonic_force_inline StringView GetRaw() const noexcept {
    return StringView(raw.p, Size());
  }

  /**
   * @brief         Get 5-bits type info
   * @return TypeFlag
   */
  sonic_force_inline TypeFlag GetType() const noexcept {
    return static_cast<TypeFlag>(t.t & kSubTypeMask);
  }

  /**
   * @brief         Get int64 data.
   * @return int64_t
   */
  sonic_force_inline int64_t GetInt64() const noexcept {
    sonic_assert(IsInt64());
    return n.i64;
  }
  /**
   * @brief         Get uint64 data.
   * @return uint64_t
   */
  sonic_force_inline uint64_t GetUint64() const noexcept {
    sonic_assert(IsUint64());
    return n.u64;
  }
  /**
   * @brief         Get double data.
   * @return double
   */
  sonic_force_inline double GetDouble() const noexcept {
    sonic_assert(IsNumber());
    if (IsDouble()) return n.f64;
    if (IsUint64())
      return static_cast<double>(
          n.u64);  // uint64_t -> double (may lose precision))
    return static_cast<double>(
        n.i64);  // int64_t -> double (may lose precision))
  }

  // Set APIs
  /**
   * @brief Set this node as null type.
   * @return NodeType& Reference to this.
   * @note this node will deconstruct firstly.
   */
  NodeType& SetNull() noexcept { return downCast()->setNullImpl(); }
  /**
   * @brief Set this node as object type. Data is ZERO!
   * @return NodeType& Reference to this.
   * @note this node will deconstruct firstly.
   */
  NodeType& SetObject() noexcept { return downCast()->setObjectImpl(); }
  /**
   * @brief Set this node as array type. Data is ZERO!
   * @return NodeType& Reference to this.
   * @note this node will deconstruct firstly.
   */
  NodeType& SetArray() noexcept { return downCast()->setArrayImpl(); }
  /**
   * @brief Set this node as bool type.
   * @param b true/fase
   * @return NodeType& Reference to this.
   * @note this node will deconstruct firstly.
   */
  NodeType& SetBool(bool b) noexcept { return downCast()->setBoolImpl(b); }
  /**
   * @brief Set this node as int64_t type.
   * @param i value of node
   * @return NodeType& Reference to this.
   * @note this node will deconstruct firstly.
   */
  NodeType& SetInt64(int64_t i) noexcept { return downCast()->setInt64Impl(i); }
  /**
   * @brief Set this node as uint64 type.
   * @param i value of node
   * @return NodeType& Reference to this.
   * @note this node will deconstruct firstly.
   */
  NodeType& SetUint64(uint64_t i) noexcept {
    return downCast()->setUint64Impl(i);
  }
  /**
   * @brief Set this node as double type.
   * @param d value of node
   * @return NodeType& Reference to this.
   * @note this node will deconstruct firstly.
   */
  NodeType& SetDouble(double d) noexcept {
    return downCast()->setDoubleImpl(d);
  }

  /**
   * @brief Set this node as string type. A string copy will be maintianed in
   * allocator.
   * @param s string_view that contain string pointer and size.
   * @param alloc Allocator which maintain nodes memory.
   * @return NodeType& Reference to this.
   * @note this node will deconstruct firstly.
   */
  NodeType& SetString(StringView s, alloc_type& alloc) {
    return SetString(s.data(), s.size(), alloc);
  }
  /**
   * @brief Set this node as string type. A string copy will be maintianed in
   * allocator.
   * @param s string pointer
   * @param len string length
   * @param alloc Allocator which maintain nodes memory.
   * @return NodeType& Reference to this.
   * @note this node will deconstruct firstly.
   */
  NodeType& SetString(const char* s, size_t len, alloc_type& alloc) {
    return downCast()->setStringImpl(s, len, alloc);
  }

  /**
   * @brief Set this node as string type. Only copy string pointer.
   * @param s string_view that contain string pointer and size.
   * @return NodeType& Reference to this.
   * @note this node will deconstruct firstly.
   */
  NodeType& SetString(StringView s) { return SetString(s.data(), s.size()); }
  /**
   * @brief Set this node as string type. Only copy string pointer.
   * @param s char array pointer
   * @return NodeType& Reference to this.
   * @note this node will deconstruct firstly.
   */
  NodeType& SetString(const char* s, size_t len) {
    return downCast()->setStringImpl(s, len);
  }

  /**
   * @brief Swap rhs and lhs.
   * @param rhs Another node.
   * @return NodeType& Reference to this.
   */
  NodeType& Swap(NodeType& rhs) noexcept {
    NodeType tmp;
    tmp.rawAssign(rhs);
    rhs.rawAssign(*(downCast()));
    downCast()->rawAssign(tmp);
    return *(downCast());
  }

  /**
   * @brief Compare with string_view
   * @param s string_view that contian string pointer and size
   * @retval true equals to
   * @retval false not equals to
   */
  sonic_force_inline bool operator==(StringView s) const noexcept {
    return GetStringView() == s;
  }

  /**
   * @brief Compare with boolean, int, uint32_t, int64_t, uint64_t, float and
   * double. Only above types is acceptable.
   * @tparam T data type.
   * @param data rhs value.
   * @retval true equals to
   * @retval false not equals to
   */
  template <
      class T,
      typename std::enable_if<
          std::is_same<T, bool>::value || std::is_same<T, int>::value ||
              std::is_same<T, int64_t>::value ||
              std::is_same<T, uint32_t>::value ||
              std::is_same<T, uint64_t>::value ||
              std::is_same<T, float>::value || std::is_same<T, double>::value,
          bool>::type = false>

  bool operator==(T data) const noexcept {
    return (*downCast()) == NodeType(data);
  }

  /**
   * @brief operator!=
   * @param s string_view that contian string pointer and size
   * @retval true not equals to
   * @retval false equals to
   */
  sonic_force_inline bool operator!=(StringView s) const noexcept {
    return !(*this == s);
  }
  /**
   * @brief operator!= with boolean, int, uint32_t, int64_t, uint64_t, float and
   * double. Only above types is acceptable.
   * @tparam T data type.
   * @param data rhs value.
   * @retval true not equals to
   * @retval false equals to
   */
  template <
      class T,
      typename std::enable_if<
          std::is_same<T, bool>::value || std::is_same<T, int>::value ||
              std::is_same<T, int64_t>::value ||
              std::is_same<T, uint32_t>::value ||
              std::is_same<T, uint64_t>::value ||
              std::is_same<T, float>::value || std::is_same<T, double>::value,
          bool>::type = false>
  bool operator!=(T data) const noexcept {
    return !(*this == data);
  }

  /**
   * @brief  Get size for string, object, array or raw json.
   * @return size_t
   */
  size_t Size() const noexcept {
    sonic_assert(this->IsContainer() || this->IsString() || this->IsRaw());
    return sv.len >> kInfoBits;
  }

  /**
   * @brief Check string, array or object is empty.
   * @retval true is empty
   * @retval false is not empty
   * @note The type of this node must be string, array or object.
   */
  bool Empty() const noexcept {
    sonic_assert(this->IsContainer() || this->IsString());
    return 0 == Size();
  }

  /**
   * @brief make this array or object empty
   * @return void
   */
  void Clear() noexcept {
    sonic_assert(this->IsContainer());
    downCast()->clearImpl();
  }

  /**
   * @brief Get object begin iterator.
   * @return MemberIterator
   */
  MemberIterator MemberBegin() noexcept {
    sonic_assert(this->IsObject());
    return downCast()->memberBeginImpl();
  }

  /**
   * @brief Get object const begin iterator.
   * @return MemberIterator
   */
  ConstMemberIterator MemberBegin() const noexcept {
    sonic_assert(this->IsObject());
    return downCast()->cmemberBeginImpl();
  }

  /**
   * @brief Get object const beign iterator.
   * @return MemberIterator
   */
  ConstMemberIterator CMemberBegin() const noexcept {
    sonic_assert(this->IsObject());
    return downCast()->cmemberBeginImpl();
  }

  /**
   * @brief Get object end iterator.
   * @return MemberIterator
   */
  MemberIterator MemberEnd() noexcept {
    sonic_assert(this->IsObject());
    return downCast()->memberEndImpl();
  }

  /**
   * @brief Get object const end iterator.
   * @return MemberIterator
   */
  ConstMemberIterator MemberEnd() const noexcept {
    sonic_assert(this->IsObject());
    return downCast()->cmemberEndImpl();
  }

  /**
   * @brief Get object const end iterator.
   * @return MemberIterator
   */
  ConstMemberIterator CMemberEnd() const noexcept {
    sonic_assert(this->IsObject());
    return downCast()->cmemberEndImpl();
  }

  /**
   * @brief Get specific node in object by key.
   * @param key string view that contain key's pointer and size.
   * @retval null-node Expected node doesn't exist.
   * @retval others Reference to the expected node.
   */
  sonic_force_inline NodeType& operator[](StringView key) noexcept {
    sonic_assert(this->IsObject());
    return downCast()->findValueImpl(key);
  }

  /**
   * @brief Get specific node in object by key.
   * @param key string view that contain key's pointer and size.
   * @retval null-node Expected node doesn't exist.
   * @retval others Reference to the expected node.
   */
  sonic_force_inline const NodeType& operator[](StringView key) const noexcept {
    sonic_assert(this->IsObject());
    return downCast()->findValueImpl(key);
  }

  /**
   * @brief Check object has specific key.
   * @param key string view that contain key's pointer and size.
   * @return bool
   */
  sonic_force_inline bool HasMember(StringView key) const noexcept {
    return FindMember(key) != MemberEnd();
  }

  /**
   * @brief Find specific member in object
   * @param key string view that contain key's pointer and size
   * @retval MemberEnd() not found
   * @retval others iterator for found member
   */
  sonic_force_inline MemberIterator FindMember(StringView key) noexcept {
    return downCast()->findMemberImpl(key);
  }

  /**
   * @brief Find specific member in object
   * @param key string view that contain key's pointer and size
   * @retval MemberEnd() not found
   * @retval others iterator for found member
   */
  sonic_force_inline ConstMemberIterator
  FindMember(StringView key) const noexcept {
    return downCast()->findMemberImpl(key);
  }

  /**
   * @brief get specific node by json pointer(RFC 6901)
   * @tparam StringType json pointer string type, can use StringView to aovid
   *                    copying string.
   * @param pointer json pointer
   * @retval nullptr get node failed
   * @retval others success
   */
  template <typename StringType>
  NodeType* AtPointer(const GenericJsonPointer<StringType>& pointer) {
    return atPointerImpl(pointer);
  }

  /**
   * @brief get specific node by json pointer. This is implemented by variable
   *        argument.
   * @retval nullptr get node failed
   * @retval others success
   * @note This method has better performance than JsonPointer when arguments
   *       are string literal, such as obj.AtPointer("a", "b", "c"). However,
   *       if arguments are string, please use JsonPointer.
   */
  sonic_force_inline NodeType* AtPointer() { return downCast(); }

  sonic_force_inline const NodeType* AtPointer() const { return downCast(); }

  template <typename... Args>
  sonic_force_inline NodeType* AtPointer(size_t idx, Args... args) {
    if (!IsArray()) {
      return nullptr;
    }
    if (idx >= Size()) {
      return nullptr;
    }
    return (*this)[idx].AtPointer(args...);
  }

  template <typename... Args>
  sonic_force_inline const NodeType* AtPointer(size_t idx, Args... args) const {
    if (!IsArray()) {
      return nullptr;
    }
    if (idx >= Size()) {
      return nullptr;
    }
    return (*this)[idx].AtPointer(args...);
  }

  template <typename... Args>
  sonic_force_inline NodeType* AtPointer(StringView key, Args... args) {
    if (!IsObject()) {
      return nullptr;
    }
    auto m = FindMember(key);
    if (m == MemberEnd()) {
      return nullptr;
    }
    return m->value.AtPointer(args...);
  }

  template <typename... Args>
  sonic_force_inline const NodeType* AtPointer(StringView key,
                                               Args... args) const {
    if (!IsObject()) {
      return nullptr;
    }
    auto m = FindMember(key);
    if (m == MemberEnd()) {
      return nullptr;
    }
    return m->value.AtPointer(args...);
  }

  /**
   * @brief get specific node by json pointer(RFC 6901)
   * @tparam StringType json pointer string type, can use StringView to aovid
   *                    copying string.
   * @param pointer json pointer
   * @retval nullptr get node failed
   * @retval others success
   */
  template <typename StringType>
  sonic_force_inline const NodeType* AtPointer(
      const GenericJsonPointer<StringType>& pointer) const {
    return atPointerImpl(pointer);
  }

  /**
   * @brief Add a new member for this object.
   * @tparam ValueType the type of member's value
   * @param key new member's name, must be string
   * @param value rvalue or lvalue reference for value, must be NodeType
   * @param copyKey copy the key's string when creating key node if copyKey is
   * true.
   * @return NodeType& Reference of the added member.
   * @note value will be moved.
   */
  template <typename ValueType>
  MemberIterator AddMember(StringView key, ValueType&& value, alloc_type& alloc,
                           bool copyKey = true) {
    sonic_assert(this->IsObject());
    return downCast()->addMemberImpl(key, value, alloc, copyKey);
  }

  /**
   * @brief Reserve object capacity if NodeType support. Otherwise do nothing.
   * @param new_cap Expected object capacity, unit is member(key-value pair)
   * @param alloc Allocator reference that maintain object memory.
   * @return NodeType& Reference to this object to support streaming APIs
   */
  NodeType& MemberReserve(size_t new_cap, alloc_type& alloc) {
    sonic_assert(this->IsObject());
    return downCast()->memberReserveImpl(new_cap, alloc);
  }

  /**
   * @brief Remove specific member in object by key.
   * @param key string view that contian key's pointer and size
   * @retval true success
   * @retval false failed
   */
  sonic_force_inline bool RemoveMember(StringView key) noexcept {
    return downCast()->removeMemberImpl(key);
  }

  /**
   * @brief Remove specific range [first, last) in object
   * @param first range start
   * @param last range end
   * @return iterator following the last removed member
   */
  MemberIterator EraseMember(MemberIterator first, MemberIterator last) {
    return downCast()->eraseMemberImpl(first, last);
  }

  // Array APIs
  /**
   * @brief Get array capacity.
   * @return size_t
   * @note If NodeType doesn't support, this function will return Size()
   */
  size_t Capacity() const noexcept {
    sonic_assert(this->IsContainer());
    return downCast()->capacityImpl();
  }

  /**
   * @brief Get array beign iterator
   * @return ValueIterator
   */
  ValueIterator Begin() noexcept {
    sonic_assert(this->IsArray());
    return downCast()->beginImpl();
  }

  /**
   * @brief Get array const beign iterator
   * @return ValueIterator
   */
  ConstValueIterator Begin() const noexcept {
    sonic_assert(this->IsArray());
    return downCast()->cbeginImpl();
  }

  /**
   * @brief Get array const beign iterator
   * @return ValueIterator
   */
  ConstValueIterator CBegin() const noexcept {
    sonic_assert(this->IsArray());
    return downCast()->cbeginImpl();
  }

  /**
   * @brief Get array end iterator
   * @return ValueIterator
   */
  ValueIterator End() noexcept {
    sonic_assert(this->IsArray());
    return downCast()->endImpl();
  }

  /**
   * @brief Get array const end iterator
   * @return ValueIterator
   */
  ConstValueIterator End() const noexcept {
    sonic_assert(this->IsArray());
    return downCast()->cendImpl();
  }

  /**
   * @brief Get array const end iterator
   * @return ValueIterator
   */
  ConstValueIterator CEnd() const noexcept {
    sonic_assert(this->IsArray());
    return downCast()->cendImpl();
  }

  /**
   * @brief Return array last element.
   * @return NodeType&
   */
  NodeType& Back() noexcept {
    sonic_assert(this->IsArray());
    sonic_assert(!this->Empty());
    return downCast()->backImpl();
  }

  /**
   * @brief Return array last element.
   * @return NodeType&
   */
  const NodeType& Back() const noexcept {
    sonic_assert(this->IsArray());
    sonic_assert(!this->Empty());
    return downCast()->backImpl();
  }

  /**
   * @brief Get specific element in array by index
   * @param idx index
   * @return NodeType&
   */
  NodeType& operator[](size_t idx) noexcept {
    sonic_assert(this->IsArray());
    sonic_assert(!this->Empty());
    return downCast()->findValueImpl(idx);
  }

  /**
   * @brief Get specific element in array by index
   * @param idx index
   * @return NodeType&
   */
  const NodeType& operator[](size_t idx) const noexcept {
    sonic_assert(this->IsArray());
    sonic_assert(!this->Empty());
    return downCast()->findValueImpl(idx);
  }

  /**
   * @brief Reserve array capacity if NodeType support. Otherwise do nothing.
   * @param new_cap Expected array capacity, unit is node
   * @param alloc Allocator reference that maintain array memory.
   * @return NodeType& Reference to this array to support streaming APIs
   */
  // TODO: Check when new_cap less than size() or capacity @Xie Gengxin
  NodeType& Reserve(size_t new_cap, alloc_type& alloc) {
    sonic_assert(this->IsArray());
    return downCast()->reserveImpl(new_cap, alloc);
  }

  /**
   * @brief Push an element into array.
   * @tparam ValueType push node type
   * @param value rvalue or lvalue reference of push node.
   * @param alloc allocator reference that maintain array memory
   * @return NodeType reference for this node to support streaming APIs
   * @note value will be moved.
   */
  template <typename ValueType>
  NodeType& PushBack(ValueType&& value, alloc_type& alloc) {
    sonic_assert(this->IsArray());
    return downCast()->pushBackImpl(value, alloc);
  }

  /**
   * @brief pop out the last element in arra
   * @return NodeType& reference for this node to support streaming APIs
   */
  NodeType& PopBack() noexcept {
    sonic_assert(this->IsArray());
    sonic_assert(!this->Empty());
    return downCast()->popBackImpl();
  }

  /**
   * @brief erase specific element in array by iterator
   * @param pos ValueIterator for expected erasing element.
   * @return ValueIterator the next iterator of erased element.
   */
  ValueIterator Erase(const ValueIterator pos) noexcept {
    return Erase(pos, pos + 1);
  }
  /**
   * @brief erase specific elements in array by iterator range
   * @param first ValueIterator for expected erasing element range start.
   * @param last ValueIterator for expected erasing element range end.
   * @return ValueType the ValueIterator point to last.
   * @note erase in range [first, last)
   */
  ValueIterator Erase(const ValueIterator first,
                      const ValueIterator last) noexcept {
    return downCast()->eraseImpl(first, last);
  }
  /**
   * @brief erase specific elements in array by index range
   * @param first expected erasing element range start index.
   * @param last expected erasing element range end.
   * @return ValueType the ValueIterator point to last.
   * @note erase in range [first, last)
   */
  ValueIterator Erase(size_t first, size_t last) noexcept {
    return Erase(Begin() + first, Begin() + last);
  }

  // Serialize API
  /**
   * @brief serialize this node as json string.
   * @param serializeFlags combination of different SerializeFlag.
   * @param wb write buffer where you want store json string.
   * @return EndcodeError
   */
  template <unsigned serializeFlags = kSerializeDefault>
  SonicError Serialize(WriteBuffer& wb) const {
    return downCast()->template serializeImpl<serializeFlags>(wb);
  }

  /**
   * @brief dump this node as json string.
   * @param serializeFlags combination of different SerializeFlag.
   * @return empty string if there are errors when serializing.
   */
  template <unsigned serializeFlags = kSerializeDefault>
  std::string Dump() const {
    WriteBuffer wb;
    SonicError err = Serialize<serializeFlags>(wb);
    return err == kErrorNone ? wb.ToString() : "";
  }

 protected:
  sonic_force_inline NodeType* next() noexcept {
    return downCast()->nextImpl();
  }
  sonic_force_inline const NodeType* next() const noexcept {
    return downCast()->cnextImpl();
  }

  sonic_force_inline void addLength(size_t c) noexcept {
    sonic_assert(IsContainer() || IsString());
    this->sv.len += (c << kTotalTypeBits);
  }
  sonic_force_inline void subLength(size_t c) noexcept {
    sonic_assert(IsContainer() || IsString());
    this->sv.len -= (c << kTotalTypeBits);
  }

  sonic_force_inline TypeFlag getBasicType() const noexcept {
    return static_cast<TypeFlag>(t.t & kBasicTypeMask);
  }
  sonic_force_inline void setLength(size_t len) noexcept {
    sv.len = (len << kInfoBits) | static_cast<uint64_t>(t.t);
  }
  sonic_force_inline void setLength(size_t len, TypeFlag flag) noexcept {
    sv.len = (len << kInfoBits) | static_cast<uint64_t>(flag);
  }
  sonic_force_inline void setType(TypeFlag flag) noexcept {
    sv.len = static_cast<uint64_t>(flag);
    if (IsString()) {
      setEmptyString();
    }
  }
  NodeType& setRaw(StringView s) {
    return downCast()->setRawImpl(s.data(), s.size());
  }
  void setEmptyString() noexcept {
    sv.p = "";
    setLength(0, kStringConst);
  }

  sonic_force_inline int64_t getIntMax() const {
    return std::numeric_limits<int>::max();
  }
  sonic_force_inline int64_t getIntMin() const {
    return std::numeric_limits<int>::min();
  }
  sonic_force_inline int64_t getInt64Max() const {
    return std::numeric_limits<int64_t>::max();
  }
  sonic_force_inline uint64_t getUintMax() const {
    return std::numeric_limits<uint>::max();
  }

 private:
  friend NodeType;
  friend class SAXHandler<NodeType>;
  friend class LazySAXHandler<NodeType>;

  struct Object {
    uint64_t len;
    union ContainerNext next;
  };  // 16 bytes

  struct Array {
    uint64_t len;
    union ContainerNext next;
  };  // 16 bytes

  struct String {
    uint64_t len;
    const char* p;
  };  // 16 bytes

  struct Type {
    uint8_t t;
    uint8_t _len[7];
    char _ptr[8];
  };  // 16 bytes

  struct Raw {
    uint64_t len;
    const char* p;
  };  // 16 bytes
  struct Number {
    uint64_t t;
    union {
      int64_t i64;
      uint64_t u64;
      double f64;
    };  // lower 8 bytes
  };

  struct Data {
    uint64_t _[2];
  };

  union {
    Type t;
    Number n;
    Object o;
    Array a;
    String sv;
    Raw raw;
    Data data = {};
  };  // anonymous member

  const NodeType* downCast() const noexcept {
    return static_cast<const NodeType*>(this);
  }
  NodeType* downCast() noexcept { return static_cast<NodeType*>(this); }

  template <typename StringType>
  NodeType* atPointerImpl(const GenericJsonPointer<StringType>& pointer) const {
    const NodeType* re = downCast();
    for (auto& node : pointer) {
      if (node.IsStr()) {
        if (re->IsObject()) {
          auto m = re->FindMember(node.GetStr());
          if (m != re->MemberEnd()) {
            re = &(m->value);
            continue;
          }
        }
        return nullptr;
      } else {  // Json Pointer node is number
        if (re->IsArray()) {
          int idx = node.GetNum();
          if (idx >= 0 && idx < static_cast<int>(re->Size())) {
            re = &(re->operator[]((size_t)idx));
            continue;
          }
        }
        return nullptr;
      }
    }
    return const_cast<NodeType*>(re);
  }
};

}  // namespace sonic_json
