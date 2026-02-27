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

#pragma once

#include <sonic/macro.h>

#include "../avx2/quote.h"
#include "../sse/quote.h"

namespace sonic_json {
namespace internal {

inline bool CpuSupportsHaswell() {
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__)
  return __builtin_cpu_supports("avx2");
#else
  return false;
#endif
}

inline bool CpuSupportsWestmere() {
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__)
  return __builtin_cpu_supports("sse4.2") && __builtin_cpu_supports("pclmul");
#else
  return false;
#endif
}

template <ParseFlags parseFlags = ParseFlags::kParseDefault>
struct ParseStringDispatcher {
  using FuncType = size_t (*)(uint8_t *&, SonicError &);

  static size_t DefaultImpl(uint8_t *& /*src*/, SonicError & /*err*/) {
    // TODO static_assert(!!!"Not Implemented!");
    return 0;
  }

  static FuncType Resolve() {
    if (CpuSupportsHaswell()) {
      return avx2::parseStringInplace<parseFlags>;
    }
    if (CpuSupportsWestmere()) {
      return sse::parseStringInplace<parseFlags>;
    }
    return DefaultImpl;
  }

  static FuncType func;
};

template <ParseFlags parseFlags>
typename ParseStringDispatcher<parseFlags>::FuncType
    ParseStringDispatcher<parseFlags>::func =
        ParseStringDispatcher<parseFlags>::Resolve();

template <SerializeFlags serializeFlags>
struct QuoteDispatcher {
  using FuncType = char *(*)(const char *src, size_t nb, char *dst);

  static char *DefaultImpl(const char * /*src*/, size_t /*nb*/,
                           char * /*dst*/) {
    // TODO static_assert(!!!"Not Implemented!");
    return 0;
  }

  static FuncType Resolve() {
    if (CpuSupportsHaswell()) {
      return avx2::Quote<serializeFlags>;
    }
    if (CpuSupportsWestmere()) {
      return sse::Quote<serializeFlags>;
    }
    return DefaultImpl;
  }

  static FuncType func;
};

template <SerializeFlags serializeFlags>
typename QuoteDispatcher<serializeFlags>::FuncType
    QuoteDispatcher<serializeFlags>::func =
        QuoteDispatcher<serializeFlags>::Resolve();

template <ParseFlags parseFlags = ParseFlags::kParseDefault>
inline size_t parseStringInplace(uint8_t *&src, SonicError &err) {
  return ParseStringDispatcher<parseFlags>::func(src, err);
}

template <SerializeFlags serializeFlags = SerializeFlags::kSerializeDefault>
inline char *Quote(const char *src, size_t nb, char *dst) {
  return QuoteDispatcher<serializeFlags>::func(src, nb, dst);
}

}  // namespace internal
}  // namespace sonic_json
