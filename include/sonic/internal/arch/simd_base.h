#pragma once

#include "simd_dispatch.h"

#ifdef SONIC_STATIC_DISPATCH
#include INCLUDE_ARCH_FILE(base.h)
#endif

namespace sonic_json {
namespace internal {

SONIC_USING_ARCH_FUNC(TrailingZeroes);
SONIC_USING_ARCH_FUNC(ClearLowestBit);
SONIC_USING_ARCH_FUNC(LeadingZeroes);
SONIC_USING_ARCH_FUNC(CountOnes);
SONIC_USING_ARCH_FUNC(AddOverflow);
SONIC_USING_ARCH_FUNC(PrefixXor);
SONIC_USING_ARCH_FUNC(Xmemcpy);

}  // namespace internal
}  // namespace sonic_json
