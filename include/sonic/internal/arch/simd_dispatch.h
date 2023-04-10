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
#include "../../macro.h"
#include "sonic_cpu_feature.h"

#ifndef SONIC_DYNAMIC_DISPATCH
#define SONIC_STATIC_DISPATCH
#endif

#if defined(SONIC_STATIC_DISPATCH)

// clang-format off
#if defined(SONIC_HAVE_AVX2)
#define SONIC_USING_ARCH_FUNC(func) using avx2::func
#define INCLUDE_ARCH_FILE(file) SONIC_STRINGIFY(avx2/file)
#elif defined(SONIC_HAVE_SSE)
#define SONIC_USING_ARCH_FUNC(func) using sse::func
#define INCLUDE_ARCH_FILE(file) SONIC_STRINGIFY(sse/file)
#endif

#if defined(SONIC_HAVE_NEON)
#define SONIC_USING_ARCH_FUNC(func) using neon::func
#define INCLUDE_ARCH_FILE(file) SONIC_STRINGIFY(neon/file)
#endif

#elif defined(SONIC_DYNAMIC_DISPATCH)

#if defined(__x86_64__)
#define SONIC_USING_ARCH_FUNC(func)
#define INCLUDE_ARCH_FILE(file) SONIC_STRINGIFY(x86_ifuncs/file)
#elif defined(SONIC_HAVE_NEON)
#define SONIC_USING_ARCH_FUNC(func) using neon::func
#define INCLUDE_ARCH_FILE(file) SONIC_STRINGIFY(neon/file)
#endif

#endif
// clang-format on
