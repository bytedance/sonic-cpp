

#pragma once

#include <cinttypes>
#include <cstdint>
#include <cstring>
#include <sstream>
#include <stack>
#include <string>
#include <vector>

#include "sonic/internal/arch/common/unicode_common.h"
#include "sonic/string_view.h"

namespace sonic_json {

namespace internal {
static constexpr char NONE = '\0';
static constexpr char WILDCARD = '*';
static constexpr char ROOT = '$';
static constexpr char IS_KEY = '\x01';
static constexpr char IS_INDEX = '\x02';
static constexpr char KEY_OR_INDEX = '\x03';

class JsonPathNode {
 public:
  JsonPathNode() noexcept = default;
  JsonPathNode(int64_t index) noexcept : index_(index), token_(IS_INDEX) {}
  JsonPathNode(StringView key) noexcept : key_(key), token_(IS_KEY) {}
  JsonPathNode(StringView key, int64_t index) noexcept
      : index_(index), key_(key), token_(KEY_OR_INDEX) {}
  JsonPathNode(char token) noexcept : token_(token) {}
  ~JsonPathNode() = default;

 public:
  bool is_wildcard() const noexcept { return token_ == WILDCARD; }

  bool is_key() const noexcept {
    return token_ == IS_KEY || token_ == KEY_OR_INDEX;
  }

  bool is_index() const noexcept {
    return token_ == IS_INDEX || token_ == KEY_OR_INDEX;
  }

  bool is_none() const noexcept { return token_ == NONE; }

  bool is_root() const noexcept { return token_ == ROOT; }

  StringView key() const noexcept {
    sonic_assert(is_key());
    return key_;
  }

  int64_t index() const noexcept {
    sonic_assert(is_index());
    return index_;
  }

  char token() const noexcept {
    sonic_assert(!is_key() && !is_index() && !is_none());
    return token_;
  }

  std::string to_string() const {
    std::stringstream ss;

    switch (token_) {
      case NONE:
        ss << "NONE(\\0)";
        break;

      case WILDCARD:
        ss << "WILDCARD(*)";
        break;

      case ROOT:
        ss << "ROOT($)";
        break;

      case IS_KEY:
        ss << "KEY(\"";
        for (char c : key_) {
          if (std::isprint(static_cast<unsigned char>(c))) {
            ss << c;
          } else {
            ss << "\\x" << std::hex << std::uppercase
               << static_cast<int>(static_cast<unsigned char>(c));
          }
        }
        ss << "\")";
        break;

      case IS_INDEX:
        ss << "INDEX(" << index_ << ")";
        break;

      case KEY_OR_INDEX:
        ss << "KEY_OR_INDEX(\"";
        for (char c : key_) {
          if (std::isprint(static_cast<unsigned char>(c))) {
            ss << c;
          } else {
            ss << "\\x" << std::hex << std::uppercase
               << static_cast<int>(static_cast<unsigned char>(c));
          }
        }
        ss << "\", " << index_ << ")";
        break;

      default:
        // 处理其他特殊字符token
        if (std::isprint(static_cast<unsigned char>(token_))) {
          ss << "TOKEN(" << token_ << ")";
        } else {
          ss << "TOKEN(\\x" << std::hex << std::uppercase
             << static_cast<int>(static_cast<unsigned char>(token_)) << ")";
        }
        break;
    }

    return ss.str();
  }

 private:
  int64_t index_ = 0;
  StringView key_ = "";
  // record special tokens, also distinguish key and index
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
 * Represent a JSON path. RFC is https://datatracker.ietf.org/doc/rfc9535/.
 * NOTE: descendant, slice, filter and current node not support.
 */
class JsonPath : public std::vector<JsonPathNode> {
 private:
  sonic_force_inline bool parseNumber(StringView path, size_t& index,
                                      uint64_t& sum) {
    size_t start = index;
    // check leading zero
    if (index < path.size() && path[index] == '0') {
      index++;
      return true;
    }

    while (index < path.size() && path[index] >= '0' && path[index] <= '9') {
      auto last = sum * 10 + (path[index] - '0');
      // check overflow
      if (last < sum) {
        return false;
      }
      sum = last;
      index++;
    }

    return (sum <= INT64_MAX) && index != start;
  }

  // case as .abc
  sonic_force_inline bool parseUnquotedKey(StringView path, size_t& index,
                                           JsonPathNode& node) {
    size_t start = index;
    while (index < path.size() && path[index] != '.' && path[index] != '[') {
      index++;
    }
    size_t len = index - start;
    if (len == 0) {
      return false;
    }

    node = JsonPathNode(path.substr(index - len, len));
    return true;
  }

  // case as [123] or [-123]
  sonic_force_inline bool parseBracktedIndex(StringView path, size_t& index,
                                             JsonPathNode& node) {
    uint64_t sum = 0;
    int sign = 1;

    // check negative
    if (index < path.size() && path[index] == '-') {
      index++;
      sign = -1;
    }

    if (!parseNumber(path, index, sum)) {
      return false;
    }

    // match ']'
    if (index >= path.size() || path[index] != ']') {
      return false;
    }
    index++;
    node = JsonPathNode(int64_t(sum) * sign);
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

  // case as [abc]
  sonic_force_inline bool parseBrackedUnquotedKey(StringView path,
                                                  size_t& index,
                                                  JsonPathNode& node) {
    size_t start = index;
    while (index < path.size() && path[index] != ']') {
      index++;
    }
    if (start == index) {
      return false;
    }
    node = JsonPathNode(path.substr(start, index - start));
    index++;
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
          this->emplace_back(JsonPathNode(WILDCARD));
          i++;
          continue;
        }
        valid = parseUnquotedKey(path, i, node);
      } else if (path[i] == '[') {
        if (i + 1 >= path.size()) {
          return false;
        }

        i++;
        if (path[i] == '*') {
          if (i + 1 < path.size() && path[i + 1] == ']') {
            this->emplace_back(JsonPathNode(WILDCARD));
            i += 2;
            continue;
          }
          return false;
        }

        if (path[i] == '\'' || path[i] == '"') {
          valid = parseQuotedName(path, i, node);
        } else if ((path[i] >= '0' && path[i] <= '9') || path[i] == '-') {
          valid = parseBracktedIndex(path, i, node);
        }
      }

      if (!valid) {
        this->clear();
        return false;
      }

      this->emplace_back(node);
    }
    return true;
  }

  std::string to_string() const {
    std::stringstream ss;
    ss << "[";
    for (const auto& node : *this) {
      ss << node.to_string() << ", ";
    }
    ss << "]";
    return ss.str();
  }
};

}  // namespace internal
}  // namespace sonic_json
