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

#include <climits>
#include <cstring>
#include <vector>

#include "sonic/dom/flags.h"
#include "sonic/dom/handler.h"
#include "sonic/dom/json_pointer.h"
#include "sonic/error.h"
#include "sonic/internal/atof_native.h"
#include "sonic/internal/haswell.h"
#include "sonic/internal/parse_number_normal_fast.h"
#include "sonic/internal/simd_str2int.h"
#include "sonic/internal/skip.h"
#include "sonic/internal/unicode.h"
#include "sonic/writebuffer.h"

namespace sonic_json {

class Parser {
 public:
  explicit Parser() noexcept = default;
  // sonic_force_inline Parser(JsonInput& input) : input_(input) {}
  Parser(Parser &&other) noexcept = default;
  sonic_force_inline Parser(const Parser &other) = delete;
  sonic_force_inline Parser &operator=(const Parser &other) = delete;
  sonic_force_inline Parser &operator=(Parser &&other) noexcept = default;
  ~Parser() noexcept = default;

  template <unsigned parseFlags = kParseDefault, typename SAX>
  sonic_force_inline ParseResult Parse(char *data, size_t len, SAX &sax) {
    reset();
    json_buf_ = reinterpret_cast<uint8_t *>(data);
    len_ = len;
    parseImpl<parseFlags>(sax);
    if (!err_ && hasTrailingChars()) {
      err_ = kParseErrorInvalidChar;
    }
    return ParseResult{err_, static_cast<size_t>(pos_)};
  }

  // parseLazyImpl only mark the json positions, and not parse any more, even
  // the keys.
  template <typename LazySAX>
  sonic_force_inline ParseResult ParseLazy(const uint8_t *data, size_t len,
                                           LazySAX &sax) {
    return parseLazyImpl(data, len, sax);
  }

 private:
  sonic_force_inline bool hasTrailingChars() {
    while (pos_ < len_) {
      if (!internal::IsSpace(json_buf_[pos_])) return true;
      pos_++;
    }
    return false;
  }

  sonic_force_inline void setParseError(SonicError err) { err_ = err; }

  template <typename SAX>
  sonic_force_inline void parseNull(SAX &sax) {
    const static uint32_t kNullBin = 0x6c6c756e;
    if (internal::EqBytes4(json_buf_ + pos_ - 1, kNullBin) && sax.Null()) {
      pos_ += 3;
      return;
    }
    setParseError(kParseErrorInvalidChar);
  }

  template <typename SAX>
  sonic_force_inline void parseFalse(SAX &sax) {
    const static uint32_t kFalseBin =
        0x65736c61;  // the binary of 'alse' in false
    if (internal::EqBytes4(json_buf_ + pos_, kFalseBin) && sax.Bool(false)) {
      pos_ += 4;
      return;
    }
    setParseError(kParseErrorInvalidChar);
  }

  template <typename SAX>
  sonic_force_inline void parseTrue(SAX &sax) {
    constexpr static uint32_t kTrueBin = 0x65757274;
    if (internal::EqBytes4(json_buf_ + pos_ - 1, kTrueBin) && sax.Bool(true)) {
      pos_ += 3;
      return;
    }
    setParseError(kParseErrorInvalidChar);
  }

  template <typename SAX>
  sonic_force_inline void parseStrInPlace(SAX &sax) {
    uint8_t *src = json_buf_ + pos_;
    uint8_t *sdst = src;
    size_t n = internal::parseStringInplace(src, err_);
    pos_ = src - json_buf_;
    if (!sax.String(StringView(reinterpret_cast<char *>(sdst), n))) {
      setParseError(kParseErrorInvalidChar);
      return;
    }
    return;
  }

  sonic_force_inline bool carry_one(char c, uint64_t &sum) const {
    uint8_t d = static_cast<uint8_t>(c - '0');
    if (d > 9) {
      return false;
    }
    sum = sum * 10 + d;
    return true;
  }

  sonic_force_inline uint64_t str2int(const char *s, size_t &i) const {
    uint64_t sum = 0;
    while (carry_one(s[i], sum)) {
      i++;
    }
    return sum;
  }

  sonic_force_inline bool parseFloatingFast(double &d, int exp10,
                                            uint64_t man) const {
    d = (double)man;
    // if man is small, but exp is large, also can parse excactly
    if (exp10 > 0) {
      if (exp10 > 22) {
        d *= internal::kPow10Tab[exp10 - 22];
        if (d > 1e15 || d < -1e15) {
          // the exponent is tooo large
          return false;
        }
        d *= internal::kPow10Tab[22];
        return true;
      }
      d *= internal::kPow10Tab[exp10];
      return true;
    } else {
      d /= internal::kPow10Tab[-exp10];
      return true;
    }
    return false;
  }

  SonicError parseFloatEiselLemire64(double &dbl, int exp10, uint64_t man,
                                     int sgn, bool trunc, const char *s) const {
    union {
      double val = 0;
      uint64_t uval;
    } d;
    double val_up = 0;
    if (internal::AtofEiselLemire64(man, exp10, sgn, &d.val)) {
      if (!trunc) {
        dbl = d.val;
        return kErrorNone;
      }
      if (internal::AtofEiselLemire64(man + 1, exp10, sgn, &val_up) &&
          val_up == d.val) {
        dbl = d.val;
        return kErrorNone;
      }
    }

    d.val = internal::AtofNative(s + pos_ - 1, len_ - pos_ + 1);
    dbl = d.val;
    /* if the float number is infinity */
    if (sonic_unlikely((d.uval << 1) == 0xFFE0000000000000)) {
      return kParseErrorInfinity;
    } else {
      return kErrorNone;
    }
  }

  template <typename SAX>
  sonic_force_inline void parseNumber(SAX &sax) {
#define FLOATING_LONGEST_DIGITS 17
#define RETURN_SET_ERROR_CODE(error_code) \
  {                                       \
    pos_ = i;                             \
    err_ = error_code;                    \
    return;                               \
  }
#define CHECK_DIGIT()                              \
  if (sonic_unlikely(s[i] < '0' || s[i] > '9')) {  \
    RETURN_SET_ERROR_CODE(kParseErrorInvalidChar); \
  }

#define SET_INT_AND_RETURN(int_val)                                       \
  {                                                                       \
    if (!sax.Int(int_val)) RETURN_SET_ERROR_CODE(kParseErrorInvalidChar); \
    RETURN_SET_ERROR_CODE(kErrorNone);                                    \
  }

#define SET_UINT_AND_RETURN(int_val)                                       \
  {                                                                        \
    if (!sax.Uint(int_val)) RETURN_SET_ERROR_CODE(kParseErrorInvalidChar); \
    RETURN_SET_ERROR_CODE(kErrorNone);                                     \
  }
#define SET_DOUBLE_AND_RETURN(dbl)                                       \
  {                                                                      \
    if (!sax.Double(dbl)) RETURN_SET_ERROR_CODE(kParseErrorInvalidChar); \
    RETURN_SET_ERROR_CODE(kErrorNone);                                   \
  }
#define SET_U64_AS_DOUBLE_AND_RETURN(int_val)                             \
  {                                                                       \
    union {                                                               \
      double d;                                                           \
      uint64_t u;                                                         \
    } du;                                                                 \
    du.u = int_val;                                                       \
    if (!sax.Double(du.d)) RETURN_SET_ERROR_CODE(kParseErrorInvalidChar); \
    RETURN_SET_ERROR_CODE(kErrorNone);                                    \
  }

    static constexpr uint64_t kUint64Max = 0xFFFFFFFFFFFFFFFF;
    int sgn = -1;
    int man_nd = 0;  // # digits of mantissa, 10 ^ 19 fits uint64_t
    int exp10 = 0;   // 10-based exponet of float point number
    int trunc = 0;
    uint64_t man = 0;  // mantissa of float point number
    size_t i = pos_ - 1;
    size_t exp10_s = i;
    const char *s = reinterpret_cast<const char *>(json_buf_);
    using internal::is_digit;

    /* check sign */
    {
      bool neg = (s[i] == '-');
      i += uint8_t(neg);
      sgn = neg ? -1 : 1;
    }

    /* check leading zero */
    if (s[i] == '0') {
      i++;
      if (sonic_likely(s[i] == '.')) {
        i++;
        CHECK_DIGIT();
        exp10_s = i;
        while (s[i] == '0') {
          i++;
        }
        if (sonic_unlikely(s[i] == 'e' || s[i] == 'E')) {
          i++;
          if (s[i] == '-' || s[i] == '+') i++;
          CHECK_DIGIT();
          while (is_digit(s[i])) {
            i++;
          }
          SET_DOUBLE_AND_RETURN(0.0 * sgn);
        }
        goto double_fract;
      } else if (sonic_unlikely(s[i] == 'e' || s[i] == 'E')) {
        i++;
        if (s[i] == '-' || s[i] == '+') i++;
        CHECK_DIGIT();
        while (is_digit(s[i])) {
          i++;
        }
        SET_DOUBLE_AND_RETURN(0.0 * sgn);
      }

      SET_UINT_AND_RETURN(0);
    }

    // CHECK_DIGIT();
    {
      size_t digit_start = i;
      man = str2int(s, i);
      man_nd = i - digit_start;
      if (man_nd == 0) {
        RETURN_SET_ERROR_CODE(kParseErrorInvalidChar);
      }
      if (man_nd > 19) {  // slow path
        i = digit_start;
        man = 0;
        man_nd = 0;
        while (is_digit(s[i])) {
          if (man_nd < 19) {
            man = man * 10 + s[i] - '0';
            man_nd++;
          } else {
            exp10++;
          }
          i++;
        }
      }
    }

    if (sonic_likely(s[i] == '.')) {
      i++;
      CHECK_DIGIT();
      exp10_s = i;
      goto double_fract;
    }
    if (sonic_unlikely(s[i] == 'e' || s[i] == 'E')) goto double_exp;

    // Integer
    if (exp10 == 0) {
      // less than or equal to 19 digits
      if (sgn == -1) {
        if (man > ((uint64_t)1 << 63)) {
          // overflow signed integer
          // Assume compiler supports convert uint64 to double
          SET_DOUBLE_AND_RETURN(-(double)(man));
        } else {
          SET_INT_AND_RETURN(-man);
        }
      } else {
        SET_UINT_AND_RETURN(man);
      }
    } else if (exp10 == 1) {
      // now we get 20 digits, it maybe overflow for uint64
      unsigned num = s[i - 1] - '0';
      if (man < kUint64Max / 10 ||
          (man == kUint64Max / 10 && num <= UINT_MAX % 10)) {
        man = man * 10 + num;
        if (sgn == -1) {
          SET_DOUBLE_AND_RETURN(-(double)(man));
        } else {
          SET_UINT_AND_RETURN(man);
        }
      } else {
        trunc = 1;
        goto double_fast;
      }
    } else {
      trunc = 1;
      goto double_fast;
    }

    // Is error when run here
    // TODO: Assert

  double_fract : {
    int fract_len = FLOATING_LONGEST_DIGITS - man_nd;
    if (fract_len > 0) {
      uint64_t sum = internal::simd_str2int_sse(s + i, fract_len);
      const uint64_t pow10[17] = {1,
                                  10,
                                  100,
                                  1000,
                                  10000,
                                  100000,
                                  1000000,
                                  10000000,
                                  100000000,
                                  1000000000,
                                  10000000000,
                                  100000000000,
                                  1000000000000,
                                  10000000000000,
                                  100000000000000,
                                  1000000000000000,
                                  10000000000000000};
      man = man * pow10[fract_len] + sum;
      man_nd += fract_len;
      i += fract_len;
      while (man_nd < FLOATING_LONGEST_DIGITS && is_digit(s[i])) {
        man = man * 10 + s[i] - '0';
        man_nd++;
        i++;
      }
    }
  }

    exp10 -= (i - exp10_s);

    while (is_digit(s[i])) {
      trunc = 1;
      i++;
    }

    if (sonic_likely(s[i] != 'e' && s[i] != 'E')) {
      goto double_fast;
    }

  double_exp : {
    int esm = 1;
    int exp = 0;

    /* check for the '+' or '-' sign, and decode the power */
    i++;
    if (s[i] == '-' || s[i] == '+') {
      esm = s[i] == '-' ? -1 : 1;
      i++;
    }
    CHECK_DIGIT();

    while (is_digit(s[i])) {
      if (sonic_likely(exp < 10000)) {
        exp = exp * 10 + (s[i] - '0');
      }
      i++;
    }
    exp10 += exp * esm;
  }

  double_fast : {
    // double floating can represent the val exactly
    double dbl;
    if ((man >> 52) == 0 && exp10 <= (22 + 15) && exp10 >= -22) {
      if (parseFloatingFast(dbl, exp10, man)) {
        SET_DOUBLE_AND_RETURN(dbl * sgn);
      }  // else goto double_fasax.st_normal;
    }
  }

    if (!trunc && exp10 > -308 + 1 && exp10 < +308 - 20) {
      uint64_t raw;
      if (internal::ParseFloatingNormalFast(raw, exp10, man, sgn)) {
        SET_U64_AS_DOUBLE_AND_RETURN(raw);
      }
    }
    {
      double d;
      SonicError error_code =
          parseFloatEiselLemire64(d, exp10, man, sgn, trunc, s);
      if (!sax.Double(d)) RETURN_SET_ERROR_CODE(kParseErrorInvalidChar);
      ;
      RETURN_SET_ERROR_CODE(error_code);
    }

    return;

#undef CHECK_DIGIT
  }

  template <typename SAX>
  void parsePrimitives(SAX &sax) {
    switch (json_buf_[pos_ - 1]) {
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
      case '-':
        parseNumber(sax);
        break;
      case '"':
        parseStrInPlace(sax);
        break;
      case 'f':
        parseFalse(sax);
        break;
      case 't':
        parseTrue(sax);
        break;
      case 'n':
        parseNull(sax);
        break;
      default:
        setParseError(kParseErrorInvalidChar);
    }
  }

  template <unsigned parseFlags, typename SAX>
  sonic_force_inline void parseImpl(SAX &sax) {
#define sonic_check_err()   \
  if (err_ != kErrorNone) { \
    goto err_invalid_char;  \
  }

    using namespace sonic_json::internal;
    // TODO (liuq19): vector is a temporary choice, will optimize in future.
    std::vector<uint32_t> depth;
    const uint32_t kArrMask = 1ull << 31;
    const uint32_t kObjMask = 0;

    uint8_t c = scan.SkipSpace(json_buf_, pos_);
    switch (c) {
      case '[': {
        sax.StartArray();
        depth.push_back(kArrMask);
        c = scan.SkipSpace(json_buf_, pos_);
        if (c == ']') {
          sax.EndArray(0);
          goto scope_end;
        }
        goto arr_val;
      };
      case '{': {
        sax.StartObject();
        depth.push_back(kObjMask);
        c = scan.SkipSpace(json_buf_, pos_);
        if (c == '}') {
          sax.EndObject(0);
          goto scope_end;
        }
        goto obj_key;
      }
      default:
        parsePrimitives(sax);
        goto doc_end;
    };

  obj_key:
    if (sonic_unlikely(c != '"')) goto err_invalid_char;
    parseStrInPlace(sax);
    sonic_check_err();
    c = scan.SkipSpace(json_buf_, pos_);
    if (sonic_unlikely(c != ':')) goto err_invalid_char;

    c = scan.SkipSpace(json_buf_, pos_);
    switch (c) {
      case '{': {
        sax.StartObject();
        depth.push_back(kObjMask);
        c = scan.SkipSpace(json_buf_, pos_);
        if (c == '}') {
          sax.EndObject(0);
          goto scope_end;
        }
        goto obj_key;
      }
      case '[': {
        sax.StartArray();
        depth.push_back(kArrMask);
        c = scan.SkipSpace(json_buf_, pos_);
        if (c == ']') {
          sax.EndArray(0);
          goto scope_end;
        }
        goto arr_val;
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
      case '-':
        parseNumber(sax);
        sonic_check_err();
        break;
      case 't':
        parseTrue(sax);
        sonic_check_err();
        break;
      case 'f':
        parseFalse(sax);
        sonic_check_err();
        break;
      case 'n':
        parseNull(sax);
        sonic_check_err();
        break;
      case '"':
        parseStrInPlace(sax);
        sonic_check_err();
        break;
      default:
        goto err_invalid_char;
    }
    c = scan.SkipSpace(json_buf_, pos_);

  obj_cont:
    depth.back()++;
    if (c == ',') {
      c = scan.SkipSpace(json_buf_, pos_);
      goto obj_key;
    }
    if (sonic_unlikely(c != '}')) {
      goto err_invalid_char;
    }
    sax.EndObject(depth.back());

  scope_end:
    sonic_check_err();
    depth.pop_back();
    if (sonic_unlikely(depth.empty())) {
      goto doc_end;
    }
    c = scan.SkipSpace(json_buf_, pos_);
    if (depth.back() & kArrMask) {
      goto arr_cont;
    }
    goto obj_cont;

  arr_val:
    switch (c) {
      case '{': {
        sax.StartObject();
        depth.push_back(kObjMask);
        c = scan.SkipSpace(json_buf_, pos_);
        if (c == '}') {
          sax.EndObject(0);
          goto scope_end;
        }
        goto obj_key;
      }
      case '[': {
        sax.StartArray();
        depth.push_back(kArrMask);
        c = scan.SkipSpace(json_buf_, pos_);
        if (c == ']') {
          sax.EndArray(0);
          goto scope_end;
        }
        goto arr_val;
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
      case '-':
        parseNumber(sax);
        sonic_check_err();
        break;
      case 't':
        parseTrue(sax);
        sonic_check_err();
        break;
      case 'f':
        parseFalse(sax);
        sonic_check_err();
        break;
      case 'n':
        parseNull(sax);
        sonic_check_err();
        break;
      case '"':
        parseStrInPlace(sax);
        sonic_check_err();
        break;
      default:
        goto err_invalid_char;
    }
    c = scan.SkipSpace(json_buf_, pos_);

  arr_cont:
    depth.back()++;
    if (c == ',') {
      c = scan.SkipSpace(json_buf_, pos_);
      goto arr_val;
    }
    if (sonic_likely(c == ']')) {
      sax.EndArray(depth.back() & (kArrMask - 1));
      goto scope_end;
    }
    goto err_invalid_char;

  doc_end:
    return;
  err_invalid_char:
    err_ = kParseErrorInvalidChar;
    return;
  }

  // parseLazyImpl only mark the json positions, and not parse any more, even
  // the keys.
  template <typename LazySAX>
  sonic_force_inline ParseResult parseLazyImpl(const uint8_t *data, size_t len,
                                               LazySAX &sax) {
    using Allocator = typename LazySAX::Allocator;
    size_t pos = 0;
    size_t cnt = 0;
    uint8_t c = scan.SkipSpaceSafe(data, pos, len);
    long start = 0;
    StringView key;
    SonicError err = kErrorNone;
    auto alloc = sax.GetAllocator();
    bool allocated = false;
    int skips = 0;
    size_t sn = 0;
    const uint8_t *src, *sdst;

    switch (c) {
      case '[': {
        sax.StartArray();
        c = scan.SkipSpaceSafe(data, pos, len);
        if (c == ']') {
          sax.EndArray(0);
          return kErrorNone;
        }
        pos--;
        goto arr_val;
      };
      case '{': {
        sax.StartObject();
        c = scan.SkipSpaceSafe(data, pos, len);
        if (c == '}') {
          sax.EndObject(0);
          return kErrorNone;
        }
        goto obj_key;
      }
      default: {
        // TODO: fix the abstract.
        pos--;
        start = scan.SkipOne(data, pos, len);
        if (start < 0) goto skip_error;
        sax.Raw(reinterpret_cast<const char *>(data + start), pos - start);
        return kErrorNone;
      }
    };

  obj_key:
    if (sonic_unlikely(c != '"')) {
      goto err_invalid_char;
    }
    // parse string in allocater if has esacped chars
    src = data + pos;
    sdst = src;
    skips = internal::SkipString(data, pos, len);
    sn = data + pos - 1 - src;
    allocated = false;
    if (!skips) {
      return kParseErrorInvalidChar;
    }
    if (skips == 2) {
      // parse escaped strings
      uint8_t *dst = (uint8_t *)alloc.Malloc(sn + 32);
      sdst = dst;
      std::memcpy(dst, src, sn);
      sn = internal::parseStringInplace(dst, err);
      if (err) {
        // update the error positions
        pos = (src - data) + (dst - sdst);
        Allocator::Free((void *)(sdst));
        return err;
      }
      allocated = true;
    }
    key = StringView(reinterpret_cast<const char *>(sdst), sn);
    if (!sax.Key(key.data(), key.size(), allocated)) {
      goto err_invalid_char;
    }
    c = scan.SkipSpaceSafe(data, pos, len);
    if (sonic_unlikely(c != ':')) {
      goto err_invalid_char;
    }
    start = scan.SkipOne(data, pos, len);
    if (start < 0) goto skip_error;
    sax.Raw(reinterpret_cast<const char *>(data + start), pos - start);
    cnt++;
    c = scan.SkipSpaceSafe(data, pos, len);
    if (c == ',') {
      c = scan.SkipSpaceSafe(data, pos, len);
      goto obj_key;
    }
    if (sonic_unlikely(c != '}')) {
      goto err_invalid_char;
    }
    sax.EndObject(cnt);
    return kErrorNone;

  arr_val:
    start = scan.SkipOne(data, pos, len);
    if (start < 0) goto skip_error;
    sax.Raw(reinterpret_cast<const char *>(data + start), pos - start);
    cnt++;
    c = scan.SkipSpaceSafe(data, pos, len);
    if (c == ',') {
      goto arr_val;
    }
    if (sonic_unlikely(c != ']')) {
      goto err_invalid_char;
    }
    sax.EndArray(cnt);
    return kErrorNone;

  err_invalid_char:
    return ParseResult(kParseErrorInvalidChar, pos - 1);
  skip_error:
    return ParseResult(SonicError(-start), pos - 1);
  }

#undef sonic_check_err

 private:
  sonic_force_inline void reset() {
    pos_ = 0;
    err_ = kErrorNone;
    len_ = 0;
  };
  constexpr static size_t kJsonPaddingSize = SONICJSON_PADDING;

  uint8_t *json_buf_{nullptr};
  size_t len_{0};
  size_t pos_{0};
  SonicError err_{kErrorNone};
  internal::SkipScanner scan{};
};

}  // namespace sonic_json
