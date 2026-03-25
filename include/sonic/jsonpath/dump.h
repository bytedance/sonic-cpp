
#pragma once

#include <algorithm>
#include <string>
#include <tuple>

#include "sonic/dom/generic_document.h"

namespace sonic_json {

namespace internal {
template <typename NodeType>
sonic_force_inline std::tuple<std::string, SonicError> Serialize(
    const JsonPathResult<NodeType>& result) {
  auto local = result;
  // filter the null nodes
  local.nodes.erase(
      std::remove_if(local.nodes.begin(), local.nodes.end(),
                     [](const auto& node) { return node->IsNull(); }),
      local.nodes.end());

  if (local.nodes.empty()) {
    return std::make_tuple("null", kErrorNone);
  }

  WriteBuffer wb;
  if (local.nodes.size() == 1) {
    // not serialize the single string
    auto& root = local.nodes[0];
    if (root->IsString()) {
      wb.Push(root->GetStringView().data(), root->Size());
    } else {
      auto err =
          local.nodes[0]
              ->template Serialize<SerializeFlags::kSerializeEscapeEmoji>(wb);
      if (err != kErrorNone) {
        return std::make_tuple("", err);
      }
    }
  } else {
    wb.Push('[');
    for (const auto& node : local.nodes) {
      auto err =
          node->template Serialize<SerializeFlags::kSerializeAppendBuffer |
                                   SerializeFlags::kSerializeEscapeEmoji>(wb);
      if (err != kErrorNone) {
        return std::make_tuple("", err);
      }
      wb.Push(',');
    }
    if (*(wb.Top<char>()) == ',') {
      wb.Pop<char>(1);
    }
    wb.Push(']');
  }
  auto sv = wb.ToStringView();
  return std::make_tuple(std::string(sv.data(), sv.size()), kErrorNone);
}
}  // namespace internal

}  // namespace sonic_json
