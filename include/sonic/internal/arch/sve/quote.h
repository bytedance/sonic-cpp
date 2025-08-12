#pragma once

#define VEC_LEN 16

#include "../common/arm_common/quote.h"
#include "unicode.h"

#ifndef PAGE_SIZE
#define PAGE_SIZE 4096
#endif

#ifdef __GNUC__
#if defined(__SANITIZE_THREAD__) || defined(__SANITIZE_ADDRESS__) || defined(__SANITIZE_LEAK__) || \
    defined(__SANITIZE_UNDEFINED__)
#ifndef SONIC_USE_SANITIZE
#define SONIC_USE_SANITIZE
#endif
#endif
#endif

#if defined(__clang__)
#if defined(__has_feature)
#if __has_feature(address_sanitizer) || __has_feature(thread_sanitizer) || __has_feature(memory_sanitizer) || \
    __has_feature(undefined_behavior_sanitizer) || __has_feature(leak_sanitizer)
#ifndef SONIC_USE_SANITIZE
#define SONIC_USE_SANITIZE
#endif
#endif
#endif
#endif

#ifndef VEC_LEN
#error "You should define VEC_LEN before including quote.h!"
#endif

#define MOVE_N_CHARS(src, N) \
    do {                     \
        (src) += (N);        \
        nb -= (N);           \
        dst += (N);          \
    } while (0)

namespace sonic_json {
namespace internal {
namespace sve {

sonic_force_inline svbool_t copy_get_escaped_mask_predicate(svbool_t pg, const char *src, char *dst)
{
    svuint8_t v = svld1_u8(pg, reinterpret_cast<const uint8_t *>(src));
    svst1_u8(pg, reinterpret_cast<uint8_t *>(dst), v);
    svbool_t m1 = svcmpeq_n_u8(pg, v, '\\');
    svbool_t m2 = svcmpeq_n_u8(pg, v, '"');
    svbool_t m3 = svcmplt_n_u8(pg, v, '\x20');
    svbool_t m4 = svorr_b_z(pg, m1, m2);
    svbool_t m5 = svorr_b_z(pg, m3, m4);
    return m5;
}

// The function returns the index of first (to the rigth) active elem
sonic_force_inline int get_first_active_index(svbool_t input)
{
    return svlastb_u8(svbrka_b_z(input, input), svindex_u8(0, 1));
}


sonic_force_inline size_t parseStringInplace(uint8_t *&src, SonicError &err) {
#define SONIC_REPEAT8(v) {v v v v v v v v}

  uint8_t *dst = src;
  uint8_t *sdst = src;
  while (1) {
  find:
    auto block = StringBlock::Find(src);
    if (block.HasQuoteFirst()) {
      int idx = block.QuoteIndex();
      src += idx;
      *src++ = '\0';
      return src - sdst - 1;
    }
    if (block.HasUnescaped()) {
      err = kParseErrorUnEscaped;
      return 0;
    }
    if (!block.HasBackslash()) {
      src += VEC_LEN;
      goto find;
    }

    /* find out where the backspace is */
    auto bs_dist = block.BsIndex();
    src += bs_dist;
    dst = src;
  cont:
    uint8_t escape_char = src[1];
    if (sonic_unlikely(escape_char == 'u')) {
      if (!handle_unicode_codepoint(const_cast<const uint8_t **>(&src), &dst)) {
        err = kParseErrorEscapedUnicode;
        return 0;
      }
    } else {
      *dst = kEscapedMap[escape_char];
      if (sonic_unlikely(*dst == 0u)) {
        err = kParseErrorEscapedFormat;
        return 0;
      }
      src += 2;
      dst += 1;
    }
    // fast path for continous escaped chars
    if (*src == '\\') {
      bs_dist = 0;
      goto cont;
    }

  find_and_move:
    // Copy the next n bytes, and find the backslash and quote in them.
    uint8x16_t v = vld1q_u8(src);
    block = StringBlock::Find(v);
    // If the next thing is the end quote, copy and return
    if (block.HasQuoteFirst()) {
      // we encountered quotes first. Move dst to point to quotes and exit
      while (1) {
        SONIC_REPEAT8(if (sonic_unlikely(*src == '"')) break;
                      else { *dst++ = *src++; });
      }
      *dst = '\0';
      src++;
      return dst - sdst;
    }
    if (block.HasUnescaped()) {
      err = kParseErrorUnEscaped;
      return 0;
    }
    if (!block.HasBackslash()) {
      /* they are the same. Since they can't co-occur, it means we
       * encountered neither. */
      vst1q_u8(dst, v);
      src += VEC_LEN;
      dst += VEC_LEN;
      goto find_and_move;
    }
    while (1) {
      SONIC_REPEAT8(if (sonic_unlikely(*src == '\\')) break;
                    else { *dst++ = *src++; });
    }
    goto cont;
  }
  sonic_assert(false);
#undef SONIC_REPEAT8
}

sonic_force_inline char *Quote(const char *src, size_t nb, char *dst)
{
    *dst++ = '"';
    sonic_assert(nb < (1LL << 0x20));
    auto svelen = svcntb();
    svbool_t ptrue = svptrue_b8();
    while (nb > svelen) {
        svbool_t mask = copy_get_escaped_mask_predicate(ptrue, src, dst);
        if (svptest_any(ptrue, mask)) {
            auto cn = get_first_active_index(mask);
            MOVE_N_CHARS(src, cn);
            DoEscape(src, dst, nb);
        } else {
            MOVE_N_CHARS(src, svelen);
        }
    }
    while (nb > 0) {
        svbool_t predicate = svwhilelt_b8_u64(0, nb);
        svbool_t mask = copy_get_escaped_mask_predicate(predicate, src, dst);
        if (svptest_any(predicate, mask)) {
            auto cn = get_first_active_index(mask);
            MOVE_N_CHARS(src, cn);
            DoEscape(src, dst, nb);
        } else {
            auto active_elems = svcntp_b8(predicate, predicate);
            MOVE_N_CHARS(src, active_elems);
        }
    }
    *dst++ = '"';
    return dst;
}
} // namespace sve
} // namespace internal
} // namespace sonic_json

#undef VEC_LEN
