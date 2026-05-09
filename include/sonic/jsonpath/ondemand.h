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
#include <new>
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
      error_ = dom_doc_.GetParseError();
      return false;
    }
    if (sonic_unlikely(!wb_.PushStr(n->GetStringView()))) {
      error_ = kErrorNoMem;
      return false;
    }

    return true;
  }
  bool writeComma() override { return writeChar(','); }
  bool isEmpty() override { return wb_.Empty(); }
  bool writeStartArray() override { return writeChar('['); }
  bool isBeginArray() override {
    return !wb_.Empty() && *(wb_.Top<char>()) == '[';
  }
  bool writeEndArray() override { return writeChar(']'); }
  bool copyCurrentStructure(StringView raw) override {
    dom_doc_.template Parse<ParseFlags::kParseAllowUnescapedControlChars |
                            ParseFlags::kParseIntegerAsRaw>(raw);
    // check parse error
    if (dom_doc_.HasParseError()) {
      error_ = dom_doc_.GetParseError();
      return false;
    }
    auto n = &dom_doc_;
    auto err = n->template Serialize<SerializeFlags::kSerializeAppendBuffer |
                                     SerializeFlags::kSerializeEscapeEmoji |
                                     serializeFlags>(wb_);
    if (sonic_unlikely(err != kErrorNone)) {
      error_ = err;
      return false;
    }

    return true;
  }
  bool copyCurrentStructureSingleResult(StringView raw) override {
    dom_doc_.template Parse<ParseFlags::kParseAllowUnescapedControlChars |
                            ParseFlags::kParseIntegerAsRaw>(raw);
    if (dom_doc_.HasParseError()) {
      error_ = dom_doc_.GetParseError();
      return false;
    }

    auto n = &dom_doc_;
    if (n->IsString()) {
      if (sonic_unlikely(!wb_.PushStr(n->GetStringView()))) {
        error_ = kErrorNoMem;
        return false;
      }
      return true;
    }

    auto err = n->template Serialize<SerializeFlags::kSerializeAppendBuffer |
                                     SerializeFlags::kSerializeEscapeEmoji |
                                     serializeFlags>(wb_);
    if (sonic_unlikely(err != kErrorNone)) {
      error_ = err;
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
      error_ = dom_doc_.GetParseError();
      return false;
    }
    auto n = &dom_doc_;

    if (type == internal::SkipScanner2::JsonValueType::STRING) {
      // strip the quotes
      if (sonic_unlikely(!wb_.PushStr(n->GetStringView()))) {
        error_ = kErrorNoMem;
        return false;
      }
      result[index] = std::string(wb_.ToStringView());
      return true;
    }

    auto err = n->template Serialize<SerializeFlags::kSerializeAppendBuffer |
                                     SerializeFlags::kSerializeEscapeEmoji |
                                     serializeFlags>(wb_);
    if (sonic_unlikely(err != kErrorNone)) {
      error_ = err;
      return false;
    }

    result[index] = std::string(wb_.ToStringView());
    return true;
  }
  bool writeRawValue(StringView sv) override {
    if (sonic_unlikely(!this->wb_.PushStr(sv))) {
      error_ = kErrorNoMem;
      return false;
    }
    return true;
  }
  SonicError getError() const override { return error_; }
  ~JsonGenerator() override = default;

 private:
  bool writeChar(char c) {
    if (sonic_unlikely(!wb_.Push(c))) {
      error_ = kErrorNoMem;
      return false;
    }
    return true;
  }

  Document& dom_doc_;
  WriteBuffer& wb_;
  SonicError error_{kErrorNone};
};

template <SerializeFlags serializeFlags = SerializeFlags::kSerializeDefault>
sonic_force_inline std::tuple<std::string, SonicError> GetByJsonPathOnDemand(
    StringView json, StringView jsonpath) {
  try {
    auto publicError = [](SonicError err) {
      return err == kParseErrorEof ? kParseErrorInvalidChar : err;
    };
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
        scan.getJsonPath<internal::SkipScanner2::WriteStyle::RAW,
                         serializeFlags>(path, 1, rootJsonGenerator.get(),
                                         jsonGeneratorFactory);
    if (matched) {
      if (!scan.consumeOnlyTrailingSpaces()) {
        return std::make_tuple("", publicError(scan.error_));
      }
      return std::make_tuple(std::string(wb.ToStringView()), kErrorNone);
    }
    // if no match, it could be because valid json, just no path.
    if (!scan.hasError()) {
      if (!scan.consumeOnlyTrailingSpaces()) {
        return std::make_tuple("", publicError(scan.error_));
      }
      return std::make_tuple("", kErrorNoneNoMatch);
    }
    // Or a parse error caused premature path termination. Do not return partial
    // output with an error; callers should only consume data on kErrorNone.
    return std::make_tuple("", publicError(scan.error_));
  } catch (const std::bad_alloc&) {
    return std::make_tuple("", kErrorNoMem);
  }
}

template <SerializeFlags serializeFlags = SerializeFlags::kSerializeDefault>
sonic_force_inline
    std::tuple<std::vector<std::optional<std::string>>, SonicError>
    JsonTupleWithCodeGenWithError(StringView json,
                                  const std::vector<StringView>& keys,
                                  const bool legacy) {
  try {
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

    auto result =
        scan.jsonTupleWithCodeGen(keys, jsonGeneratorFactory(wb).get(), legacy);
    return std::make_tuple(std::move(result), scan.error_);
  } catch (const std::bad_alloc&) {
    return std::make_tuple(std::vector<std::optional<std::string>>{},
                           kErrorNoMem);
  }
}

template <SerializeFlags serializeFlags = SerializeFlags::kSerializeDefault>
sonic_force_inline std::vector<std::optional<std::string>> JsonTupleWithCodeGen(
    StringView json, const std::vector<StringView>& keys, const bool legacy) {
  auto ret = JsonTupleWithCodeGenWithError<serializeFlags>(json, keys, legacy);
  return std::move(std::get<0>(ret));
}

}  // namespace sonic_json
