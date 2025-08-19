#pragma once

#include "../common/arm_common/base.h"
#include <arm_sve.h>

namespace sonic_json {
namespace internal {
namespace sve_256 {

using sonic_json::internal::arm_common::ClearLowestBit;
using sonic_json::internal::arm_common::CountOnes;
using sonic_json::internal::arm_common::InlinedMemcmp;
using sonic_json::internal::arm_common::LeadingZeroes;
using sonic_json::internal::arm_common::PrefixXor;
using sonic_json::internal::arm_common::TrailingZeroes;

static inline bool is_eq_lt_32(const void* a, const void* b, size_t s) {
  auto lhs = static_cast<const uint8_t*>(a);
  auto rhs = static_cast<const uint8_t*>(b);
  svbool_t pg = svwhilelt_b8((size_t)0, s);
  svbool_t ptrue = svptrue_b8();
  svuint8_t va = svld1(pg, lhs);
  svuint8_t vb = svld1(pg, rhs);
  svbool_t neq_mask = svcmpne(ptrue, va, vb);
  return svptest_any(pg, neq_mask) == 0;
}

sonic_force_inline bool InlinedMemcmpEq(const void* _a, const void* _b, size_t s) {
  const uint8_t* a = static_cast<const uint8_t*>(_a);
  const uint8_t* b = static_cast<const uint8_t*>(_b);

  if (s == 0) return true;
  if (s < 32) return is_eq_lt_32(a, b, s);

  svbool_t ptrue = svptrue_b8();
  svbool_t pg_head = svwhilelt_b8(0, 32);
  svuint8_t head_a = svld1(pg_head, a);
  svuint8_t head_b = svld1(pg_head, b);

  svbool_t cmp_head = svcmpne(pg_head, head_a, head_b);
  if (svptest_any(pg_head, cmp_head)) {
    return false;
  }

  if (s > 32) {
    size_t tail_offset = s - 32;
    svuint8_t tail_a = svld1(pg_head, a + tail_offset);
    svuint8_t tail_b = svld1(pg_head, b + tail_offset);
    svbool_t cmp_tail = svcmpne(pg_head, tail_a, tail_b);
    if (svptest_any(pg_head, cmp_tail)) {
      return false;
    }
  }

  if (s > 64) {
    for (size_t offset = 32; offset < s - 32; offset += 32) {
      svuint8_t va = svld1(ptrue, a + offset);
      svuint8_t vb = svld1(ptrue, b + offset);
      svbool_t neq_mask = svcmpne(ptrue, va, vb);
      if (svptest_any(ptrue, neq_mask)) {
        return false;
      }
    }
  }
  return true;
}

template <size_t ChunkSize>
sonic_force_inline void Xmemcpy(void* dst_, const void* src_, size_t chunks) {
  std::memcpy(dst_, src_, chunks * ChunkSize);
}

template <>
sonic_force_inline void Xmemcpy<32>(void* dst_, const void* src_, size_t chunks) {
  uint8_t* dst = reinterpret_cast<uint8_t*>(dst_);
  const uint8_t* src = reinterpret_cast<const uint8_t*>(src_);
  svbool_t pg = svptrue_b8();
  size_t blocks = chunks / 4;
  for (size_t i = 0; i < blocks; i++) {
    for (size_t j = 0; j < 4; j++) {
      svuint8_t vsrc = svld1_u8(pg, src);
      svst1_u8(pg, dst, vsrc);
      src += 32;
      dst += 32;
    }
  }

  switch (chunks & 3) {
    case 3: {
      svuint8_t vsrc = svld1_u8(pg, src);
      svst1_u8(pg, dst, vsrc);
      src += 32;
      dst += 32;
    }
    /* fall through */
    case 2: {
      svuint8_t vsrc = svld1_u8(pg, src);
      svst1_u8(pg, dst, vsrc);
      src += 32;
      dst += 32;
    }
    /* fall through */
    case 1: {
      svuint8_t vsrc = svld1_u8(pg, src);
      svst1_u8(pg, dst, vsrc);
    }
  }
}

template <>
sonic_force_inline void Xmemcpy<16>(void* dst_, const void* src_, size_t chunks) {
  uint8_t* dst = reinterpret_cast<uint8_t*>(dst_);
  const uint8_t* src = reinterpret_cast<const uint8_t*>(src_);
  svbool_t pg = svptrue_b8();
  size_t blocks = chunks / 8;
  for (size_t i = 0; i < blocks; i++) {
    for (size_t j = 0; j < 4; j++) {
      svuint8_t vsrc = svld1_u8(pg, src);
      svst1_u8(pg, dst, vsrc);
      src += 32;
      dst += 32;
    }
  }

  switch ((chunks / 2) & 3) {
    case 3: {
      svuint8_t vsrc = svld1_u8(pg, src);
      svst1_u8(pg, dst, vsrc);
      src += 32;
      dst += 32;
    }
    /* fall through */
    case 2: {
      svuint8_t vsrc = svld1_u8(pg, src);
      svst1_u8(pg, dst, vsrc);
      src += 32;
      dst += 32;
    }
    /* fall through */
    case 1: {
      svuint8_t vsrc = svld1_u8(pg, src);
      svst1_u8(pg, dst, vsrc);
      src += 32;
      dst += 32;
    }
  }

  if (chunks & 1) {
    svbool_t pg = svwhilelt_b8(0, 16);
    svuint8_t vsrc = svld1_u8(pg, src);
    svst1_u8(pg, dst, vsrc);
  }
}

}  // namespace sve_256
}  // namespace internal
}  // namespace sonic_json
