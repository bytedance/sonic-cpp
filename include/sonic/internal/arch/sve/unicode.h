#pragma once

#include <sonic/macro.h>

#include <cstdint>
#include <cstring>

#include "../common/unicode_common.h"
#include "../neon/unicode.h"
#include "base.h"

namespace sonic_json {
namespace internal {
namespace sve {

using neon::handle_unicode_codepoint;

using neon::StringBlock;

using neon::GetNonSpaceBits;

}  // namespace sve
}  // namespace internal
}  // namespace sonic_json
