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
#include <cstdint>

#include "sonic/string_view.h"

namespace sonic_json {
namespace internal {

// Root-only selection using the same SkipOne path as template ParseSchema.
// Rejected values are not fully validated. Nested retained values are complete.
template <typename Handler, typename Predicate>
class RootSchemaFilter : public Handler {
 public:
  template <typename Allocator>
  RootSchemaFilter(Allocator& alloc, const Predicate& keep)
      : Handler(alloc), keep_(keep) {}

  bool Key(StringView key) {
    if (root_object_ && depth_ == 1 && !keep_(key)) return false;
    return Handler::Key(key);
  }

  bool StartObject() {
    if (depth_ == 0) root_object_ = true;
    ++depth_;
    return Handler::StartObject();
  }

  bool StartArray() {
    ++depth_;
    return Handler::StartArray();
  }

  bool EndObject(uint32_t count) {
    --depth_;
    return Handler::EndObject(count);
  }

  bool EndArray(uint32_t count) {
    --depth_;
    return Handler::EndArray(count);
  }

  static constexpr bool check_key_return = true;

 private:
  const Predicate& keep_;
  size_t depth_{0};
  bool root_object_{false};
};

}  // namespace internal
}  // namespace sonic_json
