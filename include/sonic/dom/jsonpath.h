

#pragma once

#include "sonic/dom/parser.h"

#include <cinttypes>
#include <cstdint>
#include <cstring>
#include <stack>
#include <vector>

#include "sonic/string_view.h"

namespace sonic_json {

namespace internal {
const char NONE = '\0';
const char WILDCARD = '*';
const char ROOT = '$';

class JsonPathNode {
 public:
  JsonPathNode() noexcept = default;
  JsonPathNode(int index) noexcept : index_(index) {}
  JsonPathNode(StringView key) noexcept : key_(key) {}
  JsonPathNode(char token) noexcept : token_(token) {}
  ~JsonPathNode() = default;

 public:
  bool is_wildcard() const noexcept { return token_ == '*'; }

  bool is_key() const noexcept { return index_ == -1 && token_ == '\0'; }

  bool is_index() const noexcept { return index_ != -1; }

  bool is_root() const noexcept { return token_ == '$'; }

  StringView key() const noexcept {
    sonic_assert(index_ == -1 && token_ == '\0');
    return key_;
  }

  int index() const noexcept {
    sonic_assert(index_ != -1 && token_ == '\0');
    return index_;
  }

  char token() const noexcept {
    sonic_assert(index_ == -1 && key_ == "");
    return token_;
  }

 private:
  int index_ = -1;
  StringView key_ = "";
  // special tokens
  char token_ = '\0';
};

// to parse escaped chars inplace
sonic_force_inline std::string paddingJsonPath(StringView path) {
  std::string padded;
  padded.reserve(path.size() + 8);
  padded.append(path.data(), path.size());
  padded.append(8, '\0');
  padded.resize(path.size());
  return padded;
}

/**
 * Respresent a JSON path. RFC is https://datatracker.ietf.org/doc/rfc9535/.
 * NOTE: descendant, slice, filter and curent node not support.
 */
class JsonPath : public std::vector<JsonPathNode> {
 private:
  // case as .abc
  sonic_force_inline bool parseUnquotedKey(StringView path, size_t& index,
                                           JsonPathNode& node) {
    size_t start = index;
    while (index < path.size() && path[index] != '.' && path[index] != '[') {
      index++;
    }
    if (start == index) {
      return false;
    }
    node = JsonPathNode(path.substr(start, index - start));
    return true;
  }

  // case as .123
  sonic_force_inline bool parseRawIndex(StringView path, size_t& index,
                                        JsonPathNode& node) {
    int sum = 0;
    while (index < path.size() && path[index] >= '0' && path[index] <= '9') {
      sum = sum * 10 + (path[index] - '0');
      index++;
    }
    node = JsonPathNode(sum);
    return true;
  }

  // case as ['abc']  or ["abc"]
  sonic_force_inline bool parseQuotedName(StringView path, size_t& index,
                                          JsonPathNode& node) {
    if (path[index] != '\'' && path[index] != '"') {
      return false;
    }

    char quote = path[index++];
    size_t start = index;
    char* dst = const_cast<char*>(&path[start]);
    const char* src = &path[start];
    const char* end = &path[path.size() - 1];
    size_t len = 0;
    // normalized path
    if (quote == '\"') {
      while (src < end && *src != quote) {
        if (*src == '\\') {
          if (internal::common::unescape_with_padding(
                  reinterpret_cast<const uint8_t**>(&src),
                  reinterpret_cast<uint8_t**>(&dst)) == 0) {
            return false;
          }
        } else {
          *dst++ = *src++;
        }
      }
      len = (dst - &path[0]) - start;
    } else {
      while (src < end && *src != quote) {
        src++;
      }
      len = (src - &path[0]) - start;
    }

    index = src - &path[0];
    node = JsonPathNode(path.substr(start, len));
    if (start == index || path[++index] != ']') {
      return false;
    }

    index++;

    return true;
  }

  // case as [123]
  sonic_force_inline bool parseBracktedIndex(StringView path, size_t& index,
                                             JsonPathNode& node) {
    index += 1;
    if (!parseRawIndex(path, index, node)) {
      return false;
    }
    if (path[index++] != ']') {
      return false;
    }
    return true;
  }

  sonic_force_inline bool parseWildcard(StringView path, size_t& index,
                                        JsonPathNode& node) {
    if (index + 1 < path.size() && path[index] == '*' &&
        path[index + 1] == ']') {
      node = JsonPathNode('*');
      index += 2;
      return true;
    }
    return false;
  }

 public:
  sonic_force_inline bool Parse(StringView path) noexcept {
    this->clear();
    if (path.empty() || path[0] != '$') {
      return false;
    }

    this->emplace_back(JsonPathNode('$'));
    size_t i = 1;
    bool valid = false;
    JsonPathNode node;
    while (i < path.size()) {
      if (i + 2 < path.size() && path[i] == '.' && path[i + 1] == '.') {
        return false;
      }

      if (path[i] == '.') {
        if (i + 1 >= path.size()) {
          return false;
        }

        i++;
        if (path[i] == '*') {
          has_wildcard_ = true;
          this->emplace_back(JsonPathNode(WILDCARD));
          i++;
          continue;
        }

        if (path[i] >= '0' && path[i] <= '9') {
          valid = parseRawIndex(path, i, node);
        } else {
          valid = parseUnquotedKey(path, i, node);
        }
      } else if (path[i] == '[') {
        if (i + 1 >= path.size()) {
          return false;
        }

        i++;
        if (path[i] == '*') {
          if (i + 1 < path.size() && path[i + 1] == ']') {
            has_wildcard_ = true;
            this->emplace_back(JsonPathNode(WILDCARD));
            i += 2;
            continue;
          }
          return false;
        }

        if (path[i] == '\'' || path[i] == '"') {
          valid = parseQuotedName(path, i, node);
        } else if (path[i] >= '0' && path[i] <= '9') {
          valid = parseBracktedIndex(path, i, node);
        } else {
          return false;
        }
      } else {
        valid = false;
      }

      if (!valid) {
        this->clear();
        return false;
      }

      this->emplace_back(node);
    }
    return true;
  }

  bool HasWildcard() const noexcept { return has_wildcard_; }

 private:
  bool has_wildcard_ = false;
};

}  // namespace internal
}  // namespace sonic_json
