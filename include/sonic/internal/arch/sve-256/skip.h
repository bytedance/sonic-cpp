#pragma once

#include <sonic/internal/arch/common/skip_common.h>
#include <sonic/internal/utils.h>
#include "../neon/skip.h"
#include "base.h"

#define VEC_LEN 16
#define VEC_LEN_SVE svcntb()

namespace sonic_json {
namespace internal {
namespace sve_256 {

using sonic_json::internal::common::EqBytes4;
using sonic_json::internal::common::SkipLiteral;

using neon::to_bitmask;
#include "../common/arm_common/skip.inc.h"

using neon::SkipContainer;

sonic_force_inline uint8_t skip_space(const uint8_t *data, size_t &pos, size_t &, uint64_t &) {
    // fast path for single space
    if (!IsSpace(data[pos++]))
        return data[pos - 1];
    if (!IsSpace(data[pos++]))
        return data[pos - 1];

    svbool_t ptrue = svptrue_b8();
    // current pos is out of block
    while (1) {
        svuint8_t v = svld1_u8(ptrue, reinterpret_cast<const uint8_t *>(data + pos));
        svbool_t m1 = svcmpeq_n_u8(ptrue, v, '\r');
        svbool_t m2 = svcmpeq_n_u8(ptrue, v, '\n');
        svbool_t m3 = svcmpeq_n_u8(ptrue, v, '\t');
        svbool_t m4 = svcmpeq_n_u8(ptrue, v, ' ');
        svbool_t m5 = svorr_b_z(ptrue, m1, m2);
        svbool_t m6 = svorr_b_z(ptrue, m3, m4);
        svbool_t mask = svnor_b_z(ptrue, m5, m6);
        if (svptest_any(ptrue, mask)) {
            pos += get_first_active_index(mask);
            return data[pos++];
        } else {
            pos += VEC_LEN_SVE;
        }
    }

    sonic_assert(false && "!should not happen");
}

}  // namespace sve_256
}  // namespace internal
}  // namespace sonic_json

#undef VEC_LEN
