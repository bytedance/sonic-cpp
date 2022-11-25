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

#ifndef _SIMDJSON_HPP_
#define _SIMDJSON_HPP_
#include "json.h"
#include "simdjson.h"

class SIMDjsonStringResult : public StringResult<SIMDjsonStringResult> {
 public:
  std::string_view str_impl() const { return s.c_str(); }

  std::string s;
};

class SIMDjsonParseResult
    : public ParseResult<SIMDjsonParseResult, SIMDjsonStringResult> {
 public:
  simdjson::dom::parser parser_;
  simdjson::dom::element root_;
  simdjson::error_code error_;

  SIMDjsonParseResult(std::string_view json) { (void)json; }

  bool contains_impl(std::string_view key) const {
    (void)key;
    // auto v = root_.find_field(key);
    // if (simdjson::NO_SUCH_FIELD == v) {
    //   return false;
    // }
    return true;
  }

  bool stringfy_impl(SIMDjsonStringResult &sr) const {
    sr.s = simdjson::minify(root_);
    return true;
  }

  bool prettify_impl(SIMDjsonStringResult &sr) const {
    sr.s = simdjson::minify(root_);
    return true;
  }

  bool stat_impl(DocStat &stat) const {
    (void)stat;
    return true;
  }
  bool find_impl(DocStat &stat) const {
    (void)stat;
    return true;
  }
};

class SIMDjson : public JsonBase<SIMDjson, SIMDjsonParseResult> {
 public:
  bool parse_impl(std::string_view json, SIMDjsonParseResult &pr) const {
    pr.parser_.parse(json.data(), json.size()).tie(pr.root_, pr.error_);
    if (pr.error_) {
      return false;
    }
    return true;
  }
};

#endif
