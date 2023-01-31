#pragma once

#include "simd_dispatch.h"

#ifdef SONIC_STATIC_DISPATCH
#include INCLUDE_ARCH_FILE(itoa.h)
#endif

namespace sonic_json {
namespace internal {

SONIC_USING_ARCH_FUNC(Utoa_8);
SONIC_USING_ARCH_FUNC(Utoa_16);

}  // namespace internal
}  // namespace sonic_json
