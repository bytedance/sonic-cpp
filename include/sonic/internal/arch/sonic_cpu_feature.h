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
