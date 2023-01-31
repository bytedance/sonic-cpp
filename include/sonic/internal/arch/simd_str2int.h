#pragma once

#include "simd_dispatch.h"

#ifdef SONIC_STATIC_DISPATCH
#include INCLUDE_ARCH_FILE(str2int.h)
#endif

namespace sonic_json {
namespace internal {

SONIC_USING_ARCH_FUNC(simd_str2int);

}  // namespace internal
}  // namespace sonic_json
