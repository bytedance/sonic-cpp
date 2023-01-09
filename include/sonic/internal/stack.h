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

#include <cstdlib>

#include "sonic/allocator.h"
#include "sonic/macro.h"

namespace sonic_json {
namespace internal {

class Stack {
 public:
  Stack(size_t cap = defaultCapcity()) : cap_(cap) { Reserve(cap); };
  Stack(const Stack&) = delete;
  Stack(Stack&& rhs) : buf_(rhs.buf_), top_(rhs.top_), cap_(rhs.cap_) {
    rhs.setZero();
  }
  ~Stack() { std::free(buf_); }
  Stack& operator=(const Stack&) = delete;
  Stack& operator=(Stack&& rhs) {
    std::free(buf_);
    buf_ = rhs.buf_;
    top_ = rhs.top_;
    cap_ = rhs.cap_;
    rhs.setZero();
    return *this;
  }

  sonic_force_inline size_t Size() const { return top_ - buf_; }
  sonic_force_inline size_t Capacity() const { return cap_; }
  sonic_force_inline bool Empty() const { return Size() == 0; }

  /**
   * @brief Increase the capacity of buffer if new_cap is greater than the
   * current capacity(). Otherwise, do nothing.
   */
  sonic_force_inline void Reserve(size_t new_cap) {
    if (new_cap < Capacity()) {
      return;
    }
    size_t align_cap = SONIC_ALIGN(new_cap);
    char* tmp = static_cast<char*>(std::realloc(buf_, align_cap));
    top_ = tmp + Size();
    buf_ = tmp;
    sonic_assert(buf_ != NULL);
    cap_ = buf_ ? new_cap : 0;
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
  sonic_force_inline void Push(T v) {
    Grow(sizeof(T));
    *reinterpret_cast<T*>(top_) = v;
    top_ += sizeof(T);
  }

  /**
   * @brief Push a string into buffer.
   * @param s the begining of string
   * @param n the string size
   */
  sonic_force_inline void Push(const char* s, size_t n) {
    Grow(n + 1);
    std::memcpy(top_, s, n);
    top_ += n;
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
    Grow(n * sizeof(T));
    return PushSizeUnsafe<T>(n);
  }

  template <typename T>
  sonic_force_inline T* PushSizeUnsafe(size_t n) {
    T* ret = reinterpret_cast<T*>(top_);
    top_ += n * sizeof(T);
    return ret;
  }

  // faster api for push 5 ~ 8 bytes.
  sonic_force_inline void Push5_8(const char* bytes8, size_t n) {
    Grow(8);
    std::memcpy(top_, bytes8, 8);
    top_ += n;
  }

  /**
   * @brief Get the top value in the buffer.
   * @return the value pointer
   */
  template <typename T>
  sonic_force_inline const T* Top() const {
    return reinterpret_cast<const T*>(top_ - sizeof(T));
  }
  template <typename T>
  sonic_force_inline T* Top() {
    return reinterpret_cast<T*>(top_ - sizeof(T));
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
    if (sonic_unlikely(top_ + cnt >= buf_ + cap_)) {
      if (sonic_unlikely((top_ + cnt) > buf_ + 2 * cap_)) {
        cap_ = top_ - buf_ + cnt;
        Reserve(cap_ + cap_ / 2);
      } else {
        Reserve(cap_ * 2);
      }
    }
    sonic_assert(buf_ != NULL);
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
  }
  static constexpr size_t defaultCapcity() { return 256; }
  char* buf_{nullptr};
  char* top_{nullptr};
  size_t cap_{0};
};

}  // namespace internal
}  // namespace sonic_json