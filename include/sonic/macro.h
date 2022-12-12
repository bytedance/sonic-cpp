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

#define SONICJSON_PADDING 64

#define sonic_likely(v) (__builtin_expect((v), 1))
#define sonic_align(s) __attribute__((aligned(s)))
#define sonic_unlikely(v) (__builtin_expect((v), 0))
#define sonic_force_inline inline __attribute__((always_inline))
#define sonic_never_inline inline __attribute__((noinline))
#define sonic_static_noinline static sonic_never_inline
#define sonic_static_inline static sonic_force_inline

#ifdef SONIC_DEBUG
#include <cassert>
#include <cstdlib>
#include <iostream>
#define sonic_specific_assert(expr)                                      \
  do {                                                                   \
    if (!(expr)) {                                                       \
      std::cerr << __FILE__ << ":" << __LINE__ << ":" << __FUNCTION__    \
                << ": Assertion `" << #expr << "` failed!" << std::endl; \
      std::abort();                                                      \
    }                                                                    \
  } while (0);
#define sonic_assert(x) sonic_specific_assert(x)
#elif defined(NDEBUG)
#define sonic_assert(x) (void)(x)
#else
#include <cassert>
#define sonic_assert(x) assert((x));
#endif
