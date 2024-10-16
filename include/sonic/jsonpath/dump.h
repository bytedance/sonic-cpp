
#pragma once

#include "sonic/dom/generic_document.h"

namespace sonic_json {

namespace internal {
template <typename NodeType>
sonic_force_inline std::tuple<std::string, SonicError> Serialize( JsonPathResult<NodeType>&  result) {
  // filter the null nodes
  result.nodes.erase(
      std::remove_if(result.nodes.begin(), result.nodes.end(),
                     [](const auto& node) { return node->IsNull(); }),
      result.nodes.end());

  if (result.nodes.empty()) {
    return std::make_tuple("null", kErrorNone);
  }

  WriteBuffer wb;
  if (result.nodes.size() == 1) {
    // not serialize the single string
    auto& root = result.nodes[0];
    if (root->IsString()) {
      wb.Push(root->GetStringView().data(), root->Size());
    } else {
      auto err = result.nodes[0]->template Serialize<kSerializeEscapeEmoji>(wb);
      if (err != kErrorNone) {
        return std::make_tuple("", err);
      }
    }
  } else {
    wb.Push('[');
    for (const auto& node : result.nodes) {
      auto err = node->template Serialize<kSerializeAppendBuffer |
                                          kSerializeEscapeEmoji>(wb);
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
  return std::make_tuple(wb.ToString(), kErrorNone);
}
}

}