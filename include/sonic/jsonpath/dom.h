
#pragma once

#include <new>

#include "sonic/dom/generic_document.h"
#include "sonic/jsonpath/dump.h"

namespace sonic_json {

static constexpr ParseFlags kJsonPathParseFlags =
    ParseFlags::kParseAllowUnescapedControlChars |
    ParseFlags::kParseIntegerAsRaw;

sonic_force_inline std::tuple<std::string, SonicError> GetByJsonPathInternal(
    Document& dom, StringView jsonpath) {
  try {
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
      return std::make_tuple("null", result.error);
    }

    WriteBuffer wb;
    if (result.nodes.size() == 1) {
      // not serialize the single string
      auto& root = result.nodes[0];
      if (root->IsString()) {
        if (!wb.Push(root->GetStringView().data(), root->Size())) {
          return std::make_tuple("", kErrorNoMem);
        }
      } else {
        auto err =
            result.nodes[0]
                ->template Serialize<SerializeFlags::kSerializeEscapeEmoji>(wb);
        if (err != kErrorNone) {
          return std::make_tuple("", err);
        }
      }
    } else {
      if (!wb.Push('[')) return std::make_tuple("", kErrorNoMem);
      for (const auto& node : result.nodes) {
        auto err =
            node->template Serialize<SerializeFlags::kSerializeAppendBuffer |
                                     SerializeFlags::kSerializeEscapeEmoji>(wb);
        if (err != kErrorNone) {
          return std::make_tuple("", err);
        }
        if (!wb.Push(',')) return std::make_tuple("", kErrorNoMem);
      }
      if (*(wb.Top<char>()) == ',') {
        wb.Pop<char>(1);
      }
      if (!wb.Push(']')) return std::make_tuple("", kErrorNoMem);
    }
    auto sv = wb.ToStringView();
    return std::make_tuple(std::string(sv.data(), sv.size()), kErrorNone);
  } catch (const std::bad_alloc&) {
    return std::make_tuple("", kErrorNoMem);
  }
}

sonic_force_inline std::tuple<std::string, SonicError> GetByJsonPath(
    StringView json, StringView jsonpath) {
  // parse json into dom
  Document dom;
  dom.Parse<kJsonPathParseFlags>(json);
  if (dom.HasParseError()) {
    return std::make_tuple("", dom.GetParseError());
  }
  return GetByJsonPathInternal(dom, jsonpath);
}

sonic_force_inline
    std::tuple<std::vector<std::tuple<std::string, SonicError>>, SonicError>
    GetByJsonPaths(StringView json, const std::vector<StringView>& jsonpaths) {
  // parse json into dom
  Document dom;
  dom.Parse<kJsonPathParseFlags>(json);
  if (dom.HasParseError()) {
    return std::make_tuple(std::vector<std::tuple<std::string, SonicError>>(),
                           dom.GetParseError());
  }
  std::vector<std::tuple<std::string, SonicError>> results;
  try {
    results.reserve(jsonpaths.size());

    for (const auto& jsonpath : jsonpaths) {
      results.emplace_back(GetByJsonPathInternal(dom, jsonpath));
    }
  } catch (const std::bad_alloc&) {
    return std::make_tuple(std::vector<std::tuple<std::string, SonicError>>(),
                           kErrorNoMem);
  }
  return std::make_tuple(results, kErrorNone);
}
}  // namespace sonic_json
