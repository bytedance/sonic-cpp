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

#if __cplusplus >= 201703L
#define SONIC_IF_CONSTEXPR constexpr
#else
#define SONIC_IF_CONSTEXPR
#endif

#ifndef SONIC_STRINGIFY
#define SONIC_STRINGIFY(s) SONIC_STRINGIFY2(s)
#define SONIC_STRINGIFY2(s) #s
#endif

#define SONIC_WESTMERE "pclmul,sse4.2"
#define SONIC_HASWELL "avx2"
#define SONIC_WESTMERE_STR(s) "arch=westmere"
#define SONIC_HASWELL_STR(s) "arch=haswell"

#if defined(__clang__)
#define SONIC_PUSH_TARGET(_target)              \
  _Pragma(SONIC_STRINGIFY(clang attribute push( \
      __attribute__((target(_target))), apply_to = function)))
#define SONIC_POP_TARGET _Pragma("clang attribute pop")
#elif defined(__GNUG__)
#define SONIC_PUSH_TARGET(_target) \
  _Pragma("GCC push_options") _Pragma(SONIC_STRINGIFY(GCC target(_target)))
#define SONIC_POP_TARGET _Pragma("GCC pop_options")
#endif

#define SONIC_PUSH_WESTMERE SONIC_PUSH_TARGET(SONIC_WESTMERE)
#define SONIC_PUSH_HASWELL SONIC_PUSH_TARGET(SONIC_HASWELL)

#ifdef SONIC_SPARK_FORMAT
#define SONIC_EXPONENT_ALWAYS_DOT 1
#define SONIC_EXPONENT_UPPERCASE 1
#define SONIC_EXPONENT_ALWAYS_SIGN 0
#define SONIC_UES_EXPONENT 1
#endif

#ifdef SONIC_DEFAULT_FORMAT
#define SONIC_EXPONENT_ALWAYS_DOT 0
#define SONIC_EXPONENT_UPPERCASE 0
#define SONIC_EXPONENT_ALWAYS_SIGN 1
#endif

/* Macros used to customed JSON format */
#ifndef SONIC_EXPONENT_ALWAYS_DOT
// print exponent with dot, like 1.0e+2
#define SONIC_EXPONENT_ALWAYS_DOT 0
#endif

#ifndef SONIC_EXPONENT_UPPERCASE
// print with uppercase `E`, like 1.0E+2
#define SONIC_EXPONENT_UPPERCASE 0
#endif

#ifndef SONIC_EXPONENT_ALWAYS_SIGN
// print exponent with sign, including '+', like 1.0E+2
#define SONIC_EXPONENT_ALWAYS_SIGN 1
#endif

#ifndef SONIC_UES_EXPONENT
// use exponent format for float point number
#define SONIC_UES_EXPONENT 0
#endif
