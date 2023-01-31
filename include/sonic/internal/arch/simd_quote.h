#pragma once

#include "simd_dispatch.h"

#ifdef SONIC_STATIC_DISPATCH
#include INCLUDE_ARCH_FILE(quote.h)
#endif

namespace sonic_json {
namespace internal {

SONIC_USING_ARCH_FUNC(parseStringInplace);
SONIC_USING_ARCH_FUNC(Quote);

}  // namespace internal
}  // namespace sonic_json
