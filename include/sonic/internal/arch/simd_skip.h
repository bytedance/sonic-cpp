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

#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <vector>

#include "simd_dispatch.h"
#include "sonic/dom/flags.h"
#include "sonic/error.h"
#include "sonic/internal/arch/simd_quote.h"
#include "sonic/internal/stack.h"
#include "sonic/jsonpath/jsonpath.h"
#include "sonic/writebuffer.h"

#include INCLUDE_ARCH_FILE(skip.h)

namespace sonic_json {
namespace internal {

SONIC_USING_ARCH_FUNC(EqBytes4);
SONIC_USING_ARCH_FUNC(SkipLiteral);
SONIC_USING_ARCH_FUNC(GetNextToken);
SONIC_USING_ARCH_FUNC(SkipString);
SONIC_USING_ARCH_FUNC(SkipContainer);
SONIC_USING_ARCH_FUNC(skip_space);
SONIC_USING_ARCH_FUNC(skip_space_safe);

#define RETURN_FALSE_IF_PARSE_ERROR(x) \
  do {                                 \
    x;                                 \
    if (error_ != kErrorNone) {        \
      return false;                    \
    }                                  \
  } while (0)

static bool IsDigit(uint8_t c) { return c >= '0' && c <= '9'; }

static bool IsNonZeroDigit(uint8_t c) { return c >= '1' && c <= '9'; }

static bool IsAllowedDelimiter(uint8_t c, const char* delimiters) {
  for (const char* p = delimiters; *p != '\0'; ++p) {
    if (c == static_cast<uint8_t>(*p)) return true;
  }
  return false;
}

static bool SkipTrailingValueSpace(const uint8_t* data, size_t& pos, size_t len,
                                   const char* delimiters = ",]}") {
  while (pos < len && IsSpace(data[pos])) {
    ++pos;
  }
  return pos == len || IsAllowedDelimiter(data[pos], delimiters);
}

template <ParseFlags parseFlags>
static bool ValidateSkippedNumber(const uint8_t* data, size_t start, size_t end,
                                  Stack& scratch, SonicError& err) {
  if constexpr (parseFlags & ParseFlags::kParseOverflowNumAsNumStr) {
    return true;
  }
  const bool floating =
      std::memchr(data + start, '.', end - start) != nullptr ||
      std::memchr(data + start, 'e', end - start) != nullptr ||
      std::memchr(data + start, 'E', end - start) != nullptr;
  if (!floating) {
    return true;
  }
  size_t n = end - start;
  if (sonic_unlikely(n > std::numeric_limits<size_t>::max() - 1)) {
    err = kErrorNoMem;
    return false;
  }
  scratch.Clear();
  char* buf = scratch.PushSize<char>(n + 1);
  if (sonic_unlikely(buf == nullptr)) {
    err = kErrorNoMem;
    return false;
  }
  std::memcpy(buf, data + start, n);
  buf[n] = '\0';
  errno = 0;
  char* endptr = nullptr;
  double value = std::strtod(buf, &endptr);
  (void)value;
  if (endptr != buf + n) {
    err = kParseErrorInvalidChar;
    return false;
  }
  if (!std::isfinite(value)) {
    err = kParseErrorInfinity;
    return false;
  }
  return true;
}

template <ParseFlags parseFlags>
static bool SkipNumberStrict(const uint8_t* data, size_t& pos, size_t len,
                             const char* delimiters, Stack& scratch,
                             SonicError& err) {
  size_t i = pos - 1;
  size_t start = i;
  if (data[i] == '-') {
    ++i;
    if (i >= len) return false;
  }

  if (data[i] == '0') {
    ++i;
    if (i < len && IsDigit(data[i])) return false;
  } else if (IsNonZeroDigit(data[i])) {
    do {
      ++i;
    } while (i < len && IsDigit(data[i]));
  } else {
    return false;
  }

  if (i < len && data[i] == '.') {
    ++i;
    if (i >= len || !IsDigit(data[i])) return false;
    do {
      ++i;
    } while (i < len && IsDigit(data[i]));
  }

  if (i < len && (data[i] == 'e' || data[i] == 'E')) {
    ++i;
    if (i < len && (data[i] == '+' || data[i] == '-')) ++i;
    if (i >= len || !IsDigit(data[i])) return false;
    do {
      ++i;
    } while (i < len && IsDigit(data[i]));
  }

  pos = i;
  if (!ValidateSkippedNumber<parseFlags>(data, start, pos, scratch, err)) {
    return false;
  }
  return SkipTrailingValueSpace(data, pos, len, delimiters);
}

// SkipScanner is used to skip space and json values in json text.
class SkipScanner {
 public:
  sonic_force_inline uint8_t SkipSpace(const uint8_t* data, size_t& pos) {
    return skip_space(data, pos, nonspace_bits_end_, nonspace_bits_);
  }

  sonic_force_inline uint8_t SkipSpaceSafe(const uint8_t* data, size_t& pos,
                                           size_t len) {
    return skip_space_safe(data, pos, len, nonspace_bits_end_, nonspace_bits_);
  }

  template <ParseFlags parseFlags = ParseFlags::kParseDefault>
  sonic_force_inline SonicError GetArrayElem(const uint8_t* data, size_t& pos,
                                             size_t len, uint64_t index) {
    char c = SkipSpaceSafe(data, pos, len);
    if (c == ']') {
      return kParseErrorArrIndexOutOfRange;
    }
    pos--;
    while (index > 0 && pos < len) {
      index--;
      long start = SkipOneOnDemand<parseFlags>(data, pos, len, ",]");
      if (start < 0) {
        return SonicError(-start);
      }
      c = SkipSpaceSafe(data, pos, len);
      if (c == ']') {
        pos--;
        return kParseErrorArrIndexOutOfRange;
      }
      if (c != ',') return kParseErrorInvalidChar;
    }
    return index == 0 ? kErrorNone : kParseErrorInvalidChar;
  }

  // SkipOne skip one raw json value and return the start of value, return the
  // negative if errors.
  template <ParseFlags parseFlags = ParseFlags::kParseDefault>
  inline long SkipOne(const uint8_t* data, size_t& pos, size_t len,
                      const char* delimiters = ",]}") {
    if (sonic_unlikely(pos >= len)) return -kParseErrorInvalidChar;
    uint8_t c = SkipSpaceSafe(data, pos, len);
    size_t start = pos - 1;
    SonicError err = kParseErrorInvalidChar;

    switch (c) {
      case '"': {
        if (!SkipStringStrict<parseFlags>(data, pos, len, scratch_, err)) {
          return -err;
        }
        break;
      }
      case '{': {
        if (sonic_unlikely(depth_ >= kMaxSkipDepth))
          return -kParseErrorInvalidChar;
        ++depth_;
        bool ok = SkipObjectStrict<parseFlags>(data, pos, len, err);
        --depth_;
        if (!ok) return -err;
        break;
      }
      case '[': {
        if (sonic_unlikely(depth_ >= kMaxSkipDepth))
          return -kParseErrorInvalidChar;
        ++depth_;
        bool ok = SkipArrayStrict<parseFlags>(data, pos, len, err);
        --depth_;
        if (!ok) return -err;
        break;
      }
      case 't':
      case 'n':
      case 'f': {
        if (!SkipLiteral(data, pos, len, c)) return -err;
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
        if (!SkipNumberStrict<parseFlags>(data, pos, len, delimiters, scratch_,
                                          err)) {
          return -err;
        }
        return start;
      }
      default:
        return -err;
    }
    if (!SkipTrailingValueSpace(data, pos, len, delimiters)) return -err;
    return start;
  }

  template <ParseFlags parseFlags = ParseFlags::kParseDefault>
  inline long SkipOneOnDemand(const uint8_t* data, size_t& pos, size_t len,
                              const char* delimiters = ",]}") {
    if constexpr (parseFlags & ParseFlags::kParseValidateOnDemandFull) {
      return SkipOne<parseFlags>(data, pos, len, delimiters);
    } else {
      return SkipOneFast<parseFlags>(data, pos, len, delimiters);
    }
  }

  template <ParseFlags parseFlags = ParseFlags::kParseDefault>
  sonic_force_inline ParseResult ValidateJson(StringView json) {
    size_t pos = 0;
    const uint8_t* data = reinterpret_cast<const uint8_t*>(json.data());
    long start = SkipOne<parseFlags>(data, pos, json.size(), "");
    if (start < 0) return ParseResult(SonicError(-start), pos);
    if (pos != json.size()) return ParseResult(kParseErrorInvalidChar, pos);
    return ParseResult(kErrorNone, pos);
  }

  template <ParseFlags parseFlags = ParseFlags::kParseDefault>
  sonic_force_inline bool SkipStringStrict(const uint8_t* data, size_t& pos,
                                           size_t len, Stack& scratch,
                                           SonicError& err) {
    auto start = data + pos;
    auto status = SkipString(data, pos, len);
    if (!status) {
      err = SonicError::kParseErrorInvalidChar;
      return false;
    }
    auto slen = data + pos - 1 - start;
    if (status == 1) {
      if constexpr (!(parseFlags &
                      ParseFlags::kParseAllowUnescapedControlChars)) {
        for (const uint8_t* p = start; p < data + pos - 1; ++p) {
          if (*p < 0x20) {
            err = kParseErrorUnEscaped;
            pos = p - data;
            return false;
          }
        }
      }
      return true;
    }
    scratch.Clear();
    size_t scratch_len = static_cast<size_t>(slen);
    if (sonic_unlikely(scratch_len > std::numeric_limits<size_t>::max() - 32)) {
      err = kErrorNoMem;
      return false;
    }
    uint8_t* nsrc =
        reinterpret_cast<uint8_t*>(scratch.PushSize<char>(scratch_len + 32));
    if (sonic_unlikely(nsrc == nullptr)) {
      err = kErrorNoMem;
      return false;
    }
    uint8_t* nsrc_begin = nsrc;
    std::memcpy(nsrc, start, slen + 1);
    SonicError parse_err = kErrorNone;
    (void)parseStringInplace<parseFlags>(nsrc, parse_err);
    if (parse_err) {
      err = parse_err;
      pos = (start - data) + (nsrc - nsrc_begin);
      return false;
    }
    return true;
  }

  template <ParseFlags parseFlags = ParseFlags::kParseDefault>
  sonic_force_inline bool matchKey(const uint8_t* data, size_t& pos, size_t len,
                                   StringView key, Stack& kbuf,
                                   SonicError& err) {
    auto start = data + pos;
    auto status = SkipString(data, pos, len);
    // has errors
    if (!status) {
      err = SonicError::kParseErrorInvalidChar;
      return false;
    }

    auto slen = data + pos - 1 - start;
    // has escaped char
    if (status == 2) {
      // parse escaped key
      kbuf.Clear();
      size_t scratch_len = static_cast<size_t>(slen);
      if (sonic_unlikely(scratch_len >
                         std::numeric_limits<size_t>::max() - 32)) {
        err = kErrorNoMem;
        return false;
      }
      uint8_t* nsrc =
          reinterpret_cast<uint8_t*>(kbuf.PushSize<char>(scratch_len + 32));
      if (sonic_unlikely(nsrc == nullptr)) {
        err = kErrorNoMem;
        return false;
      }
      uint8_t* nsrc_begin = nsrc;

      // parseStringInplace need `"` as the end
      std::memcpy(nsrc, start, slen + 1);
      SonicError parse_err = kErrorNone;
      slen = parseStringInplace<parseFlags>(nsrc, parse_err);
      if (parse_err) {
        err = parse_err;
        pos = (start - data) + (nsrc - nsrc_begin);
        return false;
      }
      start = nsrc_begin;
    } else if constexpr (!(parseFlags &
                           ParseFlags::kParseAllowUnescapedControlChars)) {
      for (const uint8_t* p = start; p < data + pos - 1; ++p) {
        if (*p < 0x20) {
          err = kParseErrorUnEscaped;
          pos = p - data;
          return false;
        }
      }
    }
    // compare the key
    return slen == static_cast<long>(key.size()) &&
           std::memcmp(start, key.data(), slen) == 0;
  }

  template <ParseFlags parseFlags = ParseFlags::kParseDefault>
  sonic_force_inline int matchKeys(const uint8_t* data, size_t& pos, size_t len,
                                   const std::vector<StringView>& keys,
                                   Stack& kbuf, SonicError& err) {
    auto start = data + pos;
    auto status = SkipString(data, pos, len);
    // has errors
    if (!status) {
      err = SonicError::kParseErrorInvalidChar;
      return -1;
    }

    auto slen = data + pos - 1 - start;
    // has escaped char
    if (status == 2) {
      // parse escaped key
      kbuf.Clear();
      size_t scratch_len = static_cast<size_t>(slen);
      if (sonic_unlikely(scratch_len >
                         std::numeric_limits<size_t>::max() - 32)) {
        err = kErrorNoMem;
        return -1;
      }
      uint8_t* nsrc =
          reinterpret_cast<uint8_t*>(kbuf.PushSize<char>(scratch_len + 32));
      if (sonic_unlikely(nsrc == nullptr)) {
        err = kErrorNoMem;
        return -1;
      }
      uint8_t* nsrc_begin = nsrc;

      // parseStringInplace need `"` as the end
      std::memcpy(nsrc, start, slen + 1);
      SonicError parse_err = kErrorNone;
      slen = parseStringInplace<parseFlags>(nsrc, parse_err);
      if (parse_err) {
        err = parse_err;
        pos = (start - data) + (nsrc - nsrc_begin);
        return -1;
      }
      start = nsrc_begin;
    } else if constexpr (!(parseFlags &
                           ParseFlags::kParseAllowUnescapedControlChars)) {
      for (const uint8_t* p = start; p < data + pos - 1; ++p) {
        if (*p < 0x20) {
          err = kParseErrorUnEscaped;
          pos = p - data;
          return -1;
        }
      }
    }

    for (size_t i = 0; i < keys.size(); i++) {
      const auto& key = keys[i];
      if (slen == static_cast<long>(key.size()) &&
          std::memcmp(start, key.data(), slen) == 0) {
        return i;
      }
    }
    // compare the key
    return -1;
  }

  // GetOnDemand get the target json field through the path, and update the
  // position.
  template <ParseFlags parseFlags, typename JPStringType>
  long GetOnDemand(StringView json, size_t& pos,
                   const GenericJsonPointer<JPStringType>& path) {
    using namespace sonic_json::internal;
    size_t i = 0;
    uint8_t c;
    StringView key;
    // TODO: use stack smallvector here.
    Stack kbuf(32);  // key buffer for parsed keys
    const uint8_t* data = reinterpret_cast<const uint8_t*>(json.data());
    size_t len = json.size();
    SonicError err = kErrorNone;
    bool matched = false;
    Stack path_context(path.size());  // closing token for matched path parents
    if (sonic_unlikely(path_context.HadOom())) return -kErrorNoMem;

  query:
    if (i++ != path.size()) {
      c = SkipSpaceSafe(data, pos, len);
      if (path[i - 1].IsStr()) {
        if (c != '{') goto err_mismatch_type;
        c = SkipSpaceSafe(data, pos, len);
        if (c == '}') {
          pos--;
          goto err_unknown_key;
        }
        if (c != '"') return -kParseErrorInvalidChar;
        pos--;
        key = StringView(path[i - 1].GetStr());
        goto obj_key;
      } else {
        if (c != '[') goto err_mismatch_type;
        if (sonic_unlikely(!path[i - 1].IsValidNum())) {
          return -kParseErrorArrIndexOutOfRange;
        }
        err = GetArrayElem<parseFlags>(data, pos, len, path[i - 1].GetNum());
        if (err) return -err;
        if (sonic_unlikely(!path_context.Push<char>(']'))) {
          return -kErrorNoMem;
        }
        goto query;
      }
    }
    {
      long start = SkipOneOnDemand<parseFlags>(data, pos, len,
                                               valueDelimiters(path_context));
      if (start < 0) return start;
      size_t err_pos = pos;
      err = validateMatchedPathSuffix(data, pos, len, path_context, err_pos);
      if (err) {
        pos = err_pos;
        return -err;
      }
      return start;
    }

  obj_key:
    // advance quote
    pos++;

    matched = matchKey<parseFlags>(data, pos, len, key, kbuf, err);
    if (err != kErrorNone) {
      return -err;
    }

    c = SkipSpaceSafe(data, pos, len);
    if (c != ':') {
      goto err_invalid_char;
    }

    // match key and skip parsing unneeded fields
    if (matched) {
      if (sonic_unlikely(!path_context.Push<char>('}'))) {
        return -kErrorNoMem;
      }
      goto query;
    } else {
      long start = SkipOneOnDemand<parseFlags>(data, pos, len, ",}");
      if (start < 0) return start;
      c = SkipSpaceSafe(data, pos, len);
      if (c == '}') {
        goto err_unknown_key;
      }
      if (c != ',') goto err_invalid_char;
      c = SkipSpaceSafe(data, pos, len);
      if (c != '"') goto err_invalid_char;
      pos--;
      goto obj_key;
    }

  err_mismatch_type:
    pos -= 1;
    return -kParseErrorMismatchType;
  err_unknown_key:
    return -kParseErrorUnknownObjKey;
  err_invalid_char:
    pos -= 1;
    return -kParseErrorInvalidChar;
  }

 private:
  // Default OnDemand keeps short-circuit semantics: validate scalar values and
  // value boundaries, but use SIMD container skipping instead of recursively
  // validating every unvisited subtree. Full validation routes through SkipOne.
  template <ParseFlags parseFlags = ParseFlags::kParseDefault>
  inline long SkipOneFast(const uint8_t* data, size_t& pos, size_t len,
                          const char* delimiters) {
    if (sonic_unlikely(pos >= len)) return -kParseErrorInvalidChar;
    uint8_t c = SkipSpaceSafe(data, pos, len);
    size_t start = pos - 1;
    SonicError err = kParseErrorInvalidChar;

    switch (c) {
      case '"': {
        if (!SkipStringStrict<parseFlags>(data, pos, len, scratch_, err)) {
          return -err;
        }
        break;
      }
      case '{': {
        if (sonic_unlikely(StartsWithMismatchedClose(data, pos, len, ']'))) {
          return -err;
        }
        if (!SkipContainer(data, pos, len, '{', '}')) return -err;
        break;
      }
      case '[': {
        if (sonic_unlikely(StartsWithMismatchedClose(data, pos, len, '}'))) {
          return -err;
        }
        if (!SkipContainer(data, pos, len, '[', ']')) return -err;
        break;
      }
      case 't':
      case 'n':
      case 'f': {
        if (!SkipLiteral(data, pos, len, c)) return -err;
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
        if (!SkipNumberStrict<parseFlags>(data, pos, len, delimiters, scratch_,
                                          err)) {
          return -err;
        }
        return start;
      }
      default:
        return -err;
    }
    if (!SkipTrailingValueSpace(data, pos, len, delimiters)) return -err;
    return start;
  }

  static sonic_force_inline bool isPotentialJsonValueStart(uint8_t c) {
    switch (c) {
      case '"':
      case '{':
      case '[':
      case 't':
      case 'f':
      case 'n':
      case '-':
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
        return true;
      default:
        return false;
    }
  }

  // SkipContainer tracks only the requested bracket type. Reject the obvious
  // opposite-close case before entering that fast path.
  sonic_force_inline bool StartsWithMismatchedClose(const uint8_t* data,
                                                    size_t pos, size_t len,
                                                    uint8_t close) {
    uint8_t c = SkipSpaceSafe(data, pos, len);
    return c == close;
  }

  static sonic_force_inline const char* valueDelimiters(
      const Stack& path_context) {
    if (path_context.Empty()) return "";
    return *path_context.Top<char>() == '}' ? ",}" : ",]";
  }

  sonic_force_inline SonicError
  validateMatchedPathSuffix(const uint8_t* data, size_t value_end, size_t len,
                            const Stack& path_context, size_t& err_pos) {
    size_t cursor = value_end;
    for (const char* frame = path_context.End<char>();
         frame != path_context.Begin<char>();) {
      const char closing = *--frame;
      uint8_t c = SkipSpaceSafe(data, cursor, len);
      if (c == static_cast<uint8_t>(closing)) {
        continue;
      }
      if (c == ',') {
        c = SkipSpaceSafe(data, cursor, len);
        if ((closing == '}' && c == '"') ||
            (closing == ']' && isPotentialJsonValueStart(c))) {
          return kErrorNone;
        }
      }
      err_pos = cursor == 0 ? 0 : cursor - 1;
      return kParseErrorInvalidChar;
    }
    return kErrorNone;
  }

  template <ParseFlags parseFlags = ParseFlags::kParseDefault>
  sonic_force_inline bool SkipObjectStrict(const uint8_t* data, size_t& pos,
                                           size_t len, SonicError& err) {
    uint8_t c = SkipSpaceSafe(data, pos, len);
    if (c == '}') return true;
    while (true) {
      if (c != '"') return false;
      if (!SkipStringStrict<parseFlags>(data, pos, len, scratch_, err)) {
        return false;
      }
      c = SkipSpaceSafe(data, pos, len);
      if (c != ':') return false;
      long start = SkipOne<parseFlags>(data, pos, len, ",}");
      if (start < 0) {
        err = SonicError(-start);
        return false;
      }
      c = SkipSpaceSafe(data, pos, len);
      if (c == '}') return true;
      if (c != ',') return false;
      c = SkipSpaceSafe(data, pos, len);
      if (c == '}') return false;
    }
  }

  template <ParseFlags parseFlags = ParseFlags::kParseDefault>
  sonic_force_inline bool SkipArrayStrict(const uint8_t* data, size_t& pos,
                                          size_t len, SonicError& err) {
    uint8_t c = SkipSpaceSafe(data, pos, len);
    if (c == ']') return true;
    pos--;
    while (true) {
      long start = SkipOne<parseFlags>(data, pos, len, ",]");
      if (start < 0) {
        err = SonicError(-start);
        return false;
      }
      c = SkipSpaceSafe(data, pos, len);
      if (c == ']') return true;
      if (c != ',') return false;
      c = SkipSpaceSafe(data, pos, len);
      if (c == ']') return false;
      pos--;
    }
  }

  size_t nonspace_bits_end_{0};
  uint64_t nonspace_bits_{0};
  Stack scratch_{32};
  size_t depth_{0};
  static constexpr size_t kMaxSkipDepth = 1024;
};

class SkipScanner2 {
 public:
  static constexpr ParseFlags kJsonPathParseFlags =
      ParseFlags::kParseAllowUnescapedControlChars |
      ParseFlags::kParseIntegerAsRaw;

  template <ParseFlags parseFlags = kJsonPathParseFlags>
  sonic_force_inline StringView getOne() {
    long start = scanner_.template SkipOne<parseFlags>(data_, pos_, len_);
    if (start < 0) {
      setError(SonicError(-start));
      return "";
    }
    return StringView(reinterpret_cast<const char*>(data_) + start,
                      pos_ - start);
  }

  template <ParseFlags parseFlags = kJsonPathParseFlags>
  sonic_force_inline SonicError skipOne() {
    long start = scanner_.template SkipOne<parseFlags>(data_, pos_, len_);
    if (start < 0) {
      setError(SonicError(-start));
      return error_;
    }
    return SonicError::kErrorNone;
  }

  sonic_force_inline uint8_t peek() {
    if (sonic_unlikely(pos_ >= len_)) {
      setError(SonicError::kParseErrorEof);
      return 0;
    }

    auto c = scanner_.SkipSpaceSafe(data_, pos_, len_);

    // If we reached the end while still seeing spaces, there is no token.
    if (sonic_unlikely(pos_ >= len_) &&
        (c == ' ' || c == '\n' || c == '\r' || c == '\t')) {
      setError(SonicError::kParseErrorEof);
      return 0;
    }

    pos_ -= 1;
    return c;
  }

  sonic_force_inline bool hasError() {
    return error_ != SonicError::kErrorNone;
  }

  sonic_force_inline bool consumeOnlyTrailingSpaces() {
    while (pos_ < len_ && IsSpace(data_[pos_])) {
      ++pos_;
    }
    if (pos_ != len_) {
      setError(kParseErrorInvalidChar);
      return false;
    }
    return true;
  }

  sonic_force_inline void setIsFieldName() { this->isFieldName = true; }
  sonic_force_inline bool getAndClearIsFieldName() {
    auto ret = this->isFieldName;
    this->isFieldName = false;
    return ret;
  }
  sonic_force_inline uint8_t advance() {
    return scanner_.SkipSpaceSafe(data_, pos_, len_);
  }

  //

  sonic_force_inline bool consumeValueSeparatorOrEnd(uint8_t end,
                                                     bool& has_next) {
    uint8_t c = peek();
    if (sonic_unlikely(hasError())) {
      return false;
    }
    if (c == end) {
      has_next = false;
      return true;
    }
    if (c != ',') {
      setError(SonicError::kParseErrorInvalidChar);
      return false;
    }

    advance();
    c = peek();
    if (sonic_unlikely(hasError())) {
      return false;
    }
    if (c == end) {
      setError(SonicError::kParseErrorInvalidChar);
      return false;
    }
    has_next = true;
    return true;
  }

  sonic_force_inline bool consume(uint8_t c) {
    if (sonic_unlikely(pos_ >= len_)) {
      setError(SonicError::kParseErrorEof);
      return false;
    }

    auto got = scanner_.SkipSpaceSafe(data_, pos_, len_);
    if (got != c) {
      if (sonic_unlikely(pos_ >= len_) &&
          (got == ' ' || got == '\n' || got == '\r' || got == '\t')) {
        setError(SonicError::kParseErrorEof);
      } else {
        setError(SonicError::kParseErrorInvalidChar);
      }
      return false;
    }
    return true;
  }

  sonic_force_inline void setError(SonicError err) { error_ = err; }

  // Precondition: calling advance takes input the " of the first fieldname
  // post condition: if found peek() returns first char of the found value
  // if not found, peek() returns }
  template <ParseFlags parseFlags = kJsonPathParseFlags>
  sonic_force_inline bool advanceKey(StringView key) {
    bool matched = false;
    while (peek() != '}') {
      auto c = advance();
      if (c != '"') {
        setError(SonicError::kParseErrorInvalidChar);
        return false;
      }

      // match the key
      matched = scanner_.template matchKey<parseFlags>(data_, pos_, len_, key,
                                                       kbuf_, error_);
      if (error_ != SonicError::kErrorNone) {
        return false;
      }

      if (!consume(':')) {
        setError(SonicError::kParseErrorInvalidChar);
        return false;
      }

      if (matched) {
        break;
      }

      RETURN_FALSE_IF_PARSE_ERROR(skipOne<parseFlags>());

      bool has_next = false;
      if (!consumeValueSeparatorOrEnd('}', has_next)) {
        return false;
      }
      if (!has_next) {
        break;
      }
    }
    return matched;
  }

  // Precondition: calling advance takes input the " of the first fieldname
  // post condition: if found peek() returns first char of the found value
  // if not found, peek() returns }
  template <ParseFlags parseFlags = kJsonPathParseFlags>
  sonic_force_inline int advanceKeys(const std::vector<StringView>& keys) {
    int matched = -1;
    while (peek() != '}') {
      auto c = advance();
      if (c != '"') {
        setError(SonicError::kParseErrorInvalidChar);
        return -1;
      }

      // match the key
      matched = scanner_.template matchKeys<parseFlags>(data_, pos_, len_, keys,
                                                        kbuf_, error_);
      if (error_ != SonicError::kErrorNone) {
        return -1;
      }

      if (!consume(':')) {
        setError(SonicError::kParseErrorInvalidChar);
        return -1;
      }

      if (matched != -1) {
        break;
      }

      RETURN_FALSE_IF_PARSE_ERROR(skipOne<parseFlags>());

      bool has_next = false;
      if (!consumeValueSeparatorOrEnd('}', has_next)) {
        return -1;
      }
      if (!has_next) {
        break;
      }
    }
    return matched;
  }

  sonic_force_inline SonicError traverseObject(const JsonPath& path,
                                               size_t index,
                                               std::vector<StringView>& res) {
    while (peek() != '}') {
      auto c = advance();
      if (c != '"') {
        setError(SonicError::kParseErrorInvalidChar);
        return error_;
      }

      // skip the key
      if (!scanner_.template SkipStringStrict<kJsonPathParseFlags>(
              data_, pos_, len_, kbuf_, error_)) {
        if (!hasError()) setError(SonicError::kParseErrorInvalidChar);
        return error_;
      }

      if (!consume(':')) {
        return error_;
      }

      // recursively parse the value
      if (getJsonPath(path, index + 1, res, true) != SonicError::kErrorNone) {
        return error_;
      }

      bool has_next = false;
      if (!consumeValueSeparatorOrEnd('}', has_next)) {
        return error_;
      }
      if (!has_next) break;
    }
    return kErrorNone;
  }

  sonic_force_inline SonicError traverseArray(const JsonPath& path,
                                              size_t index,
                                              std::vector<StringView>& res) {
    while (peek() != ']') {
      // recursively parse the value
      if (getJsonPath(path, index + 1, res, true) != SonicError::kErrorNone) {
        return error_;
      }

      bool has_next = false;
      if (!consumeValueSeparatorOrEnd(']', has_next)) {
        return error_;
      }
      if (!has_next) break;
    }
    return kErrorNone;
  }

  sonic_force_inline bool advanceIndex(size_t index) /* found */ {
    if (peek() == ']') {
      return false;
    }

    while (index > 0) {
      if (skipOne<kJsonPathParseFlags>() != SonicError::kErrorNone) {
        return false;
      }

      bool has_next = false;
      if (!consumeValueSeparatorOrEnd(']', has_next)) {
        return false;
      }
      if (!has_next) return false;
      --index;
    }

    return true;
  }

  sonic_force_inline SonicError skipArrayRemain() {
    bool has_next = false;
    if (!consumeValueSeparatorOrEnd(']', has_next)) {
      return error_;
    }
    while (has_next) {
      if (skipOne<kJsonPathParseFlags>() != SonicError::kErrorNone) {
        return error_;
      }
      if (!consumeValueSeparatorOrEnd(']', has_next)) {
        return error_;
      }
    }
    return kErrorNone;
  }

  sonic_force_inline SonicError skipObjectRemain() {
    bool has_next = false;
    if (!consumeValueSeparatorOrEnd('}', has_next)) {
      return error_;
    }
    while (has_next) {
      auto c = advance();
      if (c != '"') {
        setError(SonicError::kParseErrorInvalidChar);
        return error_;
      }

      // skip the key
      if (!scanner_.template SkipStringStrict<kJsonPathParseFlags>(
              data_, pos_, len_, kbuf_, error_)) {
        if (!hasError()) setError(SonicError::kParseErrorInvalidChar);
        return error_;
      }

      if (!consume(':')) {
        return error_;
      }

      if (skipOne<kJsonPathParseFlags>() != SonicError::kErrorNone) {
        return error_;
      }

      if (!consumeValueSeparatorOrEnd('}', has_next)) {
        return error_;
      }
    }
    return kErrorNone;
  }

  enum WriteStyle { RAW, FLATTEN, QUOTE };
  enum JsonValueType { STRING, OTHER };

  template <SerializeFlags serializeFlags>
  class JsonGeneratorInterface {
   public:
    virtual bool writeRaw(StringView sv) = 0;
    virtual bool copyCurrentStructure(StringView sv) = 0;
    virtual bool copyCurrentStructureSingleResult(StringView sv) = 0;
    virtual bool copyCurrentStructureJsonTupleCodeGen(
        StringView raw, size_t index,
        std::vector<std::optional<std::string>>& result,
        JsonValueType type) = 0;
    virtual bool writeRawValue(StringView sv) = 0;
    virtual bool writeStartArray() = 0;
    virtual bool writeEndArray() = 0;
    virtual bool writeComma() = 0;
    virtual bool isEmpty() = 0;
    virtual bool isBeginArray() = 0;
    virtual SonicError getError() const { return kParseErrorUnexpect; }
    virtual ~JsonGeneratorInterface() {}
  };
  template <SerializeFlags serializeFlags>
  using JsonGeneratorFactory =
      std::function<std::shared_ptr<JsonGeneratorInterface<serializeFlags>>(
          WriteBuffer&)>;

  template <SerializeFlags serializeFlags>
  inline bool setJsonGeneratorError(
      JsonGeneratorInterface<serializeFlags>* jsonGenerator) {
    SonicError err = jsonGenerator->getError();
    setError(err == kErrorNone ? kParseErrorUnexpect : err);
    return false;
  }

  template <WriteStyle style,
            SerializeFlags serializeFlags = kSerializeJavaStyleFlag>
  inline bool getJsonPathArrayIndex(
      const JsonPath& path, size_t index,
      JsonGeneratorInterface<serializeFlags>* jsonGenerator,
      const JsonGeneratorFactory<serializeFlags>& jsonGeneratorFactory,
      const int64_t idx) {
    RETURN_FALSE_IF_PARSE_ERROR(consume('['));
    int64_t cur_idx = 0;
    bool dirty = false;
    while (peek() != ']') {
      if (cur_idx == idx) {
        dirty = getJsonPath<style, serializeFlags>(
            path, index + 1, jsonGenerator, jsonGeneratorFactory);
        if (error_ != kErrorNone) {
          return false;
        }
        bool has_next = false;
        if (!consumeValueSeparatorOrEnd(']', has_next)) {
          return false;
        }
        while (has_next) {
          RETURN_FALSE_IF_PARSE_ERROR(skipOne<kJsonPathParseFlags>());
          if (!consumeValueSeparatorOrEnd(']', has_next)) {
            return false;
          }
        }
        break;
      } else {
        RETURN_FALSE_IF_PARSE_ERROR(skipOne<kJsonPathParseFlags>());
      }
      bool has_next = false;
      if (!consumeValueSeparatorOrEnd(']', has_next)) {
        return false;
      }
      if (!has_next) {
        break;
      }
      cur_idx++;
    }
    RETURN_FALSE_IF_PARSE_ERROR(consume(']'));
    return dirty;
  }
  template <SerializeFlags serializeFlags>
  inline bool jsonTupleWithCodeGenImpl(
      const std::vector<StringView>& keys,
      JsonGeneratorInterface<serializeFlags>* jsonGenerator,
      std::vector<std::optional<std::string>>& result) {
    RETURN_FALSE_IF_PARSE_ERROR(consume('{'));

    std::vector<uint8_t> seen(keys.size(), 0);

    while (peek() != '}') {
      int keyMatchIndex = advanceKeys<kJsonPathParseFlags>(keys);
      if (error_ != kErrorNone) {
        return false;
      }
      if (keyMatchIndex != -1) {
        size_t key_index = static_cast<size_t>(keyMatchIndex);
        if (seen[key_index]) {
          RETURN_FALSE_IF_PARSE_ERROR(skipOne<kJsonPathParseFlags>());
        } else if (peek() == 'n') {
          // do not do anything for null
          seen[key_index] = 1;
          RETURN_FALSE_IF_PARSE_ERROR(skipOne<kJsonPathParseFlags>());
        } else {
          seen[key_index] = 1;
          JsonValueType type =
              peek() == '"' ? JsonValueType::STRING : JsonValueType::OTHER;
          const auto sv = getOne<kJsonPathParseFlags>();
          if (error_ != kErrorNone) {
            return false;
          }
          const auto copy_success =
              jsonGenerator->copyCurrentStructureJsonTupleCodeGen(
                  sv, keyMatchIndex, result, type);
          if (!copy_success) {
            return setJsonGeneratorError(jsonGenerator);
          }
        }
      }
      bool has_next = false;
      if (!consumeValueSeparatorOrEnd('}', has_next)) {
        return false;
      }
      if (!has_next) {
        break;
      }
    }

    RETURN_FALSE_IF_PARSE_ERROR(consume('}'));
    return true;
  }
  template <SerializeFlags serializeFlags>
  inline std::vector<std::optional<std::string>> jsonTupleWithCodeGen(
      const std::vector<StringView>& keys,
      JsonGeneratorInterface<serializeFlags>* jsonGenerator, bool legacy) {
    std::vector<std::optional<std::string>> result(keys.size(), std::nullopt);
    bool success = jsonTupleWithCodeGenImpl(keys, jsonGenerator, result);
    if (success) {
      success = consumeOnlyTrailingSpaces();
    }

    if (!success && !legacy) {
      std::vector<std::optional<std::string>> all_nulls(keys.size(),
                                                        std::nullopt);
      return all_nulls;
    }

    return result;
  }
  template <WriteStyle style,
            SerializeFlags serializeFlags = kSerializeJavaStyleFlag>
  inline bool getJsonPath(
      const JsonPath& path, size_t index,
      JsonGeneratorInterface<serializeFlags>* jsonGenerator,
      const JsonGeneratorFactory<serializeFlags>& jsonGeneratorFactory) {
    const bool path_is_nil = index >= path.size();
    const auto c = peek();
    const bool is_field_name = getAndClearIsFieldName();
    const bool value_string = !is_field_name && c == '"';
    const bool field_name = c == '"' && is_field_name;

    if (is_field_name && !value_string && !field_name) {
      setError(kParseErrorInvalidChar);
      return false;
    }
    // Primitive values cannot satisfy a non-root path, but they still need to
    // be consumed so callers can distinguish "no match" from malformed suffix.
    if (!path_is_nil && (c == 'n' || c == 't' || c == 'f' || c == '-' ||
                         (c >= '0' && c <= '9'))) {
      RETURN_FALSE_IF_PARSE_ERROR(skipOne<kJsonPathParseFlags>());
      return false;
    }
    if (value_string && !path_is_nil) {
      RETURN_FALSE_IF_PARSE_ERROR(skipOne<kJsonPathParseFlags>());
      return false;
    }

    if (value_string && path_is_nil) {
      if constexpr (style == RAW) {
        const auto sv = getOne<kJsonPathParseFlags>();
        if (error_ != kErrorNone) {
          return false;
        }
        if (!jsonGenerator->writeRaw(sv)) {
          return setJsonGeneratorError(jsonGenerator);
        }
        return true;
      }
    }

    if (c == '[' && path_is_nil) {
      if constexpr (style == FLATTEN) {
        RETURN_FALSE_IF_PARSE_ERROR(consume('['));
        bool dirty = false;

        while (peek() != ']') {
          dirty |= getJsonPath<FLATTEN, serializeFlags>(
              path, index + 1, jsonGenerator, jsonGeneratorFactory);
          if (error_ != kErrorNone) {
            return false;
          }
          bool has_next = false;
          if (!consumeValueSeparatorOrEnd(']', has_next)) {
            return false;
          }
          if (!has_next) {
            break;
          }
        }
        RETURN_FALSE_IF_PARSE_ERROR(consume(']'));
        return dirty;
      }
    }

    if (path_is_nil) {
      if (!jsonGenerator->isBeginArray() && !jsonGenerator->isEmpty()) {
        if (!jsonGenerator->writeComma()) {
          return setJsonGeneratorError(jsonGenerator);
        }
      }

      const auto sv = getOne<kJsonPathParseFlags>();
      if (error_ != kErrorNone) {
        return false;
      }
      const auto copy_success = jsonGenerator->copyCurrentStructure(sv);
      if (!copy_success) {
        return setJsonGeneratorError(jsonGenerator);
      }

      return true;
    }

    if (c == '{' && path[index].is_key()) {
      RETURN_FALSE_IF_PARSE_ERROR(consume('{'));
      bool dirty = false;
      while (peek() != '}') {
        if (dirty) {
          // Skip children
          while (peek() != '}') {
            uint8_t key = advance();
            if (key != '"' ||
                !scanner_.template SkipStringStrict<kJsonPathParseFlags>(
                    data_, pos_, len_, kbuf_, error_)) {
              if (!hasError()) setError(SonicError::kParseErrorInvalidChar);
              return false;
            }
            RETURN_FALSE_IF_PARSE_ERROR(consume(':'));
            RETURN_FALSE_IF_PARSE_ERROR(skipOne<kJsonPathParseFlags>());
            bool has_next = false;
            if (!consumeValueSeparatorOrEnd('}', has_next)) {
              return false;
            }
            if (!has_next) {
              break;
            }
          }
        } else {
          // The next "string_value" is a key
          setIsFieldName();
          dirty = getJsonPath<style, serializeFlags>(path, index, jsonGenerator,
                                                     jsonGeneratorFactory);
          if (error_ != kErrorNone) {
            return false;
          }
          bool has_next = false;
          if (!consumeValueSeparatorOrEnd('}', has_next)) {
            return false;
          }
          if (!has_next) {
            break;
          }
        }
      }
      RETURN_FALSE_IF_PARSE_ERROR(consume('}'));
      return dirty;
    }

    if (c == '[' && index + 1 < path.size() && path[index].is_wildcard() &&
        path[index + 1].is_wildcard()) {
      RETURN_FALSE_IF_PARSE_ERROR(consume('['));
      bool dirty = false;
      if (!jsonGenerator->isBeginArray() && !jsonGenerator->isEmpty()) {
        if (!jsonGenerator->writeComma()) {
          return setJsonGeneratorError(jsonGenerator);
        }
      }
      if (!jsonGenerator->writeStartArray()) {
        return setJsonGeneratorError(jsonGenerator);
      }
      while (peek() != ']') {
        const auto index_plus_two = index + 2;
        dirty |= getJsonPath<FLATTEN, serializeFlags>(
            path, index_plus_two, jsonGenerator, jsonGeneratorFactory);
        if (error_ != kErrorNone) {
          return false;
        }
        bool has_next = false;
        if (!consumeValueSeparatorOrEnd(']', has_next)) {
          return false;
        }
        if (!has_next) {
          break;
        }
      }
      if (!jsonGenerator->writeEndArray()) {
        return setJsonGeneratorError(jsonGenerator);
      }
      RETURN_FALSE_IF_PARSE_ERROR(consume(']'));
      return dirty;
    }

    if (c == '[' && path[index].is_wildcard()) {
      if constexpr (style != QUOTE) {
        int64_t dirty = 0;
        auto constexpr nextStyle = style == RAW ? QUOTE : style;
        WriteBuffer wb;
        auto localJsonGenerator = jsonGeneratorFactory(wb);

        RETURN_FALSE_IF_PARSE_ERROR(consume('['));
        while ((pos_ < len_) && peek() != ']') {
          size_t pos_before = pos_;
          dirty += getJsonPath<nextStyle, serializeFlags>(
                       path, index + 1, localJsonGenerator.get(),
                       jsonGeneratorFactory)
                       ? 1
                       : 0;
          if (pos_ == pos_before) {
            // getJsonPath() must consume at least one value on success/failure.
            // If not, skip the current JSON value to avoid infinite loop and
            // prevent desync by blindly advancing one byte.
            RETURN_FALSE_IF_PARSE_ERROR(skipOne<kJsonPathParseFlags>());
          }
          if (error_ != kErrorNone) {
            return false;
          }
          bool has_next = false;
          if (!consumeValueSeparatorOrEnd(']', has_next)) {
            return false;
          }
          if (!has_next) {
            break;
          }
        }
        if (sonic_unlikely(pos_ == len_)) {
          setError(SonicError::kParseErrorInvalidChar);
          return false;
        }
        RETURN_FALSE_IF_PARSE_ERROR(consume(']'));
        if (dirty > 1) {
          if (!jsonGenerator->isBeginArray() && !jsonGenerator->isEmpty()) {
            if (!jsonGenerator->writeComma()) {
              return setJsonGeneratorError(jsonGenerator);
            }
          }
          if (!jsonGenerator->writeStartArray()) {
            return setJsonGeneratorError(jsonGenerator);
          }
          // should always use explicit `Size`, because there maybe '\0' in the
          // wb
          if (!jsonGenerator->writeRawValue(wb.ToStringView())) {
            return setJsonGeneratorError(jsonGenerator);
          }
          if (!jsonGenerator->writeEndArray()) {
            return setJsonGeneratorError(jsonGenerator);
          }
        } else if (dirty == 1) {
          if (!jsonGenerator->copyCurrentStructureSingleResult(
                  wb.ToStringView())) {
            return setJsonGeneratorError(jsonGenerator);
          }
        }

        return dirty > 0;
      }
    }

    if (c == '[' && path[index].is_wildcard()) {
      bool dirty = false;
      if (!jsonGenerator->isBeginArray() && !jsonGenerator->isEmpty()) {
        if (!jsonGenerator->writeComma()) {
          return setJsonGeneratorError(jsonGenerator);
        }
      }
      if (!jsonGenerator->writeStartArray()) {
        return setJsonGeneratorError(jsonGenerator);
      }
      RETURN_FALSE_IF_PARSE_ERROR(consume('['));
      while (peek() != ']') {
        const auto index_plus_one = index + 1;

        dirty |= getJsonPath<QUOTE, serializeFlags>(
            path, index_plus_one, jsonGenerator, jsonGeneratorFactory);
        if (error_ != kErrorNone) {
          return false;
        }
        bool has_next = false;
        if (!consumeValueSeparatorOrEnd(']', has_next)) {
          return false;
        }
        if (!has_next) {
          break;
        }
      }
      RETURN_FALSE_IF_PARSE_ERROR(consume(']'));
      if (!jsonGenerator->writeEndArray()) {
        return setJsonGeneratorError(jsonGenerator);
      }
      return dirty;
    }

    if (c == '[' && path[index].is_index()) {
      const auto array_index = path[index].index();

      const bool path_has_two_more = index + 2 < path.size();
      if (path_has_two_more && path[index + 1].is_wildcard()) {
        return getJsonPathArrayIndex<QUOTE, serializeFlags>(
            path, index, jsonGenerator, jsonGeneratorFactory, array_index);
      }

      return getJsonPathArrayIndex<style, serializeFlags>(
          path, index, jsonGenerator, jsonGeneratorFactory, array_index);
    }

    if (field_name && path[index].is_key()) {
      const bool found = advanceKey<kJsonPathParseFlags>(path[index].key());
      if (error_ != kErrorNone) {
        return false;
      }

      if (found) {
        // if not null
        if (peek() != 'n') {
          return getJsonPath<style, serializeFlags>(
              path, index + 1, jsonGenerator, jsonGeneratorFactory);
        } else {
          // skip null
          RETURN_FALSE_IF_PARSE_ERROR(skipOne<kJsonPathParseFlags>());
          return false;
        }
      }
      return false;
    }

    if (field_name && path[index].is_wildcard()) {
      RETURN_FALSE_IF_PARSE_ERROR(skipOne<kJsonPathParseFlags>());
      RETURN_FALSE_IF_PARSE_ERROR(consume(':'));
      return getJsonPath<style, serializeFlags>(path, index + 1, jsonGenerator,
                                                jsonGeneratorFactory);
    }

    if (c == '{' || c == '[') {
      if (c == '{') {
        RETURN_FALSE_IF_PARSE_ERROR(consume('{'));
        while (peek() != '}') {
          RETURN_FALSE_IF_PARSE_ERROR(consume('"'));
          if (!scanner_.template SkipStringStrict<kJsonPathParseFlags>(
                  data_, pos_, len_, kbuf_, error_)) {
            if (!hasError()) setError(SonicError::kParseErrorInvalidChar);
            return false;
          }
          RETURN_FALSE_IF_PARSE_ERROR(consume(':'));
          RETURN_FALSE_IF_PARSE_ERROR(skipOne<kJsonPathParseFlags>());
          bool has_next = false;
          if (!consumeValueSeparatorOrEnd('}', has_next)) {
            return false;
          }
          if (!has_next) {
            break;
          }
        }
        RETURN_FALSE_IF_PARSE_ERROR(consume('}'));
      } else if (c == '[') {
        RETURN_FALSE_IF_PARSE_ERROR(consume('['));
        while (peek() != ']') {
          RETURN_FALSE_IF_PARSE_ERROR(skipOne<kJsonPathParseFlags>());
          bool has_next = false;
          if (!consumeValueSeparatorOrEnd(']', has_next)) {
            return false;
          }
          if (!has_next) {
            break;
          }
        }
        RETURN_FALSE_IF_PARSE_ERROR(consume(']'));
      }
    }
    return false;
  }
  // SkipOne skip one raw json value and return the start of value, return the
  // negative if errors.
  inline SonicError getJsonPath(const JsonPath& path, size_t index,
                                std::vector<StringView>& res,
                                bool complete = false) {
    if (index >= path.size()) {
      res.push_back(getOne<kJsonPathParseFlags>());
      return error_;
    }

    auto c = advance();
    if (path[index].is_wildcard()) {
      if (c == '{') {
        return traverseObject(path, index, res);
      } else if (c == '[') {
        return traverseArray(path, index, res);
      } else {
        // wildcard do nothing when meets the primitive value
        return kErrorNone;
      }
    }

    if (path[index].is_key()) {
      if (c != '{') {
        if (complete) {
          pos_--;
          skipOne<kJsonPathParseFlags>();
        } else {
          setError(SonicError::kUnmatchedTypeInJsonPath);
        }
        return error_;
      }

      bool found = advanceKey<kJsonPathParseFlags>(path[index].key());
      if (hasError()) {
        return error_;
      }

      if (!found) {
        return complete ? kErrorNone : kParseErrorUnknownObjKey;
      }

      error_ = getJsonPath(path, index + 1, res, complete);
      if (hasError()) {
        return error_;
      }
      return complete ? skipObjectRemain() : kErrorNone;
    }

    if (path[index].is_index()) {
      if (c != '[') {
        if (complete) {
          pos_--;
          skipOne<kJsonPathParseFlags>();
        } else {
          setError(SonicError::kUnmatchedTypeInJsonPath);
        }
        return error_;
      }

      // index maybe negative
      int64_t idx = path[index].index();
      if (idx < 0) {
        setError(SonicError::kUnsupportedJsonPath);
        return error_;
      }

      bool found = advanceIndex(path[index].index());
      if (hasError()) {
        return error_;
      }

      if (found) {
        error_ = getJsonPath(path, index + 1, res, complete);
        if (!hasError() && complete) {
          return skipArrayRemain();
        }
        return error_;
      }

      // not found the index
      if (!complete) {
        return kParseErrorArrIndexOutOfRange;
      }
      return kErrorNone;
    }
    return kUnsupportedJsonPath;
  }

 public:
  SkipScanner scanner_;
  const uint8_t* data_ = nullptr;
  size_t pos_ = 0;
  size_t len_ = 0;
  SonicError error_ = SonicError::kErrorNone;
  Stack kbuf_{32};
  bool isFieldName = false;
};
}  // namespace internal
}  // namespace sonic_json

#undef RETURN_FALSE_IF_PARSE_ERROR
