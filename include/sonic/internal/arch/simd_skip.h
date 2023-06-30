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

#include "simd_dispatch.h"

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
    return -kParseErrorUnknownObjKey;
  err_invalid_char:
    pos -= 1;
    return -kParseErrorInvalidChar;
  }

 private:
  size_t nonspace_bits_end_{0};
  uint64_t nonspace_bits_{0};
};

}  // namespace internal
}  // namespace sonic_json
