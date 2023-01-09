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

#include <cstring>

#include "sonic/internal/stack.h"

namespace sonic_json {

class WriteBuffer {
 public:
  WriteBuffer() : stack_(){};
  WriteBuffer(size_t cap) : stack_(cap){};
  WriteBuffer(const WriteBuffer&) = delete;
  WriteBuffer(WriteBuffer&& rhs) : stack_(std::move(rhs.stack_)) {}
  ~WriteBuffer() = default;
  WriteBuffer& operator=(const WriteBuffer&) = delete;
  WriteBuffer& operator=(WriteBuffer&& rhs) {
    stack_ = std::move(rhs.stack_);
    return *this;
  }

  /**
   * @brief Return the context in the buffer.
   * @return a null-terminate string.
   * @note a '\0' will be added in the ending, so, this function is not
   * thread-safe.
   */
  sonic_force_inline const char* ToString() const {
    stack_.Push('\0');
    return stack_.Begin<char>();
  }

  sonic_force_inline size_t Size() const { return stack_.Size(); }
  sonic_force_inline size_t Capacity() const { return stack_.Capacity(); }
  sonic_force_inline bool Empty() const { return stack_.Empty(); }

  /**
   * @brief Increase the capacity of buffer if new_cap is greater than the
   * current capacity(). Otherwise, do nothing.
   */
  sonic_force_inline void Reserve(size_t new_cap) { stack_.Reserve(new_cap); }

  /**
   * @brief Erases all contexts in the buffer.
   */
  sonic_force_inline void Clear() { stack_.Clear(); }

  /**
   * @brief Push a value into buffer
   * @param v the pushed value, as char, int...
   */
  template <typename T>
  sonic_force_inline void Push(T v) {
    stack_.template Push<T>(v);
  }

  /**
   * @brief Push a string into the buffer.
   * @param s the beginning of string
   * @param n the string size
   */
  sonic_force_inline void Push(const char* s, size_t n) { stack_.Push(s, n); }
  sonic_force_inline void PushUnsafe(const char* s, size_t n) {
    stack_.PushUnsafe(s, n);
  }
  template <typename T>
  sonic_force_inline void PushUnsafe(T v) {
    stack_.template PushUnsafe<T>(v);
  }

  template <typename T>
  sonic_force_inline T* PushSize(size_t n) {
    return stack_.template PushSize<T>(n);
  }

  template <typename T>
  sonic_force_inline T* PushSizeUnsafe(size_t n) {
    return stack_.template PushSizeUnsafe<T>(n);
  }

  // faster api for push 5 ~ 8 bytes.
  sonic_force_inline void Push5_8(const char* bytes8, size_t n) {
    stack_.Push5_8(bytes8, n);
  }

  /**
   * @brief Get the top value in the buffer.
   * @return the value pointer
   */
  template <typename T>
  sonic_force_inline const T* Top() const {
    return stack_.template Top<T>();
  }
  template <typename T>
  sonic_force_inline T* Top() {
    return stack_.template Top<T>();
  }

  /**
   * @brief Pop the top-N value in the buffer.
   */
  template <typename T>
  sonic_force_inline void Pop(size_t n) {
    return stack_.template Pop<T>(n);
  }

  /**
   * @brief Increase the capacity of buffer if cnt is greater than the
   * remained capacity in the buffer. Otherwise, do nothing.
   */
  sonic_force_inline char* Grow(size_t cnt) { return stack_.Grow(cnt); }

  /**
   * @brief Get the end of the buffer.
   * @return the value pointer into the ending.
   */
  template <typename T>
  sonic_force_inline T* End() {
    return stack_.template End<T>();
  }
  template <typename T>
  sonic_force_inline const T* End() const {
    return stack_.template End<T>();
  }

  /**
   * @brief Get the begin of the buffer.
   * @return the value pointer into the begin.
   */
  template <typename T>
  sonic_force_inline T* Begin() {
    return stack_.template Begin<T>();
  }
  template <typename T>
  sonic_force_inline const T* Begin() const {
    return stack_.template Begin<T>();
  }

 private:
  mutable internal::Stack stack_;
};

}  // namespace sonic_json
