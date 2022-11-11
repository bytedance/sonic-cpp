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

#include "sonic/allocator.h"
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

template <typename NodeType>
class GenericDocument;

static sonic_force_inline bool EqBytes4(const uint8_t* src, uint32_t target) {
  uint32_t val;
  static_assert(sizeof(uint32_t) <= SONICJSON_PADDING,
                "SONICJSON_PADDING must be larger than 4 bytes");
  std::memcpy(&val, src, sizeof(uint32_t));
  return val == target;
}

template <typename NodeType>
class Parser {
 public:
  using DomType = GenericDocument<NodeType>;
  using Allocator = typename DomType::Allocator;

  explicit Parser() noexcept = default;
  Parser(Parser&& other) noexcept = default;
  sonic_force_inline Parser(const Parser& other) = delete;
  sonic_force_inline Parser& operator=(const Parser& other) = delete;
  sonic_force_inline Parser& operator=(Parser&& other) noexcept = default;
  ~Parser() noexcept = default;

  template <unsigned parseFlags = kParseDefault>
  ParseResult Parse(const char* data, size_t len, DomType& doc) {
    clear();
    // Allocate the string buffer
    size_t pad_len = len + kJsonPaddingSize;
    json_buf_ = reinterpret_cast<uint8_t*>(doc.GetAllocator().Malloc(pad_len));
    if (json_buf_ == nullptr) {
      return kErrorNoMem;
    }
    std::memcpy(json_buf_, data, len);
    // Add ending mask to support parsing invalid json
    json_buf_[len] = 'x';
    json_buf_[len + 1] = '"';
    json_buf_[len + 2] = 'x';
    len_ = len;

    // NOTE: even though allocated buffer is not wholly satisfied for some
    // extreme case as([[[[[...), we also judge the bound in parse implement.
    // size_t cap = len_ / 2 + 2;
    size_t cap = len_ / 2 + 2;
    if (cap < kefaultNodeCapacity) cap = kefaultNodeCapacity;
    if (!st_ || cap_ < cap) {
      st_ = static_cast<NodeType*>(
          std::realloc(static_cast<void*>(st_), sizeof(NodeType) * cap));
      if (!st_) {
        err_ = kErrorNoMem;
        goto parse_err;
      }
      cap_ = cap;
    }

    parseImpl<parseFlags>(doc);
    // check trailing non-space chars
    if (err_) goto parse_err;
    while (pos_ != len_) {
      if (!internal::IsSpace(json_buf_[pos_])) {
        err_ = kParseErrorInvalidChar;
        goto parse_err;
      }
      pos_++;
    }

    // Assign to document
    doc.copyToRoot(st_[0]);
    doc.str_ = reinterpret_cast<char*>(json_buf_);
    json_buf_ = nullptr;
    std::free(st_);
    st_ = nullptr;
    return ParseResult{err_, static_cast<size_t>(pos_)};

  parse_err:
    Allocator::Free(json_buf_);
    for (size_t i = 0; i < np_; i++) {
      st_[i].~NodeType();
    }
    std::free(st_);
    st_ = nullptr;
    return ParseResult{err_, static_cast<size_t>(pos_)};
  }

  // if path is empty, it will parse all nodes.
  template <unsigned parseFlags = kParseDefault,
            typename JPStringType = SONIC_JSON_POINTER_NODE_STRING_DEFAULT_TYPE>
  ParseResult ParseOnDemand(const char* data, size_t len, DomType& doc,
                            const GenericJsonPointer<JPStringType>& path) {
    clear();
    // Allocate the string buffer
    size_t pad_len = len + kJsonPaddingSize;
    json_buf_ = reinterpret_cast<uint8_t*>(doc.GetAllocator().Malloc(pad_len));
    if (json_buf_ == nullptr) {
      return kErrorNoMem;
    }
    std::memcpy(json_buf_, data, len);
    // Add ending mask to support parsing invalid json
    std::memset(json_buf_ + len, 'x', pad_len - len);
    json_buf_[len + 1] = '"';
    len_ = len;

    // NOTE: even though allocated buffer is not wholly satisfied for some
    // extreme case as([[[[[...), we also judge the bound in parse implement.
    // size_t cap = len_ / 2 + 2;
    size_t cap = len_ / 2 + 2;
    if (cap < kefaultNodeCapacity) cap = kefaultNodeCapacity;
    if (!st_ || cap_ < cap) {
      st_ = static_cast<NodeType*>(
          std::realloc(static_cast<void*>(st_), sizeof(NodeType) * cap));
      if (!st_) {
        err_ = kErrorNoMem;
        goto parse_err;
      }
      cap_ = cap;
    }
    parseOnDemandImpl<parseFlags, JPStringType>(doc, path);
    if (err_) goto parse_err;
    // Assign to document
    doc.copyToRoot(st_[0]);
    doc.str_ = reinterpret_cast<char*>(json_buf_);
    json_buf_ = nullptr;
    std::free(st_);
    st_ = nullptr;
    return ParseResult{err_, static_cast<size_t>(pos_)};

  parse_err:

    Allocator::Free(json_buf_);
    for (size_t i = 0; i < np_; i++) {
      st_[i].~NodeType();
    }
    std::free(st_);
    st_ = nullptr;
    return ParseResult{err_, static_cast<size_t>(pos_)};
  }

 private:
#define sonic_check_err()   \
  if (err_ != kErrorNone) { \
    return;                 \
  }

  sonic_force_inline void setParseError(SonicError err) { err_ = err; }

  sonic_force_inline void parseNull(NodeType& node) {
    const static uint32_t kNullBin = 0x6c6c756e;
    if (EqBytes4(json_buf_ + pos_ - 1, kNullBin)) {
      JsonHandler<NodeType>::SetNull(node);
      pos_ += 3;
      return;
    }
    setParseError(kParseErrorInvalidChar);
  }

  sonic_force_inline void parseFalse(NodeType& node) {
    const static uint32_t kFalseBin =
        0x65736c61;  // the binary of 'alse' in false
    if (EqBytes4(json_buf_ + pos_, kFalseBin)) {
      JsonHandler<NodeType>::SetBool(node, false);
      pos_ += 4;
      return;
    }
    setParseError(kParseErrorInvalidChar);
  }

  sonic_force_inline void parseTrue(NodeType& node) {
    constexpr static uint32_t kTrueBin = 0x65757274;
    if (EqBytes4(json_buf_ + pos_ - 1, kTrueBin)) {
      JsonHandler<NodeType>::SetBool(node, true);
      pos_ += 3;
      return;
    }
    setParseError(kParseErrorInvalidChar);
  }

  sonic_force_inline void parseString(NodeType& node) {
#define SONIC_REPEAT8(v) \
  { v v v v v v v v }
    using internal::StringBlock;
    uint8_t* src = json_buf_ + pos_;
    uint8_t* dst = src;
    const char* sdst = reinterpret_cast<const char*>(dst);

    while (1) {
    find:
      auto block = StringBlock::Find(src);
      if (block.HasQuoteFirst()) {
        int idx = block.QuoteIndex();
        src[idx] = '\0';
        pos_ = src - json_buf_ + idx + 1;
        JsonHandler<NodeType>::SetString(
            node, sdst, reinterpret_cast<const char*>(src + idx) - sdst);
        return;
      }
      if (block.HasUnescaped()) {
        setParseError(kParseErrorUnEscaped);
        return;
      }
      if (!block.HasBackslash()) {
        src += 32;
        goto find;
      }

      /* find out where the backspace is */
      auto bs_dist = block.BsIndex();
      src += bs_dist;
      dst = src;
    cont:
      uint8_t escape_char = src[1];
      if (sonic_unlikely(escape_char == 'u')) {
        if (!internal::handle_unicode_codepoint(
                const_cast<const uint8_t**>(&src), &dst)) {
          pos_ = src - json_buf_;
          setParseError(kParseErrorEscapedUnicode);
          return;
        }
      } else {
        *dst = internal::kEscapedMap[escape_char];
        if (sonic_unlikely(*dst == 0u)) {
          pos_ = src - json_buf_;
          setParseError(kParseErrorEscapedFormat);
          return;
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
      internal::simd256<uint8_t> v(src);
      block = StringBlock{
          static_cast<uint32_t>((v == '\\').to_bitmask()),  // bs_bits
          static_cast<uint32_t>((v == '"').to_bitmask()),   // quote_bits
          static_cast<uint32_t>((v <= '\x1f').to_bitmask()),
      };
      // If the next thing is the end quote, copy and return
      if (block.HasQuoteFirst()) {
        // we encountered quotes first. Move dst to point to quotes and exit
        while (1) {
          SONIC_REPEAT8(if (sonic_unlikely(*src == '"')) break;
                        else { *dst++ = *src++; });
        }
        *dst = '\0';
        pos_ = src - json_buf_ + 1;
        JsonHandler<NodeType>::SetString(
            node, sdst, reinterpret_cast<const char*>(dst) - sdst);
        return;
      }
      if (block.HasUnescaped()) {
        setParseError(kParseErrorUnEscaped);
        return;
      }
      if (!block.HasBackslash()) {
        /* they are the same. Since they can't co-occur, it means we
         * encountered neither. */
        v.store(dst);
        src += 32;
        dst += 32;
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

  sonic_force_inline bool carry_one(char c, uint64_t& sum) const {
    uint8_t d = static_cast<uint8_t>(c - '0');
    if (d > 9) {
      return false;
    }
    sum = sum * 10 + d;
    return true;
  }

  sonic_force_inline uint64_t str2int(const char* s, size_t& i) const {
    uint64_t sum = 0;
    while (carry_one(s[i], sum)) {
      i++;
    }
    return sum;
  }

  sonic_force_inline bool parseFloatingFast(double& d, int exp10,
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

  SonicError parseFloatEiselLemire64(double& dbl, int exp10, uint64_t man,
                                     int sgn, bool trunc, const char* s) const {
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

  sonic_force_inline void parseNumber(NodeType& node) {
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

#define SET_INT_AND_RETURN(int_val)                \
  {                                                \
    JsonHandler<NodeType>::SetSint(node, int_val); \
    RETURN_SET_ERROR_CODE(kErrorNone);             \
  }

#define SET_UINT_AND_RETURN(int_val)               \
  {                                                \
    JsonHandler<NodeType>::SetUint(node, int_val); \
    RETURN_SET_ERROR_CODE(kErrorNone);             \
  }
#define SET_DOUBLE_AND_RETURN(dbl)               \
  {                                              \
    JsonHandler<NodeType>::SetDouble(node, dbl); \
    RETURN_SET_ERROR_CODE(kErrorNone);           \
  }
#define SET_U64_AS_DOUBLE_AND_RETURN(int_val)           \
  {                                                     \
    JsonHandler<NodeType>::SetDoubleU64(node, int_val); \
    RETURN_SET_ERROR_CODE(kErrorNone);                  \
  }

    static constexpr uint64_t kUint64Max = 0xFFFFFFFFFFFFFFFF;
    int sgn = -1;
    int man_nd = 0;  // # digits of mantissa, 10 ^ 19 fits uint64_t
    int exp10 = 0;   // 10-based exponet of float point number
    int trunc = 0;
    uint64_t man = 0;  // mantissa of float point number
    size_t i = pos_ - 1;
    size_t exp10_s = i;
    const char* s = reinterpret_cast<const char*>(json_buf_);
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

  double_fract:
    {
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
      }  // else goto double_fast_normal;
    }
  }

    // double_fast_normal:
    if (!trunc && exp10 > -308 + 1 && exp10 < +308 - 20) {
      uint64_t raw;
      if (internal::ParseFloatingNormalFast(raw, exp10, man, sgn)) {
        SET_U64_AS_DOUBLE_AND_RETURN(raw);
      }
    }  // else { // not exact, goto eisel_lemire64; }
    // double_fast_eisel_lemire64 :
    {
      double d;
      SonicError error_code =
          parseFloatEiselLemire64(d, exp10, man, sgn, trunc, s);
      JsonHandler<NodeType>::SetDouble(node, d);
      RETURN_SET_ERROR_CODE(error_code);
    }

    return;

#undef CHECK_DIGIT
  }

  void parsePrimitives(NodeType& node) {
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
        parseNumber(node);
        break;
      case '"':
        parseString(node);
        break;
      case 'f':
        parseFalse(node);
        break;
      case 't':
        parseTrue(node);
        break;
      case 'n':
        parseNull(node);
        break;
      default:
        setParseError(kParseErrorInvalidChar);
    }
  }

#define sonic_push_arr()                                                  \
  {                                                                       \
    ContainerMeta* cur = reinterpret_cast<ContainerMeta*>(&st_[np_ - 1]); \
    cur->PushCurType(kIsArray);                                           \
    cur->PushParentPos(container);                                        \
    container = np_ - 1;                                                  \
  }

#define sonic_push_obj()                                                  \
  {                                                                       \
    ContainerMeta* cur = reinterpret_cast<ContainerMeta*>(&st_[np_ - 1]); \
    cur->PushCurType(kIsObject);                                          \
    cur->PushParentPos(container);                                        \
    container = np_ - 1;                                                  \
  }

#define sonic_pop_arr()                                                        \
  {                                                                            \
    ContainerMeta* cur = reinterpret_cast<ContainerMeta*>(&st_[container]);    \
    parent = cur->ParentPos();                                                 \
    np_ = JsonHandler<NodeType>::SetArray(st_[container], &st_[container + 1], \
                                          cur->Len(), doc) -                   \
          st_;                                                                 \
    container = parent;                                                        \
  }

#define sonic_pop_obj()                                                     \
  {                                                                         \
    ContainerMeta* cur = reinterpret_cast<ContainerMeta*>(&st_[container]); \
    parent = cur->ParentPos();                                              \
    np_ = JsonHandler<NodeType>::SetObject(                                 \
              st_[container], &st_[container + 1], cur->Len(), doc) -       \
          st_;                                                              \
    container = parent;                                                     \
  }

  template <unsigned parseFlags, typename JPStringType>
  void parseOnDemandImpl(DomType& doc,
                         const GenericJsonPointer<JPStringType>& path) {
    using namespace sonic_json::internal;
    size_t i = 0;
    size_t sn = 0;
    uint8_t c;
    StringView key;

  query:
    if (i++ != path.size()) {
      c = scan.SkipSpace(json_buf_, pos_);
      if (path[i - 1].IsStr()) {
        if (c != '{') goto err_mismatch_type;
        key = StringView(path[i - 1].Data(), path[i - 1].Size());
        c = scan.SkipSpace(json_buf_, pos_);
        goto obj_key;
      } else {
        if (c != '[') goto err_mismatch_type;
        err_ = scan.GetArrayElem(json_buf_, pos_, len_, path[i - 1].GetNum());
        if (err_) return;
        goto query;
      }
    } else {
      parseImpl<parseFlags>(doc);
      return;
    }

  obj_key:
    if (sonic_unlikely(c != '"')) {
      goto err_invalid_char;
    }
  obj_key_parsing:
    // parse key
    parseString(st_[np_]);
    if (err_) return;
    c = scan.SkipSpace(json_buf_, pos_);
    if (c != ':') {
      goto err_invalid_char;
    }
    // match key and skip parsing unneeded fields
    sn = st_[np_].Size();
    if (sn == key.size() &&
        std::memcmp(st_[np_].GetStringView().data(), key.data(), sn) == 0) {
      goto query;
    } else {
      // skip the object elem
      c = scan.SkipSpace(json_buf_, pos_);
      if (c == '"') {
        scan.SkipString(json_buf_, pos_);
        c = scan.SkipSpace(json_buf_, pos_);
        if (c != ',') {
          goto err_unknown_key;
        }
        c = scan.SkipSpace(json_buf_, pos_);
        goto obj_key;
      }
      if (c == '{') {
        scan.SkipObject(json_buf_, pos_, len_);
        c = scan.SkipSpace(json_buf_, pos_);
        if (c != ',') {
          goto err_unknown_key;
        }
        c = scan.SkipSpace(json_buf_, pos_);
        goto obj_key;
      }
      if (c == '[') {
        scan.SkipArray(json_buf_, pos_, len_);
        c = scan.SkipSpace(json_buf_, pos_);
        if (c != ',') {
          goto err_unknown_key;
        }
        c = scan.SkipSpace(json_buf_, pos_);
        goto obj_key;
      }
      if (scan.SkipObjectPrimitives(json_buf_, pos_, len_) !=
          SkipScanner::NextObjKey) {
        goto err_unknown_key;
      }
      goto obj_key_parsing;
    }

  err_mismatch_type:
    err_ = kParseErrorMismatchType;
    return;
  err_unknown_key:
    err_ = kParseErrorUnknownObjKey;
    return;
  err_invalid_char:
    err_ = kParseErrorInvalidChar;
    return;
  }

  template <unsigned parseFlags>
  void parseImpl(DomType& doc) {
#define sonic_add_node()            \
  {                                 \
    if (sonic_likely(np_ < cap_)) { \
      np_++;                        \
    } else {                        \
      goto err_invalid_char;        \
    }                               \
  }

    using namespace sonic_json::internal;
    size_t container = ~0u;  // the postion of container node
    size_t parent = 0;       // the postion of parent container node

    uint8_t c = scan.SkipSpace(json_buf_, pos_);
    sonic_add_node();
    switch (c) {
      case '[': {
        sonic_push_arr();
        c = scan.SkipSpace(json_buf_, pos_);
        if (c == ']') {
          sonic_pop_arr();
          goto scope_end;
        }
        goto arr_val;
      };
      case '{': {
        sonic_push_obj();
        c = scan.SkipSpace(json_buf_, pos_);
        if (c == '}') {
          sonic_pop_obj();
          goto scope_end;
        }
        goto obj_key;
      }
      default:
        parsePrimitives(st_[np_ - 1]);
        goto doc_end;
    };

  obj_key:
    if (sonic_unlikely(c != '"')) goto err_invalid_char;
    sonic_add_node();
    parseString(st_[np_ - 1]);
    sonic_check_err();
    c = scan.SkipSpace(json_buf_, pos_);
    if (sonic_unlikely(c != ':')) goto err_invalid_char;

    c = scan.SkipSpace(json_buf_, pos_);
    sonic_add_node();
    switch (c) {
      case '{': {
        sonic_push_obj();
        c = scan.SkipSpace(json_buf_, pos_);
        if (c == '}') {
          sonic_pop_obj();
          goto scope_end;
        }
        goto obj_key;
      }
      case '[': {
        sonic_push_arr();
        c = scan.SkipSpace(json_buf_, pos_);
        if (c == ']') {
          sonic_pop_arr();
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
        parseNumber(st_[np_ - 1]);
        sonic_check_err();
        break;
      case 't':
        parseTrue(st_[np_ - 1]);
        sonic_check_err();
        break;
      case 'f':
        parseFalse(st_[np_ - 1]);
        sonic_check_err();
        break;
      case 'n':
        parseNull(st_[np_ - 1]);
        sonic_check_err();
        break;
      case '"':
        parseString(st_[np_ - 1]);
        sonic_check_err();
        break;
      default:
        goto err_invalid_char;
    }
    c = scan.SkipSpace(json_buf_, pos_);

  obj_cont:
    reinterpret_cast<ContainerMeta*>(&st_[container])->Increment();
    if (c == ',') {
      c = scan.SkipSpace(json_buf_, pos_);
      goto obj_key;
    }
    if (sonic_unlikely(c != '}')) {
      goto err_invalid_char;
    }
    sonic_pop_obj();

  scope_end:
    sonic_check_err();
    if (sonic_unlikely(container == ~0u)) {
      goto doc_end;
    }
    c = scan.SkipSpace(json_buf_, pos_);
    if (reinterpret_cast<ContainerMeta*>(&st_[container])->Type() ==
        kIsObject) {
      goto obj_cont;
    }
    goto arr_cont;

  arr_val:
    sonic_add_node();
    switch (c) {
      case '{': {
        sonic_push_obj();
        c = scan.SkipSpace(json_buf_, pos_);
        if (c == '}') {
          sonic_pop_obj();
          goto scope_end;
        }
        goto obj_key;
      }
      case '[': {
        sonic_push_arr();
        c = scan.SkipSpace(json_buf_, pos_);
        if (c == ']') {
          sonic_pop_arr();
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
        parseNumber(st_[np_ - 1]);
        sonic_check_err();
        break;
      case 't':
        parseTrue(st_[np_ - 1]);
        sonic_check_err();
        break;
      case 'f':
        parseFalse(st_[np_ - 1]);
        sonic_check_err();
        break;
      case 'n':
        parseNull(st_[np_ - 1]);
        sonic_check_err();
        break;
      case '"':
        parseString(st_[np_ - 1]);
        sonic_check_err();
        break;
      default:
        goto err_invalid_char;
    }
    c = scan.SkipSpace(json_buf_, pos_);

  arr_cont:
    reinterpret_cast<ContainerMeta*>(&st_[container])->Increment();
    if (c == ',') {
      c = scan.SkipSpace(json_buf_, pos_);
      goto arr_val;
    }
    if (sonic_likely(c == ']')) {
      sonic_pop_arr();
      goto scope_end;
    }
    goto err_invalid_char;

  doc_end:
    return;
  err_invalid_char:
    err_ = kParseErrorInvalidChar;
    return;
#undef sonic_add_node
  }

#undef sonic_push_obj
#undef sonic_push_arr
#undef sonic_pop_obj
#undef sonic_pop_arr
#undef sonic_check_err

 private:
  sonic_force_inline void clear() {
    np_ = 0;
    pos_ = 0;
    err_ = kErrorNone;
    len_ = 0;
  };

  enum ContainerTypeFlag {
    kIsRoot = 0,
    kIsObject,
    kIsArray,
  };
  struct ContainerMeta {
    union {
      struct {
        uint8_t t;
        uint8_t _len[7];
      };  // ContainerTypeFlag
      uint64_t len;
    };
    size_t pos;  // position of parent container

    static constexpr uint64_t kContainerTypeMask = 1 << 8;
    static constexpr uint64_t kContainerTypeBits = 8;

    sonic_force_inline void PushCurType(ContainerTypeFlag t) {
      this->len = static_cast<uint64_t>(t);
    }
    sonic_force_inline void PushParentPos(size_t pos) { this->pos = pos; }
    sonic_force_inline void Increment() { this->len += kContainerTypeMask; }
    sonic_force_inline ContainerTypeFlag Type() const {
      return static_cast<ContainerTypeFlag>(this->t);
    }
    sonic_force_inline size_t Len() const {
      return this->len >> kContainerTypeBits;
    }
    sonic_force_inline size_t ParentPos() const { return this->pos; }
  };  // 16 bytes

  static const uint64_t kefaultNodeCapacity = 64;
  constexpr static size_t kJsonPaddingSize = SONICJSON_PADDING;

  // buffer for JSON text with padding
  uint8_t* json_buf_{nullptr};
  size_t len_{0};
  size_t pos_{0};
  SonicError err_{kErrorNone};

  // buffer for node stack
  NodeType* st_{nullptr};
  size_t cap_{0};
  size_t np_{0};

  internal::SkipScanner scan{};
};

}  // namespace sonic_json
