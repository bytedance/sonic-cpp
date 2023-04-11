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

#if defined(__SSE2__)
#define SONIC_HAVE_SSE
#define SONIC_HAVE_SSE2
#if defined(__SSE3__)
#define SONIC_HAVE_SSE3
#endif
#if defined(__SSSE3__)
#define SONIC_HAVE_SSSE3
#endif
#if defined(__SSE4_1__)
#define SONIC_HAVE_SSE4_1
#endif
#if defined(__SSE4_2__)
#define SONIC_HAVE_SSE4_2
#endif
#if defined(__AVX__)
#define SONIC_HAVE_AVX
#endif
#if defined(__AVX2__)
#define SONIC_HAVE_AVX2
#endif
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
#define SONIC_HAVE_NEON
#elif defined(__ARM_FEATURE_SVE)
#define SONIC_HAVE_NEON
#endif
