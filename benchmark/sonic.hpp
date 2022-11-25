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

#ifndef _SONIC_HPP_
#define _SONIC_HPP_
#include "json.h"
#include "sonic/sonic.h"

template <typename NodeType>
class SonicStringResult : public StringResult<SonicStringResult<NodeType>> {
 public:
  std::string_view str_impl() const {
    return const_cast<sonic_json::WriteBuffer &>(wb).ToString();
  }
  sonic_json::WriteBuffer wb;
};

template <typename NodeType>
class SonicParseResult : public ParseResult<SonicParseResult<NodeType>,
                                            SonicStringResult<NodeType>> {
 public:
  sonic_json::GenericDocument<NodeType> doc;

  SonicParseResult(std::string_view json) { (void)json; }
  ~SonicParseResult() {}

  bool contains_impl(std::string_view key) const {
    (void)key;
    return false;
  }

  bool stringfy_impl(SonicStringResult<NodeType> &sr) const {
    auto err = doc.Serialize(sr.wb);
    if (err) return false;

    return true;
  }

  bool prettify_impl(SonicStringResult<NodeType> &sr) const {
    (void)sr;
    return false;
  }

  bool stat_impl(DocStat &stat) const {
    GetStats(doc, stat);
    return true;
  }

  bool find_impl(DocStat &stat) const {
    find_value(doc, stat);
    return true;
  }

 private:
  void find_value(const NodeType &v, DocStat &stat) const {
    stat = DocStat();
    switch (v.GetType()) {
      case sonic_json::kObject:
        for (auto m = v.MemberBegin(); m != v.MemberEnd(); ++m) {
          auto re = v.FindMember(m->name.GetStringView());
          if (re != v.MemberEnd()) {
            stat.members++;
            find_value(re->value, stat);
          }
        }
        break;
      case sonic_json::kArray:
        for (size_t i = 0; i < v.Size(); ++i) {
          find_value(v[i], stat);
        }
        break;
      default:
        break;
    }
  }
  void GetStats(const NodeType &v, DocStat &stat, size_t depth = 0) const {
    stat = DocStat();
    switch (v.GetType()) {
      case sonic_json::kNull:
        stat.nulls++;
        break;
      case sonic_json::kFalse:
        stat.falses++;
        break;
      case sonic_json::kTrue:
        stat.trues++;
        break;
      case sonic_json::kObject:
        if (depth > stat.depth) stat.depth = depth;
        for (auto m = v.MemberBegin(); m != v.MemberEnd(); ++m) {
          stat.length += m->name.Size();
          GetStats(m->value, stat, depth + 1);
        }
        stat.objects++;
        stat.members += v.Size();
        stat.strings += v.Size();
        break;
      case sonic_json::kArray:
        if (depth > stat.depth) stat.depth = depth;
        for (auto i = v.Begin(); i != v.End(); ++i) {
          GetStats(*i, stat, depth + 1);
        }
        stat.arrays++;
        stat.elements += v.Size();
        break;
      case sonic_json::kStringCopy:
      case sonic_json::kStringFree:
      case sonic_json::kStringConst:
        stat.strings++;
        stat.length += v.Size();
        break;
      case sonic_json::kReal:
      case sonic_json::kUint:
      case sonic_json::kSint:
        stat.numbers++;
        break;
      default:
        break;
    }
  }
};

template <typename NodeType>
class Sonic : public JsonBase<Sonic<NodeType>, SonicParseResult<NodeType>> {
 public:
  using node_type = NodeType;
  bool parse_impl(std::string_view json, SonicParseResult<NodeType> &pr) const {
    // sonic_json::Parser<NodeType> parser;
    // auto ret = parser.Parse(json.data(), json.size(), pr.doc);
    pr.doc.Parse(json.data(), json.size());
    if (pr.doc.HasParseError()) {
      return false;
    }
    return true;
  }
};

using SonicDynParseResult = SonicParseResult<sonic_json::DNode<>>;
using SonicDynStringResult = SonicStringResult<sonic_json::DNode<>>;
using SonicDyn = Sonic<sonic_json::DNode<>>;

#endif
