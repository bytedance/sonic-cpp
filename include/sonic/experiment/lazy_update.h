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

#include "sonic/allocator.h"
#include "sonic/dom/dynamicnode.h"
#include "sonic/dom/parser.h"
#include "sonic/string_view.h"

namespace sonic_json {

namespace internal {

template <typename NodeType, typename Allocator>
static inline ParseResult ParseLazy(NodeType &node, StringView json,
                                    Allocator &alloc) {
  LazySAXHandler<NodeType> sax(alloc);
  Parser p;
  ParseResult ret = p.ParseLazy(reinterpret_cast<const uint8_t *>(json.data()),
                                json.size(), sax);
  if (ret.Error()) {
    return ret;
  }
  NodeType *root = sax.stack_.template Begin<NodeType>();
  node = std::move(*root);
  return ret;
}

template <typename NodeType, typename Allocator>
static inline SonicError UpdateNodeLazy(NodeType &target, NodeType &source,
                                        Allocator &alloc) {
  ParseResult ret;
  SonicError err = kErrorNone;
  // check the raw type
  if (target.IsRaw() && *target.GetRaw().data() == '{') {
    ret = ParseLazy(target, target.GetRaw(), alloc);
  }
  if (source.IsRaw() && *source.GetRaw().data() == '{') {
    ret = ParseLazy(source, source.GetRaw(), alloc);
  }
  if (ret.Error()) {
    return ret.Error();
  }
  // update the object type
  if (!target.IsObject() || !source.IsObject() || target.Empty()) {
    target = std::move(source);
    return kErrorNone;
  }
  target.CreateMap(alloc);
  auto source_begin = source.MemberBegin(), source_end = source.MemberEnd();
  for (auto iter = source_begin; iter != source_end; iter++) {
    StringView key = iter->name.GetStringView();
    auto match = target.FindMember(key);
    if (match == target.MemberEnd()) {
      target.AddMember(key, iter->value, alloc);
    } else {
      err = UpdateNodeLazy(match->value, iter->value, alloc);
      if (err) return err;
    }
  }
  return err;
}

}  // namespace internal

/**
 * @brief UpdateLazy will update the target json with the source json, and
 * return the updated json. The update rules is: If the key is exist, the update
 * the value of target from source json. Otherwise, insert the key/value pairs
 * from source json.
 * @param target the target json
 * @param source the source json
 */
static inline std::string UpdateLazy(StringView target, StringView source) {
  using Allocator = Node::AllocatorType;
  Allocator alloc;
  WriteBuffer wb(target.size() + source.size());
  SonicError err = kErrorNone;
  ParseResult ret1, ret2;

  Node ntarget, nsource;
  ret1 = internal::ParseLazy(ntarget, target, alloc);
  ret2 = internal::ParseLazy(nsource, source, alloc);
  if (ret2.Error()) {
    return ret1.Error() ? "{}" : std::string(target.data(), target.size());
  }
  if (ret1.Error()) {
    return std::string(source.data(), source.size());
  }
  err = internal::UpdateNodeLazy(ntarget, nsource, alloc);
  if (err) {
    return "{}";
  }
  err = ntarget.Serialize(wb);
  if (err) {
    return "{}";
  }
  return std::string(wb.ToString(), wb.Size());
}

}  // namespace sonic_json