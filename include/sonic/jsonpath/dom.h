
#pragma once

#include "sonic/dom/generic_document.h"
#include "sonic/jsonpath/dump.h"

namespace sonic_json {

sonic_force_inline std::tuple<std::string, SonicError> GetByJsonPathInternal(
    Document& dom, StringView jsonpath) {
  // get the nodes
  auto result = dom.AtJsonPath(jsonpath);
  if (result.error != kErrorNone) {
    return std::make_tuple("", result.error);
  }

  // filter the null nodes
  result.nodes.erase(
      std::remove_if(result.nodes.begin(), result.nodes.end(),
                     [](const auto& node) { return node->IsNull(); }),
      result.nodes.end());

  if (result.nodes.empty()) {
    return std::make_tuple("", result.error);
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

sonic_force_inline std::tuple<std::string, SonicError> GetByJsonPath(
    StringView json, StringView jsonpath) {
  // parse json into dom
  Document dom;
  dom.Parse(json);
  if (dom.HasParseError()) {
    return std::make_tuple("", dom.GetParseError());
  }
  return GetByJsonPathInternal(dom, jsonpath);
}

sonic_force_inline
    std::tuple<std::vector<std::tuple<std::string, SonicError>>, SonicError>
    GetByJsonPaths(StringView json, std::vector<StringView> jsonpaths) {
  // parse json into dom
  Document dom;
  dom.Parse(json);
  if (dom.HasParseError()) {
    return std::make_tuple(std::vector<std::tuple<std::string, SonicError>>(),
                           dom.GetParseError());
  }
  std::vector<std::tuple<std::string, SonicError>> results;

  for (const auto& jsonpath : jsonpaths) {
    results.emplace_back(GetByJsonPathInternal(dom, jsonpath));
  }
  return std::make_tuple(results, kErrorNone);
}
}  // namespace sonic_json
