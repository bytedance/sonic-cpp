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
   * @brief Return the context in buffer.
   * @return a null-terminate string.
   */
  sonic_force_inline const char* ToString() const {
    stack_.Push('\0');
    return stack_.Begin<char>();
  }

  sonic_force_inline size_t Size() const { return stack_.Size(); }
  sonic_force_inline size_t Capacity() const { return stack_.Capacity(); }
  sonic_force_inline bool Empty() const { return stack_.Empty(); }

  mutable internal::Stack stack_;
};

}  // namespace sonic_json
