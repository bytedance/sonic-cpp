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

#ifndef _ONDEMAND_H_
#define _ONDEMAND_H_

#include <benchmark/benchmark.h>
#include <simdjson.h>
#include <sonic/sonic.h>

#include <string>
#include <string_view>
#include <vector>

#include "rapidjson_sax.hpp"

struct OnDemand {
  std::string file;
  std::string name;
  std::vector<std::string_view> path;
  uint64_t value = {0};
  bool existed = {false};
  std::string json;
};

static void BM_SonicOnDemand(benchmark::State& state, const OnDemand& data) {
  sonic_json::Document lite;
  sonic_json::GenericJsonPointer<std::string_view> path(data.path);
  lite.ParseOnDemand(data.json.data(), data.json.size(), path);
  bool existed = !lite.HasParseError();
  bool ok = existed == data.existed && lite.GetUint64() == data.value;
  if (!ok) {
    state.SkipWithError("Verify failed");
    return;
  }

  uint64_t get;
  for (auto _ : state) {
    sonic_json::Document lite;
    lite.ParseOnDemand(data.json.data(), data.json.size(), path);
    get = lite.GetUint64();
  }

  state.SetLabel(data.name);
  state.SetBytesProcessed(int64_t(state.iterations()) *
                          int64_t(data.json.size()));
}

static void BM_SIMDjsonOnDemand(benchmark::State& state, const OnDemand& data) {
  using namespace simdjson;
  ondemand::parser parser;
  uint64_t u = 0;
  error_code err = SUCCESS;

  auto json_pad = padded_string(data.json);
  auto dom = parser.iterate(json_pad);
  auto value = dom.get_value();
  for (const auto key : data.path) {
    value = value[key];
  }
  err = value.get(u);
  bool ok = (!err) == data.existed && u == data.value;
  if (!ok) {
    state.SkipWithError("Verify failed");
    return;
  }

  for (auto _ : state) {
    auto value = parser.iterate(json_pad).get_value();
    for (const auto key : data.path) {
      value = value[key];
    }
    err = value.get(u);
  }
  state.SetLabel(data.name);
  state.SetBytesProcessed(int64_t(state.iterations()) *
                          int64_t(data.json.size()));
}

static void BM_RapidjsonSaxOnDemand(benchmark::State& state,
                                    const OnDemand& data) {
  uint64_t get = 0;
  bool existed = RapidjsonSaxOnDemand(data.json, data.path, get);
  bool ok = existed == data.existed && get == data.value;
  if (!ok) {
    state.SkipWithError("Verify failed");
    return;
  }
  for (auto _ : state) {
    RapidjsonSaxOnDemand(data.json, data.path, get);
  }
  state.SetLabel(data.name);
  state.SetBytesProcessed(int64_t(state.iterations()) *
                          int64_t(data.json.size()));
}
#endif