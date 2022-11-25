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

#ifndef _RAPIDJSON_HPP_
#define _RAPIDJSON_HPP_
#include "json.h"
#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"

class RapidjsonStringResult : public StringResult<RapidjsonStringResult> {
 public:
  std::string_view str_impl() const { return sb.GetString(); }

  rapidjson::StringBuffer sb;
};

class RapidjsonParseResult
    : public ParseResult<RapidjsonParseResult, RapidjsonStringResult> {
 public:
  rapidjson::Document document;

  RapidjsonParseResult(std::string_view json) { (void)json; }

  bool contains_impl(std::string_view key) const {
    return document.HasMember(key.data());
  }

  bool stringfy_impl(RapidjsonStringResult &sr) const {
    rapidjson::Writer<rapidjson::StringBuffer> writer(sr.sb);
    document.Accept(writer);
    return true;
  }

  bool prettify_impl(RapidjsonStringResult &sr) const {
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(sr.sb);
    document.Accept(writer);
    return true;
  }

  bool stat_impl(DocStat &stat) const {
    GetStats(document, stat);
    return true;
  }

  bool find_impl(DocStat &stat) const {
    find_value(document, stat);
    return true;
  }

 private:
  void find_value(const rapidjson::Value &v, DocStat &stat) const {
    stat = DocStat();
    switch (v.GetType()) {
      case rapidjson::kObjectType:
        for (auto m = v.MemberBegin(); m != v.MemberEnd(); ++m) {
          auto re = v.FindMember(m->name);
          if (re != v.MemberEnd()) {
            stat.members++;
            find_value(re->value, stat);
          }
        }
        break;
      case rapidjson::kArrayType:
        for (size_t i = 0; i < v.Size(); ++i) {
          find_value(v[i], stat);
        }
        break;
      default:
        break;
    }
  }
  void GetStats(const rapidjson::Value &v, DocStat &stat,
                size_t depth = 0) const {
    stat = DocStat();
    switch (v.GetType()) {
      case rapidjson::kNullType:
        stat.nulls++;
        break;
      case rapidjson::kFalseType:
        stat.falses++;
        break;
      case rapidjson::kTrueType:
        stat.trues++;
        break;

      case rapidjson::kObjectType:
        if (depth > stat.depth) stat.depth = depth;
        for (rapidjson::Value::ConstMemberIterator m = v.MemberBegin();
             m != v.MemberEnd(); ++m) {
          stat.length += m->name.GetStringLength();
          GetStats(m->value, stat, depth + 1);
        }
        stat.objects++;
        stat.members += (v.MemberEnd() - v.MemberBegin());
        stat.strings += (v.MemberEnd() - v.MemberBegin());
        break;

      case rapidjson::kArrayType:
        if (depth > stat.depth) stat.depth = depth;
        for (rapidjson::Value::ConstValueIterator i = v.Begin(); i != v.End();
             ++i)
          GetStats(*i, stat, depth + 1);
        stat.arrays++;
        stat.elements += v.Size();
        break;

      case rapidjson::kStringType:
        stat.strings++;
        stat.length += v.GetStringLength();
        break;

      case rapidjson::kNumberType:
        stat.numbers++;
        break;
    }
  }
};

template <unsigned RapidjsonParseFlags>
class RapidjsonGeneric : public JsonBase<RapidjsonGeneric<RapidjsonParseFlags>,
                                         RapidjsonParseResult> {
 public:
  bool parse_impl(std::string_view json, RapidjsonParseResult &pr) const {
#if RAPIDJSON_MINOR_VERSION > 0
    pr.document.Parse<RapidjsonParseFlags>(json.data(), json.size());
#else
    pr.document.Parse<RapidjsonParseFlags>(json.data());
#endif
    if (pr.document.HasParseError()) {
      return false;
    }
    return true;
  }
};

using Rapidjson = RapidjsonGeneric<rapidjson::ParseFlag::kParseNoFlags>;
using RapidjsonFullPercision =
    RapidjsonGeneric<rapidjson::ParseFlag::kParseFullPrecisionFlag>;

#endif
