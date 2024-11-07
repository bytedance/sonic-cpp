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

#ifndef VEC_LEN
#error "Define vector length firstly!"
#endif

template <typename T>
sonic_force_inline uint64_t GetStringBits(const uint8_t *data,
                                          uint64_t &prev_instring,
                                          uint64_t &prev_escaped) {
  const T v(data);
  uint64_t escaped = 0;
  uint64_t bs_bits = v.eq('\\');
  if (bs_bits) {
    escaped = common::GetEscaped<64>(prev_escaped, bs_bits);
  } else {
    escaped = prev_escaped;
    prev_escaped = 0;
  }
  uint64_t quote_bits = v.eq('"') & ~escaped;
  uint64_t in_string = PrefixXor(quote_bits) ^ prev_instring;
  prev_instring = uint64_t(static_cast<int64_t>(in_string) >> 63);
  return in_string;
}

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
      if (pos + 1 >= len) {
        return kUnclosed;
      }
      ret = kEscaped;
      pos += 2;
      continue;
    }
    if (data[pos++] == '"') {
      return ret;
    }
  };
  return kUnclosed;
}

// return true if container is closed.
template <typename T>
sonic_force_inline bool skip_container(const uint8_t *data, size_t &pos,
                                       size_t len, uint8_t left,
                                       uint8_t right) {
  uint64_t prev_instring = 0, prev_escaped = 0, instring;
  int rbrace_num = 0, lbrace_num = 0, last_lbrace_num;
  const uint8_t *p;
  while (pos + 64 <= len) {
    p = data + pos;
#define SKIP_LOOP()                                                    \
  {                                                                    \
    instring = GetStringBits<T>(p, prev_instring, prev_escaped);       \
    T v(p);                                                            \
    last_lbrace_num = lbrace_num;                                      \
    uint64_t rbrace = v.eq(right) & ~instring;                         \
    uint64_t lbrace = v.eq(left) & ~instring;                          \
    /* traverse each '}' */                                            \
    while (rbrace > 0) {                                               \
      rbrace_num++;                                                    \
      lbrace_num = last_lbrace_num + CountOnes((rbrace - 1) & lbrace); \
      bool is_closed = lbrace_num < rbrace_num;                        \
      if (is_closed) {                                                 \
        sonic_assert(rbrace_num == lbrace_num + 1);                    \
        pos += TrailingZeroes(rbrace) + 1;                             \
        return true;                                                   \
      }                                                                \
      rbrace &= (rbrace - 1);                                          \
    }                                                                  \
    lbrace_num = last_lbrace_num + CountOnes(lbrace);                  \
  }
    SKIP_LOOP();
    pos += 64;
  }
  uint8_t buf[64] = {0};
  std::memcpy(buf, data + pos, len - pos);
  p = buf;
  SKIP_LOOP();
#undef SKIP_LOOP
  return false;
}

sonic_force_inline uint8_t skip_space_safe(const uint8_t *data, size_t &pos,
                                           size_t len, size_t &, uint64_t &) {
  while (pos < len && IsSpace(data[pos++]))
    ;
  // if not found, still return the space chars
  return data[pos - 1];
}
