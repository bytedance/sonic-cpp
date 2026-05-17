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

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <type_traits>

#include "sonic/allocator.h"
#include "sonic/error.h"
#include "sonic/macro.h"

namespace sonic_json {
namespace internal {

class Stack {
 public:
  Stack(size_t cap = defaultCapcity()) noexcept : cap_(0) {
    buf_ = nullptr;
    top_ = nullptr;
    Reserve(cap);
  }
  Stack(const Stack&) = delete;
  Stack(Stack&& rhs) noexcept
      : buf_(rhs.buf_), top_(rhs.top_), cap_(rhs.cap_), error_(rhs.error_) {
    rhs.setZero();
  }
  ~Stack() noexcept { std::free(buf_); }
  Stack& operator=(const Stack&) = delete;
  Stack& operator=(Stack&& rhs) noexcept {
    std::free(buf_);
    buf_ = rhs.buf_;
    top_ = rhs.top_;
    cap_ = rhs.cap_;
    error_ = rhs.error_;
    rhs.setZero();
    return *this;
  }

  sonic_force_inline size_t Size() const {
    return buf_ == nullptr ? 0 : static_cast<size_t>(top_ - buf_);
  }
  sonic_force_inline size_t Capacity() const { return cap_; }
  sonic_force_inline bool Empty() const { return Size() == 0; }
  sonic_force_inline bool HadOom() const { return error_ == kErrorNoMem; }
  sonic_force_inline SonicError GetError() const { return error_; }
  sonic_force_inline void ClearOom() {
    if (error_ == kErrorNoMem) error_ = kErrorNone;
  }

  /**
   * @brief Increase the capacity of buffer if new_cap is greater than the
   * current capacity(). Otherwise, do nothing.
   */
  sonic_force_inline bool Reserve(size_t new_cap) {
    if (new_cap <= Capacity()) {
      return true;
    }
    if (new_cap > std::numeric_limits<size_t>::max() - 7) {
      setOom();
      return false;
    }
    size_t align_cap = SONIC_ALIGN(new_cap);
    size_t old_size = Size();
    char* tmp = static_cast<char*>(std::realloc(buf_, align_cap));
    if (sonic_unlikely(tmp == nullptr)) {
      setOom();
      return false;
    }
    top_ = tmp + old_size;
    buf_ = tmp;
    cap_ = new_cap;
    return true;
  }

  /**
   * @brief Erases all contexts in the buffer.
   */
  sonic_force_inline void Clear() { top_ = buf_; }

  /**
   * @brief Push a value into buffer
   * @param v the pushed value, as char, int...
   */
  template <typename T>
  sonic_force_inline bool Push(T v) {
    static_assert(std::is_trivially_copyable<T>::value,
                  "Stack only supports trivially copyable values");
    if (sonic_unlikely(GrowTyped<T>(sizeof(T)) == nullptr)) return false;
#if defined(__GNUC__) && __GNUC__ >= 8
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstringop-overflow"
#pragma GCC diagnostic ignored "-Warray-bounds"
#endif
    *reinterpret_cast<T*>(top_) = v;
#if defined(__GNUC__) && __GNUC__ >= 8
#pragma GCC diagnostic pop
#endif
    top_ += sizeof(T);
    return true;
  }

  /**
   * @brief Push a string into buffer.
   * @param s the beginning of string
   * @param n the string size
   */
  sonic_force_inline bool Push(const char* s, size_t n) {
    if (sonic_unlikely(n == std::numeric_limits<size_t>::max())) {
      setOom();
      return false;
    }
    if (sonic_unlikely(Grow(n + 1) == nullptr)) return false;
#if defined(__GNUC__) && __GNUC__ >= 8
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstringop-overflow"
#pragma GCC diagnostic ignored "-Warray-bounds"
#endif
    std::memcpy(top_, s, n);
#if defined(__GNUC__) && __GNUC__ >= 8
#pragma GCC diagnostic pop
#endif
    top_ += n;
    return true;
  }
  sonic_force_inline void PushUnsafe(const char* s, size_t cnt) {
    std::memcpy(top_, s, cnt);
    top_ += cnt;
  }
  template <typename T>
  sonic_force_inline void PushUnsafe(T v) {
    *reinterpret_cast<T*>(top_) = v;
    top_ += sizeof(T);
  }

  template <typename T>
  sonic_force_inline T* PushSize(size_t n) {
    static_assert(std::is_trivially_copyable<T>::value,
                  "Stack only supports trivially copyable values");
    if (n == 0) {
      return reinterpret_cast<T*>(top_);
    }
    if (n > std::numeric_limits<size_t>::max() / sizeof(T)) {
      setOom();
      return nullptr;
    }
    if (sonic_unlikely(GrowTyped<T>(n * sizeof(T)) == nullptr)) return nullptr;
    return PushSizeUnsafe<T>(n);
  }

  template <typename T>
  sonic_force_inline T* PushSizeUnsafe(size_t n) {
    static_assert(std::is_trivially_copyable<T>::value,
                  "Stack only supports trivially copyable values");
    T* ret = reinterpret_cast<T*>(top_);
    top_ += n * sizeof(T);
    return ret;
  }

  // faster api for push 5 ~ 8 bytes.
  sonic_force_inline bool Push5_8(const char* bytes8, size_t n) {
    if (sonic_unlikely(Grow(8) == nullptr)) return false;
    std::memcpy(top_, bytes8, 8);
    top_ += n;
    return true;
  }

  /**
   * @brief Get the top value in the buffer.
   * @return the value pointer
   */
  template <typename T>
  sonic_force_inline const T* Top() const {
    const char* p = top_ - sizeof(T);
    sonic_assert(IsAligned(p, alignof(T)));
    return reinterpret_cast<const T*>(p);
  }
  template <typename T>
  sonic_force_inline T* Top() {
    char* p = top_ - sizeof(T);
    sonic_assert(IsAligned(p, alignof(T)));
    return reinterpret_cast<T*>(p);
  }

  /**
   * @brief Pop the top-N value in the buffer.
   */
  template <typename T>
  sonic_force_inline void Pop(size_t n) {
    top_ -= n * sizeof(T);
    return;
  }

  /**
   * @brief Increase the capacity of buffer if cnt is greater than the
   * remained capacity in the buffer. Otherwise, do nothing.
   */
  sonic_force_inline char* Grow(size_t cnt) {
    size_t old_size = Size();
    if (cnt > std::numeric_limits<size_t>::max() - old_size) {
      setOom();
      return nullptr;
    }
    size_t needed = old_size + cnt;
    if (sonic_unlikely(buf_ == nullptr || needed >= cap_)) {
      size_t new_cap = defaultCapcity();
      if (cap_ != 0) {
        new_cap = cap_ > std::numeric_limits<size_t>::max() / 2
                      ? std::numeric_limits<size_t>::max()
                      : cap_ * 2;
      }
      if (new_cap <= needed) {
        if (needed > std::numeric_limits<size_t>::max() - needed / 2) {
          setOom();
          return nullptr;
        }
        new_cap = needed + needed / 2;
      }
      if (new_cap <= needed) {
        if (needed == std::numeric_limits<size_t>::max()) {
          setOom();
          return nullptr;
        }
        new_cap = needed + 1;
      }
      if (sonic_unlikely(!Reserve(new_cap))) return nullptr;
    }
    sonic_assert(buf_ != NULL);
    return top_;
  }

  template <typename T>
  sonic_force_inline char* GrowTyped(size_t cnt) {
    size_t padding = AlignmentPadding(Size(), alignof(T));
    if (padding > std::numeric_limits<size_t>::max() - cnt) {
      setOom();
      return nullptr;
    }
    if (sonic_unlikely(Grow(padding + cnt) == nullptr)) return nullptr;
    top_ += padding;
    sonic_assert(IsAligned(top_, alignof(T)));
    return top_;
  }

  /**
   * @brief Get the end of the buffer.
   * @return the value pointer into the ending.
   */
  template <typename T>
  sonic_force_inline T* End() {
    return reinterpret_cast<T*>(top_);
  }
  template <typename T>
  sonic_force_inline const T* End() const {
    return reinterpret_cast<T*>(top_);
  }

  /**
   * @brief Get the begin of the buffer.
   * @return the value pointer into the begin.
   */
  template <typename T>
  sonic_force_inline T* Begin() {
    return reinterpret_cast<T*>(buf_);
  }
  template <typename T>
  sonic_force_inline const T* Begin() const {
    return reinterpret_cast<T*>(buf_);
  }

 private:
  void setZero() {
    buf_ = nullptr;
    top_ = nullptr;
    cap_ = 0;
    error_ = kErrorNone;
  }
  sonic_force_inline void setOom() { error_ = kErrorNoMem; }
  static sonic_force_inline size_t AlignmentPadding(size_t size,
                                                    size_t alignment) {
    size_t remainder = size & (alignment - 1);
    return remainder == 0 ? 0 : alignment - remainder;
  }
  static sonic_force_inline bool IsAligned(const void* ptr, size_t alignment) {
    return (reinterpret_cast<uintptr_t>(ptr) & (alignment - 1)) == 0;
  }
  static constexpr size_t defaultCapcity() { return 256; }
  char* buf_{nullptr};
  char* top_{nullptr};
  size_t cap_{0};
  SonicError error_{kErrorNone};
};

}  // namespace internal
}  // namespace sonic_json
