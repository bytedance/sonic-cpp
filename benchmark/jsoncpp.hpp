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

#ifndef _JsonCpp_HPP_
#define _JsonCpp_HPP_
#include "json.h"
#include "json/json.h"

class JsonCppStringResult : public StringResult<JsonCppStringResult> {
 public:
  std::string_view str_impl() const { return data.c_str(); }
  std::string data;
};

class JsonCppParseResult
    : public ParseResult<JsonCppParseResult, JsonCppStringResult> {
 public:
  Json::Value document;

  JsonCppParseResult(std::string_view json) { (void)json; }
  ~JsonCppParseResult() {}

  bool contains_impl(std::string_view key) const {
    return document.isMember(key.data(), key.data() + key.size());
  }

  bool stringfy_impl(JsonCppStringResult &sr) const {
    Json::StreamWriterBuilder writer_builder;
    std::unique_ptr<Json::StreamWriter> stream_writer(
        writer_builder.newStreamWriter());
    std::ostringstream oss;
    int res = stream_writer->write(document, &oss);
    sr.data = oss.str();
    return (res == 0 && oss.good());
  }

  bool prettify_impl(JsonCppStringResult &sr) const {
    (void)sr;
    return false;
  }

  bool stat_impl(DocStat &stat) const {
    (void)stat;
    return true;
  }

  bool find_impl(DocStat &stat) const {
    (void)stat;
    return true;
  }

 private:
};

class JsonCpp : public JsonBase<JsonCpp, JsonCppParseResult> {
 public:
  bool parse_impl(std::string_view json, JsonCppParseResult &pr) const {
    Json::CharReaderBuilder reader_builder;
    std::unique_ptr<Json::CharReader> char_reader(
        reader_builder.newCharReader());
    if (!char_reader->parse(json.data(), json.data() + json.size(),
                            &pr.document, nullptr)) {
      return false;
    }
    return true;
  }
};

#endif
