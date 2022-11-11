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
#include <sonic/sonic.h>

#include <string>
#include <string_view>
#include <vector>


struct OnDemand {
  std::string file;
  std::string name;
  std::vector<std::string> path;
  uint64_t value = {~0ull};
  std::string json = {};
};

static void BM_SonicOnDemand(benchmark::State& state, const OnDemand& data) {
  sonic_json::Document lite;
  sonic_json::JsonPointer path(data.path);
  lite.ParseOnDemand(data.json.data(), data.json.size(), path);
  bool found = !lite.HasParseError();
  bool ok = (found && lite.GetUint64() == data.value) ||
            (!found && data.value == ~0ull);
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

#endif