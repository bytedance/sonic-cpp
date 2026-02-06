

#pragma once

#include <cctype>
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
        // Handle other special-character tokens
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
  // Keep the extra '\0' bytes *within* the string size so that
  // internal::common::unescape_with_padding() can safely read past the logical
  // end of the path without triggering out-of-bounds reads.
  std::string padded(path.data(), path.size());
  padded.append(8, '\0');
  return padded;
}

/**
 * Represent a JSON path. RFC is https://datatracker.ietf.org/doc/rfc9535/.
 * NOTE: descendant, slice, filter and current node not support.
 */
class JsonPath : public std::vector<JsonPathNode> {
 private:
  // Keep a writable, padded copy of the input path so that:
  // - parseQuotedName() can do in-place unescaping safely
  // - unescape_with_padding() can read past logical end without OOB
  std::string padded_;

  // Parse using a caller-provided padded, writable buffer.
  // The caller must ensure `padded.data()` points to a buffer that has at least
  // 8 extra '\0' bytes after `logical_len`.
  // The caller must keep the buffer alive while the parsed JsonPath is used.
  sonic_force_inline bool ParsePaddedInternal(StringView padded,
                                              size_t logical_len) noexcept {
    StringView p(padded.data(), logical_len);

    if (p.empty() || p[0] != '$') {
      return false;
    }

    this->emplace_back(JsonPathNode('$'));
    size_t i = 1;
    bool valid = false;
    JsonPathNode node;
    while (i < p.size()) {
      valid = false;

      if (i + 2 < p.size() && p[i] == '.' && p[i + 1] == '.') {
        return false;
      }

      if (p[i] == '.') {
        if (i + 1 >= p.size()) {
          return false;
        }

        i++;
        if (p[i] == '*') {
          this->emplace_back(JsonPathNode(WILDCARD));
          i++;
          continue;
        }
        valid = parseUnquotedKey(p, i, node);
      } else if (p[i] == '[') {
        if (i + 1 >= p.size()) {
          return false;
        }

        i++;
        if (p[i] == '*') {
          if (i + 1 < p.size() && p[i + 1] == ']') {
            this->emplace_back(JsonPathNode(WILDCARD));
            i += 2;
            continue;
          }
          return false;
        }

        if (p[i] == '\'' || p[i] == '"') {
          valid = parseQuotedName(p, i, node);
        } else if ((p[i] >= '0' && p[i] <= '9') || p[i] == '-') {
          valid = parseBracktedIndex(p, i, node);
        } else {
          // Unsupported bracket expression (e.g. unquoted name).
          valid = false;
        }
      } else {
        // Unknown token, prevent infinite loop / stale `valid` reuse.
        return false;
      }

      if (!valid) {
        this->clear();
        return false;
      }

      this->emplace_back(node);
    }
    return true;
  }

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
    if (index >= path.size()) {
      return false;
    }

    if (path[index] != '\'' && path[index] != '"') {
      return false;
    }

    const char* base = path.data();
    const size_t n = path.size();

    char quote = base[index++];
    size_t start = index;
    if (start >= n) {
      return false;
    }

    char* dst = const_cast<char*>(base + start);
    const char* src = base + start;
    const char* end = base + n;
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
      if (src >= end) {
        return false;
      }
      len = static_cast<size_t>(dst - (base + start));
    } else {
      while (src < end && *src != quote) {
        src++;
      }
      if (src >= end) {
        return false;
      }
      len = static_cast<size_t>(src - (base + start));
    }

    const size_t quote_pos = static_cast<size_t>(src - base);
    node = JsonPathNode(path.substr(start, len));
    // Expect closing quote then ']'.
    if (start == quote_pos || quote_pos + 1 >= n ||
        base[quote_pos + 1] != ']') {
      return false;
    }

    index = quote_pos + 2;

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
  // Parse with a padded, writable buffer (avoids extra copy).
  // See ParsePaddedInternal() for lifetime and padding requirements.
  sonic_force_inline bool ParsePadded(StringView padded,
                                      size_t logical_len) noexcept {
    this->clear();
    padded_.clear();
    return ParsePaddedInternal(padded, logical_len);
  }

  sonic_force_inline bool Parse(StringView path) noexcept {
    this->clear();
    padded_ = paddingJsonPath(path);
    return ParsePaddedInternal(StringView(padded_.data(), padded_.size()),
                               path.size());
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
