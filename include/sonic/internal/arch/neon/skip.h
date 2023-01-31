/*
 * Copyright 2022 ByteDance Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include "../common/skip_common.h"
#include "base.h"
#include "quote.h"
#include "simd.h"
#include "sonic/dom/json_pointer.h"
#include "sonic/error.h"
#include "sonic/internal/utils.h"
#include "sonic/macro.h"
#include "unicode.h"

namespace sonic_json {
namespace internal {
namespace neon {

using sonic_json::internal::common::EqBytes4;
using sonic_json::internal::common::SkipLiteral;

#ifndef VEC_LEN
#error "Define vector length firstly!"
#endif

// GetNextToken find the next characters in tokens and update the position to
// it.
template <size_t N>
sonic_force_inline uint8_t GetNextToken(const uint8_t *data, size_t &pos,
                                        size_t len, const char (&tokens)[N]) {
  while (pos + VEC_LEN <= len) {
    uint8x16_t v = vld1q_u8(data + pos);
    // simd256<uint8_t> v(data + pos);
    // simd256<bool> vor(false);
    uint8x16_t vor = vdupq_n_u8(0);
    for (size_t i = 0; i < N - 1; i++) {
      uint8x16_t vtmp = vceqq_u8(v, vdupq_n_u8((uint8_t)(tokens[i])));
      vor = vorrq_u8(vor, vtmp);
    }

    // neon doesn't have instrution same as movemask, to_bitmask uses shrn to
    // reduce 128bits -> 64bits. If a 128bits bool vector in x86 can convert
    // as 0101, neon shrn will convert it as 0000111100001111.
    uint64_t next = to_bitmask(vor);
    if (next) {
      pos += (TrailingZeroes(next) >> 2);
      return data[pos];
    }
    pos += VEC_LEN;
  }
  while (pos < len) {
    for (size_t i = 0; i < N - 1; i++) {
      if (data[pos] == tokens[i]) {
        return tokens[i];
      }
    }
    pos++;
  }
  return '\0';
}

// pos is the after the ending quote
sonic_force_inline int SkipString(const uint8_t *data, size_t &pos,
                                  size_t len) {
  const static int kEscaped = 2;
  const static int kNormal = 1;
  const static int kUnclosed = 0;
  uint64_t quote_bits = 0;
  uint64_t bs_bits = 0;
  int ret = kNormal;
  while (pos + VEC_LEN <= len) {
    // const simd::simd256<uint8_t> v(data + pos);
    uint8x16_t v = vld1q_u8(data + pos);
    bs_bits = to_bitmask(vceqq_u8(v, vdupq_n_u8('\\')));
    quote_bits = to_bitmask(vceqq_u8(v, vdupq_n_u8('"')));
    if (((bs_bits - 1) & quote_bits) != 0) {
      pos += (TrailingZeroes(quote_bits) >> 2) + 1;
      return ret;
    }
    if (bs_bits) {
      ret = kEscaped;
      pos += ((TrailingZeroes(bs_bits) >> 2) + 2);
      while (pos < len) {
        if (data[pos] == '\\') {
          pos += 2;
        } else {
          break;
        }
      }
    } else {
      pos += VEC_LEN;
    }
  }
  while (pos < len) {
    if (data[pos] == '\\') {
      ret = kEscaped;
      pos += 2;
      continue;
    }
    if (data[pos++] == '"') {
      break;
    }
  };
  if (pos >= len) return kUnclosed;
  return ret;
}

// return true if container is closed.
sonic_force_inline bool SkipContainer(const uint8_t *data, size_t &pos,
                                      size_t len, uint8_t left, uint8_t right) {
  int rbrace_num = 0, lbrace_num = 0, last_lbrace_num = 0;
  while (pos + VEC_LEN <= len) {
    const uint8_t *p = data + pos;
    last_lbrace_num = lbrace_num;

    uint8x16_t v = vld1q_u8(p);
    uint64_t quote_bits = to_bitmask(vceqq_u8(v, vdupq_n_u8('"')));
    uint64_t not_in_str_mask = 0xFFFFFFFFFFFFFFFF;
    int quote_idx = VEC_LEN;
    if (quote_bits) {
      quote_idx = TrailingZeroes(quote_bits);
      not_in_str_mask =
          quote_idx == 0 ? 0 : not_in_str_mask >> (64 - quote_idx);
      quote_idx = (quote_idx >> 2) + 1;  // point to next char after '"'
    }
    uint64_t to_one_mask = 0x8888888888888888ull;
    uint64_t rbrace = to_bitmask(vceqq_u8(v, vdupq_n_u8(right))) & to_one_mask &
                      not_in_str_mask;
    uint64_t lbrace = to_bitmask(vceqq_u8(v, vdupq_n_u8(left))) & to_one_mask &
                      not_in_str_mask;

    /* traverse each `right` */
    while (rbrace > 0) {
      rbrace_num++;
      lbrace_num = last_lbrace_num + CountOnes((rbrace - 1) & lbrace);
      if (lbrace_num < rbrace_num) { /* closed */
        pos += (TrailingZeroes(rbrace) >> 2) + 1;
        return true;
      }
      rbrace &= (rbrace - 1);
    }
    lbrace_num = last_lbrace_num + CountOnes(lbrace);
    pos += quote_idx;
    if (quote_bits) {
      SkipString(data, pos, len);
    }
  }

  while (pos < len) {
    uint8_t c = data[pos++];
    if (c == left) {
      lbrace_num++;
    } else if (c == right) {
      rbrace_num++;
    } else if (c == '"') {
      SkipString(data, pos, len);
    } /* else { do nothing } */

    if (lbrace_num < rbrace_num) { /* closed */
      return true;
    }
  }
  /* attach the end of string, but not closed */
  return false;
}

sonic_force_inline bool SkipArray(const uint8_t *data, size_t &pos,
                                  size_t len) {
  return SkipContainer(data, pos, len, '[', ']');
}

sonic_force_inline bool SkipObject(const uint8_t *data, size_t &pos,
                                   size_t len) {
  return SkipContainer(data, pos, len, '{', '}');
}

sonic_force_inline uint8_t SkipNumber(const uint8_t *data, size_t &pos,
                                      size_t len) {
  return GetNextToken(data, pos, len, "]},");
}

// SkipScanner is used to skip space and json values in json text.
// TODO: optimize by removing bound checking.
class SkipScanner {
 public:
  sonic_force_inline uint8_t SkipSpace(const uint8_t *data, size_t &pos) {
    // fast path for single space
    if (!IsSpace(data[pos++])) return data[pos - 1];
    if (!IsSpace(data[pos++])) return data[pos - 1];

    // current pos is out of block
    while (1) {
      uint64_t nonspace = GetNonSpaceBits(data + pos);
      if (nonspace) {
        pos += TrailingZeroes(nonspace) >> 2;
        return data[pos++];
      } else {
        pos += 16;
      }
    }
    sonic_assert(false && "!should not happen");
  }

  sonic_force_inline uint8_t SkipSpaceSafe(const uint8_t *data, size_t &pos,
                                           size_t len) {
    while (pos < len && IsSpace(data[pos++]))
      ;
    // if not found, still return the space chars
    return data[pos - 1];
  }

  sonic_force_inline SonicError GetArrayElem(const uint8_t *data, size_t &pos,
                                             size_t len, int index) {
    while (index > 0 && pos < len) {
      index--;
      char c = SkipSpaceSafe(data, pos, len);
      switch (c) {
        case '{': {
          if (!SkipObject(data, pos, len)) {
            return kParseErrorInvalidChar;
          }
          break;
        }
        case '[': {
          if (!SkipArray(data, pos, len)) {
            return kParseErrorInvalidChar;
          }
          break;
        }
        case '"': {
          if (!SkipString(data, pos, len)) {
            return kParseErrorInvalidChar;
          }
          break;
        }
      }
      // skip space and primitives
      // TODO (liuq): fast path for compat json.
      if (GetNextToken(data, pos, len, ",]") != ',') {
        return kParseErrorArrIndexOutOfRange;
      }
      pos++;
    }
    return index == 0 ? kErrorNone : kParseErrorInvalidChar;
  }

  // SkipOne skip one raw json value and return the start of value, return the
  // negtive if errors.
  sonic_force_inline long SkipOne(const uint8_t *data, size_t &pos,
                                  size_t len) {
    uint8_t c = SkipSpaceSafe(data, pos, len);
    size_t start = pos - 1;
    long err = -kParseErrorInvalidChar;

    switch (c) {
      case '"': {
        if (!SkipString(data, pos, len)) return err;
        break;
      }
      case '{': {
        if (!SkipObject(data, pos, len)) return err;
        break;
      }
      case '[': {
        if (!SkipArray(data, pos, len)) return err;
        break;
      }
      case 't':
      case 'n':
      case 'f': {
        if (!SkipLiteral(data, pos, len, c)) return err;
        break;
      }
      case '0':
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
      case '8':
      case '9':
      case '-': {
        SkipNumber(data, pos, len);
        break;
      }
      default:
        return err;
    }
    return start;
  }

  // GetOnDemand get the target json field through the path, and update the
  // position.
  template <typename JPStringType>
  long GetOnDemand(StringView json, size_t &pos,
                   const GenericJsonPointer<JPStringType> &path) {
    using namespace sonic_json::internal;
    size_t i = 0;
    const uint8_t *sp;
    long sn = 0;
    uint8_t c;
    StringView key;
    int skips;
    // TODO: use stack smallvector here.
    std::vector<uint8_t> kbuf(32);  // key buffer for parsed keys
    const uint8_t *data = reinterpret_cast<const uint8_t *>(json.data());
    size_t len = json.size();
    SonicError err = kErrorNone;

  query:
    if (i++ != path.size()) {
      c = SkipSpaceSafe(data, pos, len);
      if (path[i - 1].IsStr()) {
        if (c != '{') goto err_mismatch_type;
        c = GetNextToken(data, pos, len, "\"}");
        if (c != '"') goto err_unknown_key;
        key = StringView(path[i - 1].GetStr());
        goto obj_key;
      } else {
        if (c != '[') goto err_mismatch_type;
        err = GetArrayElem(data, pos, len, path[i - 1].GetNum());
        if (err) return -err;
        goto query;
      }
    }
    return SkipOne(data, pos, len);

  obj_key:
    // advance quote
    pos++;
    sp = data + pos;
    skips = SkipString(data, pos, len);
    sn = data + pos - 1 - sp;
    if (!skips) goto err_invalid_char;
    if (skips == 2) {
      // parse escaped key
      kbuf.resize(sn + 32);
      uint8_t *nsrc = &kbuf[0];
      std::memcpy(nsrc, sp, sn);
      sn = parseStringInplace(nsrc, err);
      if (err) {
        pos = (sp - data) + (nsrc - &kbuf[0]);
        return err;
      }
      sp = &kbuf[0];
    }

    c = SkipSpaceSafe(data, pos, len);
    if (c != ':') {
      goto err_invalid_char;
    }
    // match key and skip parsing unneeded fields
    if (sn == static_cast<long>(key.size()) &&
        std::memcmp(sp, key.data(), sn) == 0) {
      goto query;
    } else {
      c = SkipSpaceSafe(data, pos, len);
      switch (c) {
        case '{': {
          if (!SkipObject(data, pos, len)) {
            goto err_invalid_char;
          }
          break;
        }
        case '[': {
          if (!SkipArray(data, pos, len)) {
            goto err_invalid_char;
          }
          break;
        }
        case '"': {
          if (!SkipString(data, pos, len)) {
            goto err_invalid_char;
          }
          break;
        }
      }
      // skip space and , find next " or }
      c = GetNextToken(data, pos, len, "\"}");
      if (c != '"') {
        goto err_unknown_key;
      }
      goto obj_key;
    }

  err_mismatch_type:
    pos -= 1;
    return -kParseErrorMismatchType;
  err_unknown_key:
    pos -= 1;
    return -kParseErrorUnknownObjKey;
  err_invalid_char:
    pos -= 1;
    return -kParseErrorInvalidChar;
  }
};

}  // namespace neon
}  // namespace internal
}  // namespace sonic_json
