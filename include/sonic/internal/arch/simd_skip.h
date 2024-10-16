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
#include "simd_dispatch.h"
#include "sonic/jsonpath/jsonpath.h"
#include "sonic/error.h"

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
                                   StringView key, std::vector<uint8_t>& kbuf, SonicError& err) {

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
    return slen == static_cast<long>(key.size()) && std::memcmp(start, key.data(), slen) == 0;
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
        long start = this->scanner_.SkipOne(this->data_, pos_, this->len_);
        if (start < 0) {
            setError(SonicError(-start));
            return "";
        }
        return  StringView(reinterpret_cast<const char *>(this->data_) + start, this->pos_ - start);
    }

    sonic_force_inline SonicError skipOne() {
        long start = this->scanner_.SkipOne(this->data_, pos_, this->len_);
        if (start < 0) {
            setError(SonicError(-start));
            return this->error_;
        }
        return SonicError::kErrorNone;
    }

    sonic_force_inline uint8_t peek() {
        auto peek = this->scanner_.SkipSpaceSafe(this->data_, pos_, this->len_);
        this->pos_ -= 1;
        return peek;
    }

    sonic_force_inline bool hasError() {
        return error_ != SonicError::kErrorNone;
    }

    sonic_force_inline uint8_t advance() {
        return this->scanner_.SkipSpaceSafe(this->data_, pos_, this->len_);
    }

    sonic_force_inline bool consume(uint8_t c) {
        auto got = this->scanner_.SkipSpaceSafe(this->data_, pos_, this->len_);
        if (got != c) {
            this->setError(SonicError::kParseErrorInvalidChar);
            return false;
        }
        return true;
    }

    sonic_force_inline void setError(SonicError err) {
        this->error_ = SonicError::kParseErrorInvalidChar;
    }

    sonic_force_inline void advanceKey(StringView key) {
        auto c = advance();
        bool matched = false;
        while (c != '}') {
            if (c != '"') {
                setError(SonicError::kParseErrorInvalidChar);
                return;
            }

            // match the key
            matched = scanner_.matchKey(data_, pos_, len_, key, kbuf_, error_);
            if (error_ != SonicError::kErrorNone) {
                return;
            }

            if (!this->consume(':')) {
                setError(SonicError::kParseErrorInvalidChar);
                return;
            }


            if (matched) {
                return;
            } 

            if (this->skipOne() != SonicError::kErrorNone) {
                return;
            }

            // get the next key
            c = advance();
            if (c == ',') {
                c = advance();
            } else if (c != '}') {
                this->setError(SonicError::kParseErrorInvalidChar);
            }
        }
    }

    sonic_force_inline SonicError traverseObject(const JsonPath& path, size_t index,
                             std::vector<StringView>& res) {
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
        
            if (!this->consume(':')) {
                return error_;
            }

            // recursively parse the value
            if (getJsonPath(path, index + 1, res, true)!= SonicError::kErrorNone) {
                return error_;
            }

            // get the next key
            c = advance();
            if (c == ',') {
                c = advance();
            } else if (c != '}') {
                this->setError(SonicError::kParseErrorInvalidChar);
                return error_;
            }
        }
        return kErrorNone;
    }

    sonic_force_inline SonicError traverseArray(const JsonPath& path, size_t index,
                             std::vector<StringView>& res) {
        auto c = advance();
        pos_ --;
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
                this->setError(SonicError::kParseErrorInvalidChar);
                return error_;
            }
        }
        return kErrorNone;
    }

    sonic_force_inline void advanceIndex(size_t index) {
        auto c = advance();
        if (c == ']') {
            setError(SonicError::kParseErrorArrIndexOutOfRange);
            return;
        }

        pos_ --; // backwared for skip the first elem
        while (c != ']' && index > 0) {
            if (this->skipOne() != SonicError::kErrorNone) {
                return;
            }
            
            // get the next key
            c = advance();
            if (c == ',') {
                index --;
            } else if (c != ']') {
                setError(SonicError::kParseErrorInvalidChar);
            }
        }

        if (index > 0) {
            setError(SonicError::kParseErrorArrIndexOutOfRange);
        }
    }

    sonic_force_inline SonicError skipArrayRemain() {
        auto c = advance();
        while (c != ']') {
            if (c != ',') {
                setError(SonicError::kParseErrorInvalidChar);
                return error_;
            }

            if (this->skipOne() != SonicError::kErrorNone) {
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

            if (this->skipOne() != SonicError::kErrorNone) {
                return error_;
            }
    
            // get the next key
            c = advance();
        }
        return kErrorNone;
    }


  // SkipOne skip one raw json value and return the start of value, return the
  // negtive if errors.
  inline SonicError getJsonPath(const JsonPath& path, size_t index,
                             std::vector<StringView>& res, bool complete = false) {
    if (index >= path.size()) {
        res.push_back(this->getOne());
        return this->error_;
    }

    auto c = advance();
    if (path[index].is_wildcard()) {
        if (c == '{') {
            return traverseObject(path, index, res);
        }
        
        if (c == '[') {
            return traverseArray(path, index, res);
        }

        setError(SonicError::kUnmatchedTypeInJsonPath);
        return error_;
    }

    if (path[index].is_key()) {
        if (c != '{') {
            setError(SonicError::kUnmatchedTypeInJsonPath);
            return error_;
        }

        advanceKey(path[index].key());
        if (hasError()) {
            return error_;
        }
    
        error_ = getJsonPath(path, index + 1, res, complete);
        if (hasError() || !complete) {
            return error_;
        }

        return skipObjectRemain();
    }

    if (path[index].is_index()) {
        if (c != '[') {
            setError(SonicError::kUnmatchedTypeInJsonPath);
            return error_;
        }

        // index maybe negative
        int64_t idx = path[index].index();
        if (idx < 0) {
            setError(SonicError::kUnsupportedJsonPath);
            return error_;
        }

        advanceIndex(path[index].index());
        if (hasError()) {
            return this->error_;
        }
        

        error_ = getJsonPath(path, index + 1, res, complete);
        if (hasError() || !complete) {
            return error_;
        }

        return skipArrayRemain();
    }
    return SonicError::kErrorNone;
   }
 public:
  SkipScanner scanner_;
  const uint8_t *data_ = nullptr;
  size_t pos_ = 0;
  size_t len_ = 0;
  SonicError error_ = SonicError::kErrorNone;
  std::vector<uint8_t> kbuf_ = {};
};
}  // namespace internal
}  // namespace sonic_json
