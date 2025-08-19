#pragma once

#include <sonic/macro.h>

#include <cstdint>
#include <cstring>

#include "../common/unicode_common.h"
#include "../neon/unicode.h"
#include "base.h"

namespace sonic_json {
namespace internal {
namespace sve_256 {

using neon::handle_unicode_codepoint;

using neon::StringBlock;

using neon::GetNonSpaceBits;

}  // namespace sve_256
}  // namespace internal
}  // namespace sonic_json
