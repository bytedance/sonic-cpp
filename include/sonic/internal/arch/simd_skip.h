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

#include <sys/types.h>

#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

#include "simd_dispatch.h"
#include "sonic/error.h"
#include "sonic/jsonpath/jsonpath.h"

#include INCLUDE_ARCH_FILE(skip.h)

#include <sonic/dom/parser.h>

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

static bool SkipArray(const uint8_t *data, size_t &pos, size_t len) {
  return SkipContainer(data, pos, len, '[', ']');
}

static bool SkipObject(const uint8_t *data, size_t &pos, size_t len) {
  return SkipContainer(data, pos, len, '{', '}');
}

static uint8_t SkipNumber(const uint8_t *data, size_t &pos, size_t len) {
  return GetNextToken(data, pos, len, "]},");
}

// SkipScanner is used to skip space and json values in json text.
class SkipScanner {
 public:
  sonic_force_inline uint8_t SkipSpace(const uint8_t *data, size_t &pos) {
    return skip_space(data, pos, nonspace_bits_end_, nonspace_bits_);
  }

  sonic_force_inline uint8_t SkipSpaceSafe(const uint8_t *data, size_t &pos,
                                           size_t len) {
    return skip_space_safe(data, pos, len, nonspace_bits_end_, nonspace_bits_);
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

  sonic_force_inline bool matchKey(const uint8_t *data, size_t &pos, size_t len,
                                   StringView key, std::vector<uint8_t> &kbuf,
                                   SonicError &err) {
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
      kbuf.resize(slen + 32);
      uint8_t *nsrc = &kbuf[0];

      // parseStringInplace need `"` as the end
      std::memcpy(nsrc, start, slen + 1);
      slen = parseStringInplace(nsrc, err);
      if (err) {
        pos = (start - data) + (nsrc - &kbuf[0]);
        return err;
      }
      start = &kbuf[0];
    }

    // compare the key
    return slen == static_cast<long>(key.size()) &&
           std::memcmp(start, key.data(), slen) == 0;
  }
  sonic_force_inline int matchKeys(const uint8_t *data, size_t &pos, size_t len,
                                   std::vector<StringView> keys,
                                   std::vector<uint8_t> &kbuf,
                                   SonicError &err) {
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
      kbuf.resize(slen + 32);
      uint8_t *nsrc = &kbuf[0];

      // parseStringInplace need `"` as the end
      std::memcpy(nsrc, start, slen + 1);
      slen = parseStringInplace(nsrc, err);
      if (err) {
        pos = (start - data) + (nsrc - &kbuf[0]);
        return err;
      }
      start = &kbuf[0];
    }

    for (size_t i = 0; i < keys.size(); i++) {
      auto const &key = keys[i];
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
  template <typename JPStringType>
  long GetOnDemand(StringView json, size_t &pos,
                   const GenericJsonPointer<JPStringType> &path) {
    using namespace sonic_json::internal;
    size_t i = 0;
    uint8_t c;
    StringView key;
    // TODO: use stack smallvector here.
    std::vector<uint8_t> kbuf(32);  // key buffer for parsed keys
    const uint8_t *data = reinterpret_cast<const uint8_t *>(json.data());
    size_t len = json.size();
    SonicError err = kErrorNone;
    bool matched = false;

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

    matched = matchKey(data, pos, len, key, kbuf, err);
    if (err != kErrorNone) {
      return -err;
    }

    c = SkipSpaceSafe(data, pos, len);
    if (c != ':') {
      goto err_invalid_char;
    }

    // match key and skip parsing unneeded fields
    if (matched) {
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
    return -kParseErrorUnknownObjKey;
  err_invalid_char:
    pos -= 1;
    return -kParseErrorInvalidChar;
  }

 private:
  size_t nonspace_bits_end_{0};
  uint64_t nonspace_bits_{0};
};

class SkipScanner2 {
 public:
  sonic_force_inline StringView getOne() {
    long start = scanner_.SkipOne(data_, pos_, len_);
    if (start < 0) {
      setError(SonicError(-start));
      return "";
    }
    return StringView(reinterpret_cast<const char *>(data_) + start,
                      pos_ - start);
  }

  sonic_force_inline SonicError skipOne() {
    long start = scanner_.SkipOne(data_, pos_, len_);
    if (start < 0) {
      setError(SonicError(-start));
      return error_;
    }
    return SonicError::kErrorNone;
  }

  sonic_force_inline uint8_t peek() {
    auto peek = scanner_.SkipSpaceSafe(data_, pos_, len_);
    pos_ -= 1;
    return peek;
  }

  sonic_force_inline bool hasError() {
    return error_ != SonicError::kErrorNone;
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

  sonic_force_inline void skipIfPresent(const uint8_t c) {
    if (sonic_unlikely(pos_ == len_)) {
      setError(SonicError::kParseErrorEof);
      return;
    }
    if (peek() == c) {
      advance();
    }
  }

  sonic_force_inline bool consume(uint8_t c) {
    auto got = scanner_.SkipSpaceSafe(data_, pos_, len_);
    if (got != c) {
      setError(SonicError::kParseErrorInvalidChar);
      return false;
    }
    return true;
  }

  sonic_force_inline void setError(SonicError err) { error_ = err; }

  // Precondition: calling advance takes input the " of the first fieldname
  // post condition: if found peek() returns first char of the found value
  // if not found, peek() returns }
  sonic_force_inline bool advanceKey(StringView key) {
    auto c = advance();
    bool matched = false;
    while (c != '}') {
      if (c != '"') {
        setError(SonicError::kParseErrorInvalidChar);
        return false;
      }

      // match the key
      matched = scanner_.matchKey(data_, pos_, len_, key, kbuf_, error_);
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

      RETURN_FALSE_IF_PARSE_ERROR(skipOne());

      // get the next key
      c = advance();
      if (c == ',') {
        c = advance();
      } else if (c != '}') {
        setError(SonicError::kParseErrorInvalidChar);
      }
    }

    // The previous loop logic is such that if no match, all children of object
    // are consumed, plus the closing }
    // This does not expectation of getJsonPathSpark, which expects '}' is left
    // unconsumed and will be consumed by calling code that processes the obj.
    if (!matched && c == '}') {
      pos_--;
    }
    return matched;
  }

  // Precondition: calling advance takes input the " of the first fieldname
  // post condition: if found peek() returns first char of the found value
  // if not found, peek() returns }
  sonic_force_inline int advanceKeys(std::vector<StringView> const &keys) {
    auto c = advance();
    int matched = -1;
    while (c != '}') {
      if (c != '"') {
        setError(SonicError::kParseErrorInvalidChar);
        return -1;
      }

      // match the key
      matched = scanner_.matchKeys(data_, pos_, len_, keys, kbuf_, error_);
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

      RETURN_FALSE_IF_PARSE_ERROR(skipOne());

      // get the next key
      c = advance();
      if (c == ',') {
        c = advance();
      } else if (c != '}') {
        setError(SonicError::kParseErrorInvalidChar);
      }
    }

    // The previous loop logic is such that if no match, all children of object
    // are consumed, plus the closing }
    // This does not expectation of getJsonPathSpark, which expects '}' is left
    // unconsumed and will be consumed by calling code that processes the obj.
    if (matched == -1 && c == '}') {
      pos_--;
    }
    return matched;
  }

  sonic_force_inline SonicError traverseObject(const JsonPath &path,
                                               size_t index,
                                               std::vector<StringView> &res) {
    auto c = advance();
    while (c != '}') {
      if (c != '"') {
        setError(SonicError::kParseErrorInvalidChar);
        return error_;
      }

      // skip the key
      if (!SkipString(data_, pos_, len_)) {
        setError(SonicError::kParseErrorInvalidChar);
        return error_;
      }

      if (!consume(':')) {
        return error_;
      }

      // recursively parse the value
      if (getJsonPath(path, index + 1, res, true) != SonicError::kErrorNone) {
        return error_;
      }

      // get the next key
      c = advance();
      if (c == ',') {
        c = advance();
      } else if (c != '}') {
        setError(SonicError::kParseErrorInvalidChar);
        return error_;
      }
    }
    return kErrorNone;
  }

  sonic_force_inline SonicError traverseArray(const JsonPath &path,
                                              size_t index,
                                              std::vector<StringView> &res) {
    auto c = advance();
    pos_--;
    while (c != ']') {
      // recursively parse the value
      if (getJsonPath(path, index + 1, res, true) != SonicError::kErrorNone) {
        return error_;
      }

      // get the next elem
      c = advance();
      if (c == ',') {
        continue;
      } else if (c != ']') {
        setError(SonicError::kParseErrorInvalidChar);
        return error_;
      }
    }
    return kErrorNone;
  }

  sonic_force_inline bool advanceIndex(size_t index) /* found */ {
    auto c = advance();
    if (c == ']') {
      return false;
    }

    pos_--;  // backwared for skip the first elem
    while (c != ']' && index > 0) {
      if (skipOne() != SonicError::kErrorNone) {
        return false;
      }

      // get the next key
      c = advance();
      if (c == ',') {
        index--;
      } else if (c != ']') {
        setError(SonicError::kParseErrorInvalidChar);
        return false;
      }
    }

    return (index == 0);
  }

  sonic_force_inline SonicError skipArrayRemain() {
    auto c = advance();
    while (c != ']') {
      if (c != ',') {
        setError(SonicError::kParseErrorInvalidChar);
        return error_;
      }

      if (skipOne() != SonicError::kErrorNone) {
        return error_;
      }

      c = advance();
    }
    return kErrorNone;
  }

  sonic_force_inline SonicError skipObjectRemain() {
    auto c = advance();
    while (c != '}') {
      if (c != ',') {
        setError(SonicError::kParseErrorInvalidChar);
        return error_;
      }

      c = advance();
      if (c != '"') {
        setError(SonicError::kParseErrorInvalidChar);
        return error_;
      }

      // skip the key
      if (!SkipString(data_, pos_, len_)) {
        setError(SonicError::kParseErrorInvalidChar);
        return error_;
      }

      if (!consume(':')) {
        return error_;
      }

      if (skipOne() != SonicError::kErrorNone) {
        return error_;
      }

      // get the next key
      c = advance();
    }
    return kErrorNone;
  }

  enum WriteStyle { RAW, FLATTEN, QUOTE };
  enum JsonValueType { STRING, OTHER };

  class JsonGeneratorInterface {
   public:
    virtual bool writeRaw(StringView sv) = 0;
    virtual bool copyCurrentStructure(StringView sv) = 0;
    virtual bool copyCurrentStructureJsonTupleCodeGen(
        StringView raw, size_t index,
        std::vector<std::optional<std::string>> &result,
        JsonValueType type) = 0;
    virtual bool writeRawValue(StringView sv) = 0;
    virtual bool writeStartArray() = 0;
    virtual bool writeEndArray() = 0;
    virtual bool writeComma() = 0;
    virtual bool isEmpty() = 0;
    virtual bool isBeginArray() = 0;
    virtual ~JsonGeneratorInterface() {}
  };

  using JsonGeneratorFactory =
      std::function<std::shared_ptr<JsonGeneratorInterface>(WriteBuffer &)>;

  inline bool getJsonPathSparkArrayIndex(
      const JsonPath &path, size_t index, WriteStyle style,
      std::shared_ptr<JsonGeneratorInterface> jsonGenerator,
      JsonGeneratorFactory jsonGeneratorFactory, int64_t const idx) {
    RETURN_FALSE_IF_PARSE_ERROR(consume('['));
    int64_t cur_idx = 0;
    bool dirty = false;
    while (peek() != ']') {
      if (cur_idx == idx) {
        dirty = getJsonPathSpark(path, index + 1, style, jsonGenerator,
                                 jsonGeneratorFactory);
        while (peek() != ']') {
          RETURN_FALSE_IF_PARSE_ERROR(skipIfPresent(','));
          if (peek() == ']') {
            break;
          }
          RETURN_FALSE_IF_PARSE_ERROR(skipOne());
        }
        break;
      } else {
        RETURN_FALSE_IF_PARSE_ERROR(skipOne());
      }
      RETURN_FALSE_IF_PARSE_ERROR(skipIfPresent(','));
      cur_idx++;
    }
    RETURN_FALSE_IF_PARSE_ERROR(consume(']'));
    return dirty;
  }

  inline bool jsonTupleWithCodeGenImpl(
      std::vector<StringView> const &keys,
      std::shared_ptr<JsonGeneratorInterface> jsonGenerator,
      std::vector<std::optional<std::string>> &result) {
    RETURN_FALSE_IF_PARSE_ERROR(consume('{'));

    int todo = keys.size();

    while (peek() != '}' && todo > 0) {
      int keyMatchIndex = advanceKeys(keys);
      if (keyMatchIndex != -1) {
        todo--;
        JsonValueType type =
            peek() == '"' ? JsonValueType::STRING : JsonValueType::OTHER;
        if (peek() == 'n') {
          // do not do anything for null
          RETURN_FALSE_IF_PARSE_ERROR(skipOne());
        } else {
          const auto sv = getOne();
          if (error_ != kErrorNone) {
            return false;
          }
          const auto copy_success =
              jsonGenerator->copyCurrentStructureJsonTupleCodeGen(
                  sv, keyMatchIndex, result, type);
          if (!copy_success) {
            error_ = kParseErrorUnexpect;
            return false;
          }
        }
      }
      RETURN_FALSE_IF_PARSE_ERROR(skipIfPresent(','));
    }

    return true;
  }

  inline std::vector<std::optional<std::string>> jsonTupleWithCodeGen(
      std::vector<StringView> const &keys,
      std::shared_ptr<JsonGeneratorInterface> jsonGenerator, bool legacy) {
    std::vector<std::optional<std::string>> result(keys.size(), std::nullopt);
    const auto success = jsonTupleWithCodeGenImpl(keys, jsonGenerator, result);

    if (!success && !legacy) {
      std::vector<std::optional<std::string>> all_nulls(keys.size(),
                                                        std::nullopt);
      return all_nulls;
    }

    return result;
  }

  inline bool getJsonPathSpark(
      const JsonPath &path, size_t index, WriteStyle style,
      std::shared_ptr<JsonGeneratorInterface> jsonGenerator,
      JsonGeneratorFactory const &jsonGeneratorFactory) {
    const bool path_is_nil = index >= path.size();
    const auto c = peek();
    const bool is_field_name = getAndClearIsFieldName();
    const bool value_string = !is_field_name && c == '"';
    const bool field_name = c == '"' && is_field_name;

    if (is_field_name && !value_string && !field_name) {
      setError(kParseErrorUnexpect);
      return false;
    }
    // superhack to guarantee advancement
    if (c == 'n' && !path_is_nil) {
      // null cannot evaluate
      RETURN_FALSE_IF_PARSE_ERROR(skipOne());
      return false;
    }
    if (value_string && !path_is_nil) {
      RETURN_FALSE_IF_PARSE_ERROR(skipOne());
      return false;
    }

    if (value_string && path_is_nil && style == RAW) {
      const auto sv = getOne();
      if (error_ != kErrorNone) {
        return false;
      }
      jsonGenerator->writeRaw(sv);
      return true;
    }

    if (c == '[' && path_is_nil && style == FLATTEN) {
      RETURN_FALSE_IF_PARSE_ERROR(consume('['));
      bool dirty = false;

      while (peek() != ']') {
        dirty |= getJsonPathSpark(path, index + 1, style, jsonGenerator,
                                  jsonGeneratorFactory);
        RETURN_FALSE_IF_PARSE_ERROR(skipIfPresent(','));
      }
      RETURN_FALSE_IF_PARSE_ERROR(consume(']'));
      return dirty;
    }

    if (path_is_nil) {
      if (!jsonGenerator->isBeginArray() && !jsonGenerator->isEmpty()) {
        jsonGenerator->writeComma();
      }

      const auto sv = getOne();
      if (error_ != kErrorNone) {
        return false;
      }
      const auto copy_success = jsonGenerator->copyCurrentStructure(sv);
      if (!copy_success) {
        error_ = kParseErrorUnexpect;
        return false;
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
            RETURN_FALSE_IF_PARSE_ERROR(skipOne());
            RETURN_FALSE_IF_PARSE_ERROR(consume(':'));
            RETURN_FALSE_IF_PARSE_ERROR(skipOne());
            RETURN_FALSE_IF_PARSE_ERROR(skipIfPresent(','));
          }
        } else {
          // The next "string_value" is a key
          setIsFieldName();
          dirty = getJsonPathSpark(path, index, style, jsonGenerator,
                                   jsonGeneratorFactory);

          RETURN_FALSE_IF_PARSE_ERROR(skipIfPresent(','));
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
        jsonGenerator->writeComma();
      }
      jsonGenerator->writeStartArray();
      while (peek() != ']') {
        const auto index_plus_two = index + 2;
        dirty |= getJsonPathSpark(path, index_plus_two, FLATTEN, jsonGenerator,
                                  jsonGeneratorFactory);
        RETURN_FALSE_IF_PARSE_ERROR(skipIfPresent(','));
      }
      jsonGenerator->writeEndArray();
      RETURN_FALSE_IF_PARSE_ERROR(consume(']'));
      return dirty;
    }

    if (c == '[' && path[index].is_wildcard() && style != QUOTE) {
      int64_t dirty = 0;
      auto nextStyle = style == RAW ? QUOTE : style;
      WriteBuffer wb;
      auto localJsonGenerator = jsonGeneratorFactory(wb);

      RETURN_FALSE_IF_PARSE_ERROR(consume('['));
      while ((pos_ < len_) && peek() != ']') {
        size_t pos_before = pos_;
        dirty += getJsonPathSpark(path, index + 1, nextStyle,
                                  localJsonGenerator, jsonGeneratorFactory)
                     ? 1
                     : 0;
        if (pos_ == pos_before) {
          if (pos_ < len_) {
            pos_++;
          } else {
            setError(SonicError::kParseErrorEof);
            return false;
          }
        }
        RETURN_FALSE_IF_PARSE_ERROR(skipIfPresent(','));
      }
      if (sonic_unlikely(pos_ == len_)) {
        setError(SonicError::kParseErrorEof);
        return false;
      }
      RETURN_FALSE_IF_PARSE_ERROR(consume(']'));
      if (dirty > 1) {
        if (!jsonGenerator->isBeginArray() && !jsonGenerator->isEmpty()) {
          jsonGenerator->writeComma();
        }
        jsonGenerator->writeStartArray();
        // should always use explicit `Size`, because there maybe '\0' in the wb
        jsonGenerator->writeRawValue(wb.ToStringView());
        jsonGenerator->writeEndArray();
      } else if (dirty == 1) {
        jsonGenerator->writeRawValue(wb.ToStringView());
      }

      return dirty > 0;
    }

    if (c == '[' && path[index].is_wildcard()) {
      bool dirty = false;
      if (!jsonGenerator->isBeginArray() && !jsonGenerator->isEmpty()) {
        jsonGenerator->writeComma();
      }
      jsonGenerator->writeStartArray();
      RETURN_FALSE_IF_PARSE_ERROR(consume('['));
      while (peek() != ']') {
        const auto index_plus_one = index + 1;

        dirty |= getJsonPathSpark(path, index_plus_one, QUOTE, jsonGenerator,
                                  jsonGeneratorFactory);
        RETURN_FALSE_IF_PARSE_ERROR(skipIfPresent(','));
      }
      RETURN_FALSE_IF_PARSE_ERROR(consume(']'));
      jsonGenerator->writeEndArray();
      return dirty;
    }

    if (c == '[' && path[index].is_index()) {
      const auto array_index = path[index].index();
      auto nextStyle = style;

      const bool path_has_two_more = index + 2 < path.size();
      if (path_has_two_more && path[index + 1].is_wildcard()) {
        nextStyle = QUOTE;
      }

      return getJsonPathSparkArrayIndex(path, index, nextStyle, jsonGenerator,
                                        jsonGeneratorFactory, array_index);
    }

    if (field_name && path[index].is_key()) {
      const bool found = advanceKey(path[index].key());
      if (error_ != kErrorNone) {
        return false;
      }

      if (found) {
        // if not null
        if (peek() != 'n') {
          return getJsonPathSpark(path, index + 1, style, jsonGenerator,
                                  jsonGeneratorFactory);
        } else {
          // skip null
          RETURN_FALSE_IF_PARSE_ERROR(skipOne());
          return false;
        }
      }
      return false;
    }

    if (field_name && path[index].is_wildcard()) {
      RETURN_FALSE_IF_PARSE_ERROR(skipOne());
      RETURN_FALSE_IF_PARSE_ERROR(consume(':'));
      return getJsonPathSpark(path, index + 1, style, jsonGenerator,
                              jsonGeneratorFactory);
    }

    if (c == '{' || c == '[') {
      if (c == '{') {
        RETURN_FALSE_IF_PARSE_ERROR(consume('{'));
        while (peek() != '}') {
          RETURN_FALSE_IF_PARSE_ERROR(SkipString(data_, pos_, len_));
          RETURN_FALSE_IF_PARSE_ERROR(consume(':'));
          RETURN_FALSE_IF_PARSE_ERROR(skipOne());
          RETURN_FALSE_IF_PARSE_ERROR(skipIfPresent(','));
        }
      } else if (c == '[') {
        RETURN_FALSE_IF_PARSE_ERROR(consume('['));
        while (peek() != ']') {
          RETURN_FALSE_IF_PARSE_ERROR(skipOne());
          RETURN_FALSE_IF_PARSE_ERROR(skipIfPresent(','));
        }
      }
    }
    return false;
  }
  // SkipOne skip one raw json value and return the start of value, return the
  // negtive if errors.
  inline SonicError getJsonPath(const JsonPath &path, size_t index,
                                std::vector<StringView> &res,
                                bool complete = false) {
    if (index >= path.size()) {
      res.push_back(getOne());
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
          skipOne();
        } else {
          setError(SonicError::kUnmatchedTypeInJsonPath);
        }
        return error_;
      }

      bool found = advanceKey(path[index].key());
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
          skipOne();
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
  const uint8_t *data_ = nullptr;
  size_t pos_ = 0;
  size_t len_ = 0;
  SonicError error_ = SonicError::kErrorNone;
  std::vector<uint8_t> kbuf_ = {};
  bool isFieldName = false;
};
}  // namespace internal
}  // namespace sonic_json
