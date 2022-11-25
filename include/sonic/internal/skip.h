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
#include <immintrin.h>

#include "sonic/dom/json_pointer.h"
#include "sonic/error.h"
#include "sonic/internal/haswell.h"
#include "sonic/internal/quote.h"
#include "sonic/internal/simd.h"
#include "sonic/internal/unicode.h"
#include "sonic/macro.h"

namespace sonic_json {
namespace internal {

using namespace haswell;

static sonic_force_inline bool EqBytes4(const uint8_t *src, uint32_t target) {
  uint32_t val;
  static_assert(sizeof(uint32_t) <= SONICJSON_PADDING,
                "SONICJSON_PADDING must be larger than 4 bytes");
  std::memcpy(&val, src, sizeof(uint32_t));
  return val == target;
}

static sonic_force_inline bool IsSpace(uint8_t ch) {
  return ch == ' ' || ch == '\r' || ch == '\n' || ch == '\t';
}

sonic_force_inline uint64_t GetStringBits(const uint8_t *data,
                                          uint64_t &prev_instring,
                                          uint64_t &prev_escaped) {
  const simd::simd8x64<uint8_t> v(data);
  uint64_t escaped = 0;
  uint64_t bs_bits = v.eq('\\');
  if (bs_bits) {
    escaped = GetEscapedBranchless(prev_escaped, bs_bits);
  } else {
    escaped = prev_escaped;
    prev_escaped = 0;
  }
  uint64_t quote_bits = v.eq('"') & ~escaped;
  uint64_t in_string = prefix_xor(quote_bits) ^ prev_instring;
  prev_instring = uint64_t(static_cast<int64_t>(in_string) >> 63);
  return in_string;
}

// GetNextToken find the next characters in tokens and update the position to
// it.
template <size_t N>
sonic_force_inline uint8_t GetNextToken(const uint8_t *data, size_t &pos,
                                        size_t len, const char (&tokens)[N]) {
  while (pos + 32 <= len) {
    simd256<uint8_t> v(data + pos);
    simd256<bool> vor(false);
    for (size_t i = 0; i < N - 1; i++) {
      vor |= (v == (uint8_t)(tokens[i]));
    }
    uint32_t next = static_cast<uint32_t>(vor.to_bitmask());
    if (next) {
      pos += trailing_zeroes(next);
      return data[pos];
    }
    pos += 32;
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
  uint64_t quote_bits;
  uint64_t escaped, bs_bits, prev_escaped = 0;
  bool found = false;
  while (pos + 32 <= len) {
    const simd::simd256<uint8_t> v(data + pos);
    bs_bits = static_cast<uint32_t>((v == '\\').to_bitmask());
    quote_bits = static_cast<uint32_t>((v == '"').to_bitmask());
    if (((bs_bits - 1) & quote_bits) != 0) {
      pos += trailing_zeroes(quote_bits) + 1;
      return found ? kEscaped : kNormal;
    }
    if (bs_bits) {
      escaped = GetEscapedBranchless(prev_escaped, bs_bits);
      found = true;
      quote_bits &= ~escaped;
      if (quote_bits) {
        pos += trailing_zeroes(quote_bits) + 1;
        return kEscaped;
      }
    }
    pos += 32;
  }
  while (pos < len) {
    if (data[pos] == '\\') {
      found = true;
      pos += 2;
      continue;
    }
    if (data[pos++] == '"') {
      break;
    }
  };
  if (pos >= len) return kUnclosed;
  return found ? kEscaped : kNormal;
}

// return true if container is closed.
sonic_force_inline bool SkipContainer(const uint8_t *data, size_t &pos,
                                      size_t len, uint8_t left, uint8_t right) {
  uint64_t prev_instring = 0, prev_escaped = 0, instring;
  int rbrace_num = 0, lbrace_num = 0, last_lbrace_num;
  const uint8_t *p;
  while (pos + 64 <= len) {
    p = data + pos;
#define SKIP_LOOP()                                                     \
  {                                                                     \
    instring = GetStringBits(p, prev_instring, prev_escaped);           \
    simd::simd8x64<uint8_t> v(p);                                       \
    last_lbrace_num = lbrace_num;                                       \
    uint64_t rbrace = v.eq(right) & ~instring;                          \
    uint64_t lbrace = v.eq(left) & ~instring;                           \
    /* traverse each '}' */                                             \
    while (rbrace > 0) {                                                \
      rbrace_num++;                                                     \
      lbrace_num = last_lbrace_num + count_ones((rbrace - 1) & lbrace); \
      bool is_closed = lbrace_num < rbrace_num;                         \
      if (is_closed) {                                                  \
        sonic_assert(rbrace_num == lbrace_num + 1);                     \
        pos += trailing_zeroes(rbrace) + 1;                             \
        return true;                                                    \
      }                                                                 \
      rbrace &= (rbrace - 1);                                           \
    }                                                                   \
    lbrace_num = last_lbrace_num + count_ones(lbrace);                  \
  }
    SKIP_LOOP();
    pos += 64;
  }
  uint8_t buf[64] = {0};
  std::memcpy(buf, data + pos, len - pos);
  p = buf;
  SKIP_LOOP();
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

sonic_force_inline bool SkipLiteral(const uint8_t *data, size_t &pos,
                                    size_t len, uint8_t token) {
  static constexpr uint32_t kNullBin = 0x6c6c756e;
  static constexpr uint32_t kTrueBin = 0x65757274;
  static constexpr uint32_t kFalseBin =
      0x65736c61;  // the binary of 'alse' in false
  auto start = data + pos - 1;
  auto end = data + len;
  switch (token) {
    case 't':
      if (start + 4 <= end && EqBytes4(start, kTrueBin)) {
        pos += 3;
        return true;
      };
      break;
    case 'n':
      if (start + 4 <= end && EqBytes4(start, kNullBin)) {
        pos += 3;
        return true;
      };
      break;
    case 'f':
      if (start + 5 <= end && EqBytes4(start + 1, kFalseBin)) {
        pos += 4;
        return true;
      }
  }
  return false;
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

    uint64_t nonspace;
    // current pos is out of block
    if (pos >= nonspace_bits_end_) {
    found_space:
      while (1) {
        nonspace = GetNonSpaceBits(data + pos);
        if (nonspace) {
          nonspace_bits_end_ = pos + 64;
          pos += trailing_zeroes(nonspace);
          nonspace_bits_ = nonspace;
          return data[pos++];
        } else {
          pos += 64;
        }
      }
      sonic_assert(false && "!should not happen");
    }

    // current pos is in block
    sonic_assert(pos + 64 > nonspace_bits_end_);
    size_t block_start = nonspace_bits_end_ - 64;
    sonic_assert(pos >= block_start);
    size_t bit_pos = pos - block_start;
    uint64_t mask = (1ull << bit_pos) - 1;
    nonspace = nonspace_bits_ & (~mask);
    if (nonspace == 0) {
      pos = nonspace_bits_end_;
      goto found_space;
    }
    pos = block_start + trailing_zeroes(nonspace);
    return data[pos++];
  }

  sonic_force_inline uint8_t SkipSpaceSafe(const uint8_t *data, size_t &pos,
                                           size_t len) {
    if (pos + 64 + 2 > len) {
      goto tail;
    }
    // fast path for single space
    if (!IsSpace(data[pos++])) return data[pos - 1];
    if (!IsSpace(data[pos++])) return data[pos - 1];

    uint64_t nonspace;
    // current pos is out of block
    if (pos >= nonspace_bits_end_) {
    found_space:
      while (pos + 64 <= len) {
        nonspace = GetNonSpaceBits(data + pos);
        if (nonspace) {
          nonspace_bits_end_ = pos + 64;
          pos += trailing_zeroes(nonspace);
          nonspace_bits_ = nonspace;
          return data[pos++];
        } else {
          pos += 64;
        }
      }
      goto tail;
    }

    // current pos is in block
    {
      sonic_assert(pos + 64 > nonspace_bits_end_);
      size_t block_start = nonspace_bits_end_ - 64;
      sonic_assert(pos >= block_start);
      size_t bit_pos = pos - block_start;
      uint64_t mask = (1ull << bit_pos) - 1;
      nonspace = nonspace_bits_ & (~mask);
      if (nonspace == 0) {
        pos = nonspace_bits_end_;
        goto found_space;
      }
      pos = block_start + trailing_zeroes(nonspace);
      return data[pos++];
    }

  tail:
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

 private:
  size_t nonspace_bits_end_{0};
  uint64_t nonspace_bits_{0};
};

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
