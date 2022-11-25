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

#ifndef _CJSON_HPP_
#define _CJSON_HPP_
#include "cJSON.h"
#include "json.h"

class cJsonStringResult : public StringResult<cJsonStringResult> {
 public:
  std::string_view str_impl() const { return s; }
  cJsonStringResult() : s(nullptr) {}
  ~cJsonStringResult() { cJSON_free(s); }
  char *s;
};

class cJsonParseResult
    : public ParseResult<cJsonParseResult, cJsonStringResult> {
 public:
  cJSON *json;

  cJsonParseResult(std::string_view json) { (void)json; }
  ~cJsonParseResult() { cJSON_Delete(json); }

  bool contains_impl(std::string_view key) const {
    (void)key;
    return false;
  }

  bool stringfy_impl(cJsonStringResult &sr) const {
    sr.s = cJSON_PrintUnformatted(json);
    return sr.s != nullptr;
  }

  bool prettify_impl(cJsonStringResult &sr) const {
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
};

class cJson : public JsonBase<cJson, cJsonParseResult> {
 public:
  bool parse_impl(std::string_view json, cJsonParseResult &pr) const {
    pr.json = cJSON_Parse(json.data());
    if (pr.json == NULL) {
      return false;
    }
    return true;
  }
};

#endif
