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

#include <benchmark/benchmark.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string_view>

#include "cjson.hpp"
#include "ondemand.hpp"
#include "rapidjson.hpp"
#include "simdjson.hpp"
#include "sonic.hpp"
#include "yyjson.hpp"

static std::string get_json(const std::string_view file) {
  std::ifstream ifs;
  std::stringstream ss;
  ifs.open(file.data());
  ss << ifs.rdbuf();
  return ss.str();
}

template <typename Json, typename PR, typename SR>
static void BM_Encode(benchmark::State &state, std::string_view filename,
                      std::string_view data) {
  Json json;
  std::unique_ptr<const PR> pr = json.parse(data);

  for (auto _ : state) {
    if (!pr || !pr->stringfy()) state.SkipWithError("Failed to do stringfy");
  }

  state.SetLabel(filename.data());
  state.SetBytesProcessed(int64_t(state.iterations()) * int64_t(data.size()));
}

template <typename Json, typename PR, typename SR>
static void BM_Stat(benchmark::State &state, std::string filename,
                    std::string_view data) {
  Json json;
  std::unique_ptr<const PR> pr;
  pr = json.parse(data);

  if (!pr) {
    state.SkipWithError("Failed to parse file");
    return;
  }

  auto sr = pr->stringfy();
  if (!sr) {
    state.SkipWithError("Failed to convert to string");
    return;
  }

  auto new_pr = json.parse(sr->str());
  if (!new_pr) {
    state.SkipWithError("Failed to parse string");
    return;
  }

  DocStat stat1, stat2;

  bool stat_re;
  for (auto _ : state) stat_re = pr->stat(stat1);

  if (!stat_re || !new_pr->stat(stat2)) {
    state.SkipWithError("Failed to get statistic data");
    return;
  }

  if (stat1 != stat2) {
    state.SkipWithError("Stat Verify failed");
  }

  state.SetLabel(filename.data());
  state.SetBytesProcessed(int64_t(state.iterations()) * int64_t(data.size()));
}

template <typename Json, typename PR, typename SR>
static void BM_Find(benchmark::State &state, std::string filename,
                    std::string_view data) {
  Json json;
  std::unique_ptr<const PR> pr;
  pr = json.parse(data);

  if (!pr) {
    state.SkipWithError("Failed to parse file");
    return;
  }

  auto sr = pr->stringfy();
  if (!sr) {
    state.SkipWithError("Failed to convert to string");
    return;
  }

  auto new_pr = json.parse(sr->str());
  if (!new_pr) {
    state.SkipWithError("Failed to parse string");
    return;
  }

  DocStat stat1, stat2;
  if (!pr->stat(stat1) || !new_pr->stat(stat2)) {
    state.SkipWithError("Failed to get statistic data");
    return;
  }

  if (stat1 != stat2) {
    state.SkipWithError("Stat Verify failed");
  }

  DocStat stat3;
  for (auto _ : state) pr->find(stat3);

  state.SetLabel(filename.data());
  state.SetBytesProcessed(int64_t(state.iterations()) * int64_t(data.size()));
}

template <typename Json, typename PR, typename SR>
static void BM_Decode(benchmark::State &state, std::string filename,
                      std::string_view data) {
  Json json;
  std::unique_ptr<const PR> pr;
  for (auto _ : state) pr = json.parse(data);

  if (!pr) {
    state.SkipWithError("Failed to parse file");
    return;
  }

  auto sr = pr->stringfy();
  if (!sr) {
    state.SkipWithError("Failed to convert to string");
    return;
  }

  auto new_pr = json.parse(sr->str());
  if (!new_pr) {
    state.SkipWithError("Failed to parse string");
    return;
  }

  DocStat stat1, stat2;
  if (!pr->stat(stat1) || !new_pr->stat(stat2)) {
    state.SkipWithError("Failed to get statistic data");
    return;
  }

  if (stat1 != stat2) {
    state.SkipWithError("Stat Verify failed");
  }

  state.SetLabel(filename.data());
  state.SetBytesProcessed(int64_t(state.iterations()) * int64_t(data.size()));
}

static void regitser_OnDemand() {
  std::vector<OnDemand> tests = {
      {"twitter", "Normal", {"search_metadata", "count"}, 100, true},
      {"citm_catalog",
       "Fronter",
       {"events", "342742596", "id"},
       342742596,
       true},
      {"twitter", "NotFound", {"NotFound"}},
  };

  for (auto &t : tests) {
    auto file_path = std::string("testdata/") + t.file + ".json";
    t.json = get_json(file_path);

#define REG_ONDEMAND(JSON)                                                   \
  {                                                                          \
    auto name =                                                              \
        std::string(t.file) + ("/" #JSON "OnDemand") + "_" + t.name.c_str(); \
    benchmark::RegisterBenchmark(name.c_str(), BM_##JSON##OnDemand, t);      \
  }
    REG_ONDEMAND(Sonic);
    REG_ONDEMAND(RapidjsonSax);
    REG_ONDEMAND(SIMDjson);
  }
}

int main(int argc, char **argv) {
  benchmark::Initialize(&argc, argv);

  // Read the data from json files
  std::vector<std::pair<std::filesystem::path, std::string>> jsons;
  for (const auto &entry : std::filesystem::directory_iterator("testdata"))
    if (entry.path().extension() == ".json")
      jsons.push_back(
          std::make_pair(entry.path(), get_json(entry.path().string())));

  regitser_OnDemand();
#define ADD_JSON_BMK(JSON, ACT)                                      \
  do {                                                               \
    benchmark::RegisterBenchmark(                                    \
        (json.first.stem().string() + ("/" #ACT "_" #JSON)).c_str(), \
        BM_##ACT<JSON, JSON##ParseResult, JSON##StringResult>,       \
        json.first.string(), json.second);                           \
  } while (0)

#define ADD_BMK(METHOD)                \
  do {                                 \
    for (const auto &json : jsons) {   \
      ADD_JSON_BMK(SonicDyn, METHOD);  \
      ADD_JSON_BMK(Rapidjson, METHOD); \
      ADD_JSON_BMK(YYjson, METHOD);    \
      ADD_JSON_BMK(SIMDjson, METHOD);  \
    }                                  \
  } while (0)

  ADD_BMK(Decode);
  ADD_BMK(Encode);
  // ADD_BMK(Stat);
  // ADD_BMK(Find);
  do {
    for (const auto &json : jsons) {
      ADD_JSON_BMK(SonicDyn, Stat);
      ADD_JSON_BMK(SonicDyn, Find);
      ADD_JSON_BMK(Rapidjson, Stat);
      ADD_JSON_BMK(Rapidjson, Find);
    }
  } while (0);

  benchmark::RunSpecifiedBenchmarks();
  benchmark::Shutdown();
  return 0;
}
