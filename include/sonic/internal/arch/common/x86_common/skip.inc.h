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

sonic_force_inline uint64_t GetStringBits(const uint8_t *data,
                                          uint64_t &prev_instring,
                                          uint64_t &prev_escaped) {
  const simd::simd8x64<uint8_t> v(data);
  uint64_t escaped = 0;
  uint64_t bs_bits = v.eq('\\');
  if (bs_bits) {
    escaped = common::GetEscapedBranchless<64>(prev_escaped, bs_bits);
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
    VecUint8Type v(data + pos);
    VecBoolType vor(false);
    for (size_t i = 0; i < N - 1; i++) {
      vor |= (v == (uint8_t)(tokens[i]));
    }
    uint32_t next = static_cast<uint32_t>(vor.to_bitmask());
    if (next) {
      pos += TrailingZeroes(next);
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
  uint64_t quote_bits;
  uint64_t escaped, bs_bits, prev_escaped = 0;
  bool found = false;
  while (pos + VEC_LEN <= len) {
    const VecUint8Type v(data + pos);
    bs_bits = static_cast<uint64_t>((v == '\\').to_bitmask());
    quote_bits = static_cast<uint64_t>((v == '"').to_bitmask());

    // maybe has escaped quotes
    if (((quote_bits - 1) & bs_bits) || prev_escaped) {
      escaped = common::GetEscapedBranchless<32>(prev_escaped, bs_bits);
      // NOTE: maybe mark the normal string as escaped, example "abc":"\\",
      // abc will marked as escaped.
      found = true;
      quote_bits &= ~escaped;
    }

    // real quote bits
    if (quote_bits) {
      pos += TrailingZeroes(quote_bits) + 1;
      return found ? kEscaped : kNormal;
    }
    pos += VEC_LEN;
  }

  // skip the possible prev escaped quote
  if (prev_escaped) {
    pos++;
  }

  // found quote for remaining bytes
  while (pos < len) {
    if (data[pos] == '\\') {
      if (pos + 1 >= len) {
        return kUnclosed;
      }
      found = true;
      pos += 2;
      continue;
    }
    if (data[pos++] == '"') {
      return found ? kEscaped : kNormal;
    }
  };
  return kUnclosed;
}

// return true if container is closed.
sonic_force_inline bool SkipContainer(const uint8_t *data, size_t &pos,
                                      size_t len, uint8_t left, uint8_t right) {
  uint64_t prev_instring = 0, prev_escaped = 0, instring;
  int rbrace_num = 0, lbrace_num = 0, last_lbrace_num;
  const uint8_t *p;
  while (pos + 64 <= len) {
    p = data + pos;
#define SKIP_LOOP()                                                    \
  {                                                                    \
    instring = GetStringBits(p, prev_instring, prev_escaped);          \
    simd::simd8x64<uint8_t> v(p);                                      \
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

// TODO: optimize by removing bound checking.
sonic_force_inline uint8_t skip_space(const uint8_t *data, size_t &pos,
                                      size_t &nonspace_bits_end,
                                      uint64_t &nonspace_bits) {
  // fast path for single space
  if (!IsSpace(data[pos++])) return data[pos - 1];
  if (!IsSpace(data[pos++])) return data[pos - 1];

  uint64_t nonspace;
  // current pos is out of block
  if (pos >= nonspace_bits_end) {
  found_space:
    while (1) {
      nonspace = GetNonSpaceBits(data + pos);
      if (nonspace) {
        nonspace_bits_end = pos + 64;
        pos += TrailingZeroes(nonspace);
        nonspace_bits = nonspace;
        return data[pos++];
      } else {
        pos += 64;
      }
    }
    sonic_assert(false && "!should not happen");
  }

  // current pos is in block
  sonic_assert(pos + 64 > nonspace_bits_end);
  size_t block_start = nonspace_bits_end - 64;
  sonic_assert(pos >= block_start);
  size_t bit_pos = pos - block_start;
  uint64_t mask = (1ull << bit_pos) - 1;
  nonspace = nonspace_bits & (~mask);
  if (nonspace == 0) {
    pos = nonspace_bits_end;
    goto found_space;
  }
  pos = block_start + TrailingZeroes(nonspace);
  return data[pos++];
}

sonic_force_inline uint8_t skip_space_safe(const uint8_t *data, size_t &pos,
                                           size_t len,
                                           size_t &nonspace_bits_end,
                                           uint64_t &nonspace_bits) {
  if (pos + 64 + 2 > len) {
    goto tail;
  }
  // fast path for single space
  if (!IsSpace(data[pos++])) return data[pos - 1];
  if (!IsSpace(data[pos++])) return data[pos - 1];

  uint64_t nonspace;
  // current pos is out of block
  if (pos >= nonspace_bits_end) {
  found_space:
    while (pos + 64 <= len) {
      nonspace = GetNonSpaceBits(data + pos);
      if (nonspace) {
        nonspace_bits_end = pos + 64;
        pos += TrailingZeroes(nonspace);
        nonspace_bits = nonspace;
        return data[pos++];
      } else {
        pos += 64;
      }
    }
    goto tail;
  }

  // current pos is in block
  {
    sonic_assert(pos + 64 > nonspace_bits_end);
    size_t block_start = nonspace_bits_end - 64;
    sonic_assert(pos >= block_start);
    size_t bit_pos = pos - block_start;
    uint64_t mask = (1ull << bit_pos) - 1;
    nonspace = nonspace_bits & (~mask);
    if (nonspace == 0) {
      pos = nonspace_bits_end;
      goto found_space;
    }
    pos = block_start + TrailingZeroes(nonspace);
    return data[pos++];
  }

tail:
  while (pos < len && IsSpace(data[pos++]))
    ;
  // if not found, still return the space chars
  return data[pos - 1];
}
