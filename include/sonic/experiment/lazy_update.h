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

#include <new>
#include <tuple>

#include "sonic/allocator.h"
#include "sonic/dom/dynamicnode.h"
#include "sonic/dom/parser.h"
#include "sonic/string_view.h"

namespace sonic_json {

namespace internal {

template <typename NodeType, typename Allocator, ParseFlags parseFlags>
static inline ParseResult ParseLazy(NodeType& node, StringView json,
                                    Allocator& alloc,
                                    bool copyBorrowedValues = false) {
  LazySAXHandler<NodeType> sax(alloc);
  Parser<parseFlags> p;
  ParseResult ret = p.ParseLazy(reinterpret_cast<const uint8_t*>(json.data()),
                                json.size(), sax);
  if (ret.Error()) {
    return ret;
  }
  if (sonic_unlikely(sax.oom_)) {
    return ParseResult(kErrorNoMem, json.size());
  }
  NodeType* root = sax.Root();
  if (copyBorrowedValues) {
    NodeType owned;
    if (sonic_unlikely(!owned.TryCopyFrom(*root, alloc, true))) {
      return ParseResult(kErrorNoMem, ret.Offset());
    }
    node = std::move(owned);
  } else {
    node = std::move(*root);
  }
  return ret;
}

template <typename NodeType, typename Allocator, ParseFlags parseFlags>
static inline SonicError UpdateNodeLazyInPlace(NodeType& target,
                                               NodeType& source,
                                               Allocator& alloc) {
  SonicError err = kErrorNone;
  if (target.IsRaw() && !target.GetRaw().empty() &&
      *target.GetRaw().data() == '{') {
    ParseResult ret = ParseLazy<NodeType, Allocator, parseFlags>(
        target, target.GetRaw(), alloc, true);
    if (ret.Error()) return ret.Error();
  }
  if (source.IsRaw() && !source.GetRaw().empty() &&
      *source.GetRaw().data() == '{') {
    ParseResult ret = ParseLazy<NodeType, Allocator, parseFlags>(
        source, source.GetRaw(), alloc, true);
    if (ret.Error()) return ret.Error();
  }
  // update the object type
  if (!target.IsObject() || !source.IsObject() || target.Empty()) {
    target = std::move(source);
    return kErrorNone;
  }
  if (!target.CreateMap(alloc)) return kErrorNoMem;
  auto source_begin = source.MemberBegin(), source_end = source.MemberEnd();
  for (auto iter = source_begin; iter != source_end; iter++) {
    StringView key = iter->name.GetStringView();
    auto match = target.FindMember(key);
    if (match == target.MemberEnd()) {
      SonicError add_err =
          target.AddMemberWithError(key, std::move(iter->value), alloc);
      if (add_err) return add_err;
    } else {
      err = UpdateNodeLazyInPlace<NodeType, Allocator, parseFlags>(
          match->value, iter->value, alloc);
      if (err) return err;
    }
  }
  return err;
}

template <typename NodeType, typename Allocator, ParseFlags parseFlags>
static inline SonicError UpdateNodeLazy(NodeType& target, NodeType& source,
                                        Allocator& alloc) {
  NodeType shadow;
  if (sonic_unlikely(!shadow.TryCopyFrom(target, alloc))) {
    return kErrorNoMem;
  }
  SonicError err = UpdateNodeLazyInPlace<NodeType, Allocator, parseFlags>(
      shadow, source, alloc);
  if (err) return err;
  target = std::move(shadow);
  return kErrorNone;
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
template <ParseFlags parseFlags = ParseFlags::kParseDefault>
static inline std::tuple<std::string, SonicError> UpdateLazyWithError(
    StringView target, StringView source) {
  try {
    using Allocator = Node::AllocatorType;
    Allocator alloc;
    WriteBuffer wb(target.size() + source.size());
    SonicError err = kErrorNone;
    ParseResult ret1, ret2;

    Node ntarget, nsource;
    ret1 = internal::ParseLazy<Node, Allocator, parseFlags>(ntarget, target,
                                                            alloc);
    ret2 = internal::ParseLazy<Node, Allocator, parseFlags>(nsource, source,
                                                            alloc);
    if (ret2.Error()) {
      return std::make_tuple(
          ret1.Error() ? "{}" : std::string(target.data(), target.size()),
          ret2.Error());
    }
    if (ret1.Error()) {
      return std::make_tuple(std::string(source.data(), source.size()),
                             ret1.Error());
    }
    err = internal::UpdateNodeLazy<Node, Allocator, parseFlags>(ntarget,
                                                                nsource, alloc);
    if (err) {
      return std::make_tuple("{}", err);
    }
    err = ntarget.Serialize(wb);
    if (err) {
      return std::make_tuple("{}", err);
    }
    auto sv = wb.ToStringView();
    return std::make_tuple(std::string(sv.data(), sv.size()), kErrorNone);
  } catch (const std::bad_alloc&) {
    return std::make_tuple("{}", kErrorNoMem);
  }
}

template <ParseFlags parseFlags = ParseFlags::kParseDefault>
static inline std::string UpdateLazy(StringView target, StringView source) {
  auto ret = UpdateLazyWithError<parseFlags>(target, source);
  return std::move(std::get<0>(ret));
}

}  // namespace sonic_json
