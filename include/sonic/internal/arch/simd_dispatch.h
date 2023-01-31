#pragma once
#include "sonic_cpu_feature.h"

#ifndef SONIC_DYNAMIC_DISPATCH
#define SONIC_STATIC_DISPATCH
#endif

#ifndef SONIC_STRINGIFY
#define SONIC_STRINGIFY(s) SONIC_STRINGIFY2(s)
#define SONIC_STRINGIFY2(s) #s
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

#if defined(SONIC_HAVE_SVE)
#define SONIC_USING_ARCH_FUNC(func) using sve::func
#define INCLUDE_ARCH_FILE(file) SONIC_STRINGIFY(sve/file)
#elif defined(SONIC_HAVE_NEON)
#define SONIC_USING_ARCH_FUNC(func) using neon::func
#define INCLUDE_ARCH_FILE(file) SONIC_STRINGIFY(neon/file)
#endif
// clang-format on

#endif
