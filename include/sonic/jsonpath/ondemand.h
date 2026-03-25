/*
 * Copyright (c) ByteDance Ltd. and/or its affiliates
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

#include <memory>
#include <optional>
#include <tuple>
#include <vector>

#include "sonic/dom/generic_document.h"
#include "sonic/dom/parser.h"
#include "sonic/jsonpath/dump.h"
#include "sonic/jsonpath/jsonpath.h"

namespace sonic_json {

template <SerializeFlags serializeFlags>
class JsonGenerator
    : public internal::SkipScanner2::JsonGeneratorInterface<serializeFlags> {
 public:
  JsonGenerator(Document& dom_doc, WriteBuffer& wb)
      : dom_doc_(dom_doc), wb_(wb) {}
  bool writeRaw(StringView raw) override {
    dom_doc_.template Parse<ParseFlags::kParseAllowUnescapedControlChars |
                            ParseFlags::kParseIntegerAsRaw>(raw);
    auto n = &dom_doc_;
    // check parse error
    if (dom_doc_.HasParseError()) {
      return false;
    }
    wb_.PushStr(n->GetStringView());

    return true;
  }
  bool writeComma() override {
    wb_.Push(',');
    return true;
  }
  bool isEmpty() override { return wb_.Empty(); }
  bool writeStartArray() override {
    wb_.Push('[');
    return true;
  }
  bool isBeginArray() override {
    return !wb_.Empty() && *(wb_.Top<char>()) == '[';
  }
  bool writeEndArray() override {
    wb_.Push(']');
    return true;
  }
  bool copyCurrentStructure(StringView raw) override {
    dom_doc_.template Parse<ParseFlags::kParseAllowUnescapedControlChars |
                            ParseFlags::kParseIntegerAsRaw>(raw);
    // check parse error
    if (dom_doc_.HasParseError()) {
      return false;
    }
    auto n = &dom_doc_;
    auto err = n->template Serialize<SerializeFlags::kSerializeAppendBuffer |
                                     SerializeFlags::kSerializeEscapeEmoji |
                                     serializeFlags>(wb_);
    if (sonic_unlikely(err != kErrorNone)) {
      return false;
    }

    return true;
  }
  bool copyCurrentStructureSingleResult(StringView raw) override {
    dom_doc_.template Parse<ParseFlags::kParseAllowUnescapedControlChars |
                            ParseFlags::kParseIntegerAsRaw>(raw);
    if (dom_doc_.HasParseError()) {
      return false;
    }

    auto n = &dom_doc_;
    if (n->IsString()) {
      wb_.PushStr(n->GetStringView());
      return true;
    }

    auto err = n->template Serialize<SerializeFlags::kSerializeAppendBuffer |
                                     SerializeFlags::kSerializeEscapeEmoji |
                                     serializeFlags>(wb_);
    if (sonic_unlikely(err != kErrorNone)) {
      return false;
    }
    return true;
  }
  bool copyCurrentStructureJsonTupleCodeGen(
      StringView raw, size_t index,
      std::vector<std::optional<std::string>>& result,
      internal::SkipScanner2::JsonValueType type) override {
    wb_.Clear();
    dom_doc_.template Parse<ParseFlags::kParseAllowUnescapedControlChars |
                            ParseFlags::kParseIntegerAsRaw>(raw);
    // check parse error
    if (dom_doc_.HasParseError()) {
      return false;
    }
    auto n = &dom_doc_;

    if (type == internal::SkipScanner2::JsonValueType::STRING) {
      // strip the quotes
      wb_.PushStr(n->GetStringView());
      result[index] = std::string(wb_.ToStringView());
      return true;
    }

    auto err = n->template Serialize<SerializeFlags::kSerializeAppendBuffer |
                                     SerializeFlags::kSerializeEscapeEmoji |
                                     serializeFlags>(wb_);
    if (sonic_unlikely(err != kErrorNone)) {
      return false;
    }

    result[index] = std::string(wb_.ToStringView());
    return true;
  }
  bool writeRawValue(StringView sv) override {
    this->wb_.PushStr(sv);
    return true;
  }
  ~JsonGenerator() override = default;

 private:
  Document& dom_doc_;
  WriteBuffer& wb_;
};

template <SerializeFlags serializeFlags = SerializeFlags::kSerializeDefault>
sonic_force_inline std::tuple<std::string, SonicError> GetByJsonPathOnDemand(
    StringView json, StringView jsonpath) {
  internal::SkipScanner2 scan;

  scan.data_ = reinterpret_cast<const uint8_t*>(json.data());
  scan.len_ = json.size();
  internal::JsonPath path;

  // padding some buffers
  std::string pathpadd = internal::paddingJsonPath(jsonpath);
  // Only parse the logical jsonpath length; the extra '\0' bytes are for safe
  // lookahead during unescaping.
  if (!path.ParsePadded(StringView(pathpadd.data(), pathpadd.size()),
                        jsonpath.size())) {
    return std::make_tuple("", kUnsupportedJsonPath);
  }

  Document dom_doc;
  WriteBuffer wb;

  const internal::SkipScanner2::JsonGeneratorFactory<serializeFlags>
      jsonGeneratorFactory = [&](WriteBuffer& local_wb) {
        std::shared_ptr<
            internal::SkipScanner2::JsonGeneratorInterface<serializeFlags>>
            local_ret = std::make_shared<JsonGenerator<serializeFlags>>(
                dom_doc, local_wb);
        return local_ret;
      };

  auto rootJsonGenerator = jsonGeneratorFactory(wb);
  const bool matched =
      scan.getJsonPath<internal::SkipScanner2::WriteStyle::RAW, serializeFlags>(
          path, 1, rootJsonGenerator.get(), jsonGeneratorFactory);
  if (matched) {
    return std::make_tuple(std::string(wb.ToStringView()), kErrorNone);
  }
  // if no match, it could be because valid json, just no path.
  if (!scan.hasError()) {
    return std::make_tuple("", kErrorNoneNoMatch);
  }
  // or parse error caused premature path match termination, hence no match.
  // In this case, return whatever that's been written to buffer.
  return std::make_tuple(std::string(wb.ToStringView()), scan.error_);
}

template <SerializeFlags serializeFlags = SerializeFlags::kSerializeDefault>
sonic_force_inline std::vector<std::optional<std::string>> JsonTupleWithCodeGen(
    StringView json, const std::vector<StringView>& keys, const bool legacy) {
  internal::SkipScanner2 scan;

  scan.data_ = reinterpret_cast<const uint8_t*>(json.data());
  scan.len_ = json.size();

  Document dom_doc;
  WriteBuffer wb;

  const internal::SkipScanner2::JsonGeneratorFactory<serializeFlags>
      jsonGeneratorFactory = [&](WriteBuffer& local_wb) {
        std::shared_ptr<
            internal::SkipScanner2::JsonGeneratorInterface<serializeFlags>>
            local_ret = std::make_shared<JsonGenerator<serializeFlags>>(
                dom_doc, local_wb);
        return local_ret;
      };

  return scan.jsonTupleWithCodeGen(keys, jsonGeneratorFactory(wb).get(),
                                   legacy);
}

}  // namespace sonic_json
