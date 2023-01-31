#pragma once

#include "simd_dispatch.h"

#ifdef SONIC_STATIC_DISPATCH
#include INCLUDE_ARCH_FILE(skip.h)
#endif

namespace sonic_json {
namespace internal {

SONIC_USING_ARCH_FUNC(EqBytes4);
SONIC_USING_ARCH_FUNC(SkipString);
SONIC_USING_ARCH_FUNC(SkipContainer);
SONIC_USING_ARCH_FUNC(SkipArray);
SONIC_USING_ARCH_FUNC(SkipObject);
SONIC_USING_ARCH_FUNC(SkipLiteral);
SONIC_USING_ARCH_FUNC(SkipNumber);
SONIC_USING_ARCH_FUNC(SkipScanner);

template <typename JPStringType>
ParseResult GetOnDemand(StringView json,
                        const GenericJsonPointer<JPStringType> &path,
                        StringView &target) {
  SkipScanner scan;
  size_t pos = 0;
  long start = scan.GetOnDemand(json, pos, path);
  if (start < 0) {
    target = "";
    return ParseResult(SonicError(-start), pos - 1);
  }
  target = StringView(json.data() + start, pos - start);
  return ParseResult(kErrorNone, pos);
}

}  // namespace internal
}  // namespace sonic_json
