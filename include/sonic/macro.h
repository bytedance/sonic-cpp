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

#include <cstdint>

#if defined(_MSC_VER)
#include <intrin.h>
#endif

#define SONICJSON_PADDING 64

#if defined(_MSC_VER)
#define sonic_likely(v) (v)
#define sonic_align(s) alignas(s)
#define sonic_unlikely(v) (v)
#define sonic_force_inline __forceinline
#define sonic_never_inline __declspec(noinline) inline
#else
#define sonic_likely(v) (__builtin_expect((v), 1))
#define sonic_align(s) alignas(s)
#define sonic_unlikely(v) (__builtin_expect((v), 0))
#define sonic_force_inline inline __attribute__((always_inline))
#define sonic_never_inline inline __attribute__((noinline))
#endif
#define sonic_static_noinline static sonic_never_inline
#define sonic_static_inline static sonic_force_inline

namespace sonic_json {
namespace internal {

static sonic_force_inline int CountTrailingZeros32(uint32_t value) {
  if (value == 0) return 32;
#if defined(_MSC_VER)
  unsigned long index;
  _BitScanForward(&index, static_cast<unsigned long>(value));
  return static_cast<int>(index);
#else
  return __builtin_ctz(value);
#endif
}

static sonic_force_inline int CountTrailingZeros64(uint64_t value) {
  if (value == 0) return 64;
#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_ARM64))
  unsigned long index;
  _BitScanForward64(&index, value);
  return static_cast<int>(index);
#elif defined(_MSC_VER)
  const uint32_t low = static_cast<uint32_t>(value);
  return low != 0
             ? CountTrailingZeros32(low)
             : 32 + CountTrailingZeros32(static_cast<uint32_t>(value >> 32));
#else
  return __builtin_ctzll(value);
#endif
}

static sonic_force_inline int CountLeadingZeros64(uint64_t value) {
  if (value == 0) return 64;
#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_ARM64))
  unsigned long index;
  _BitScanReverse64(&index, value);
  return 63 - static_cast<int>(index);
#elif defined(_MSC_VER)
  unsigned long index;
  const uint32_t high = static_cast<uint32_t>(value >> 32);
  if (high != 0) {
    _BitScanReverse(&index, static_cast<unsigned long>(high));
    return 31 - static_cast<int>(index);
  }
  _BitScanReverse(&index, static_cast<unsigned long>(value));
  return 63 - static_cast<int>(index);
#else
  return __builtin_clzll(value);
#endif
}

static sonic_force_inline long long CountOnes64(uint64_t value) {
#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_ARM64))
  return static_cast<long long>(__popcnt64(value));
#elif defined(_MSC_VER)
  return static_cast<long long>(__popcnt(static_cast<uint32_t>(value)) +
                                __popcnt(static_cast<uint32_t>(value >> 32)));
#else
  return static_cast<long long>(__builtin_popcountll(value));
#endif
}

static sonic_force_inline void CpuRelax() {
#if defined(_MSC_VER) && defined(_M_ARM64)
  __yield();
#elif defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
  _mm_pause();
#elif defined(__x86_64__) || defined(__i386__)
  __builtin_ia32_pause();
#elif defined(__aarch64__)
  asm volatile("yield");
#elif defined(__riscv)
  __asm__ volatile("pause");
#endif
}

}  // namespace internal
}  // namespace sonic_json

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
#define sonic_assert(x) assert((x))
#endif

#if (defined(_MSVC_LANG) && _MSVC_LANG >= 201703L) || \
    (!defined(_MSVC_LANG) && __cplusplus >= 201703L)
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
#elif defined(_MSC_VER)
// MSVC selects the target ISA for the translation unit (for example,
// /arch:AVX2) and does not support per-function target attributes.
#define SONIC_PUSH_TARGET(_target)
#define SONIC_POP_TARGET
#else
#error "Unsupported compiler"
#endif

#define SONIC_PUSH_WESTMERE SONIC_PUSH_TARGET(SONIC_WESTMERE)
#define SONIC_PUSH_HASWELL SONIC_PUSH_TARGET(SONIC_HASWELL)
