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

#ifndef _YYJSON_HPP_
#define _YYJSON_HPP_
#include "json.h"
#include "yyjson.h"

class YYjsonStringResult : public StringResult<YYjsonStringResult> {
 public:
  std::string_view str_impl() const { return s; }
  YYjsonStringResult() : s(nullptr) {}
  ~YYjsonStringResult() { std::free(s); }
  char *s;
};

class YYjsonParseResult
    : public ParseResult<YYjsonParseResult, YYjsonStringResult> {
 public:
  yyjson_doc *doc;
  // yyjson_val *root;

  YYjsonParseResult(std::string_view json) { (void)json; }
  ~YYjsonParseResult() { yyjson_doc_free(doc); }

  bool contains_impl(std::string_view key) const {
    (void)key;
    return NULL != yyjson_obj_get(yyjson_doc_get_root(doc), "name");
  }

  bool stringfy_impl(YYjsonStringResult &sr) const {
    size_t out_size = 0;
    sr.s = yyjson_write(doc, YYJSON_WRITE_NOFLAG, &out_size);
    return out_size != 0;
  }

  bool prettify_impl(YYjsonStringResult &sr) const {
    size_t out_size = 0;
    sr.s = yyjson_write(doc, YYJSON_WRITE_PRETTY, &out_size);
    return out_size != 0;
  }

  bool stat_impl(DocStat &stat) const {
    GetStats(yyjson_doc_get_root(doc), stat);
    return true;
  }

  bool find_impl(DocStat &stat) const {
    (void)stat;
    return true;
  }

 private:
  void GetStat(yyjson_val *val, DocStat &stat) const {
    if (yyjson_is_str(val)) {
      stat.strings++;
      stat.length += yyjson_get_len(val);
    } else if (yyjson_is_num(val)) {
      stat.numbers++;
    } else if (yyjson_is_true(val)) {
      stat.trues++;
    } else if (yyjson_is_false(val)) {
      stat.falses++;
    } else if (yyjson_is_null(val)) {
      stat.nulls++;
    }
  }
  void GetStats(yyjson_val *val, DocStat &stat, size_t depth = 0) const {
    if (!val) return;
    if (yyjson_is_arr(val)) {
      stat.arrays++;
      if (depth > stat.depth) stat.depth = depth;
      // not safe
      size_t idx, max;
      yyjson_val *tmp;
      yyjson_arr_foreach(val, idx, max, tmp) {
        if (yyjson_is_ctn(tmp))
          GetStats(tmp, stat, depth + 1);
        else
          GetStat(tmp, stat);
      }
    } else if (yyjson_is_obj(val)) {
      stat.objects++;
      if (depth > stat.depth) stat.depth = depth;
      // not safe
      size_t idx, max;
      yyjson_val *k, *v;
      yyjson_obj_foreach(val, idx, max, k, v) {
        if (yyjson_is_ctn(v))
          GetStats(v, stat, depth + 1);
        else
          GetStat(v, stat);
        stat.length += yyjson_get_len(v);
      }
      stat.members += yyjson_obj_size(val);
      stat.strings += yyjson_obj_size(val);
    } else {
      GetStat(val, stat);
    }
  }
};

class YYjson : public JsonBase<YYjson, YYjsonParseResult> {
 public:
  bool parse_impl(std::string_view json, YYjsonParseResult &pr) const {
    pr.doc = yyjson_read(json.data(), json.size(), 0);
    // yyjson_mut_doc *docm = yyjson_doc_mut_copy(pr.doc, NULL);
    if (pr.doc == NULL) {
      return false;
    }
    return true;
  }
};

#endif
