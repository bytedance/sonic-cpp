// Copyright 2022 ByteDance Inc.
// Unit test for StringBlock::Find control character handling

#if defined(__aarch64__) && defined(__ARM_FEATURE_SVE2)
#include <gtest/gtest.h>

#include "sonic/internal/arch/sve2-128/unicode.h"

using namespace sonic_json::internal::sve2_128;

// 1. Base case: Contains no characters that require escaping or special
// handling
TEST(StringBlockTest, NoSpecialCharacters) {
  alignas(16) uint8_t data[16] = {'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h',
                                  'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p'};
  StringBlock block = StringBlock::Find(data);

  EXPECT_EQ(block.unescaped_index, 16u);
  EXPECT_EQ(block.quote_index, 16u);
  EXPECT_EQ(block.bs_index, 16u);
}

// 2. Fix and extend original tests: Test different control characters and the
// head/tail boundaries of the block
TEST(StringBlockTest, FindsControlCharactersAtBoundaries) {
  // Encounters 0x00 (NUL) at the beginning
  alignas(16) uint8_t data_head[16] = {0x00, 'b', 'c', 'd', 'e', 'f', 'g', 'h',
                                       'i',  'j', 'k', 'l', 'm', 'n', 'o', 'p'};
  EXPECT_EQ(StringBlock::Find(data_head).unescaped_index, 0u);

  // Encounters 0x1F (US - Unit Separator, the highest control character) at the
  // end
  alignas(16) uint8_t data_tail[16] = {'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h',
                                       'i', 'j', 'k', 'l', 'm', 'n', 'o', 0x1F};
  EXPECT_EQ(StringBlock::Find(data_tail).unescaped_index, 15u);
}

// 3. Finding quotes (") and backslashes (\)
TEST(StringBlockTest, FindsQuoteAndBackslash) {
  alignas(16) uint8_t data[16] = {'a', 'b', '"', 'd', 'e', 'f', '\\', 'h',
                                  'i', 'j', 'k', 'l', 'm', 'n', 'o',  'p'};
  StringBlock block = StringBlock::Find(data);

  EXPECT_EQ(block.unescaped_index, 16u);
  EXPECT_EQ(block.quote_index, 2u);  // 0x22
  EXPECT_EQ(block.bs_index, 6u);     // 0x5C
}

// 4. Multiple targets coexisting: Test if each property can independently and
// correctly report its respective first index
TEST(StringBlockTest, MultipleTargetsCoexist) {
  alignas(16) uint8_t data[16] = {'a', '\n', 'c', '"', 'e', '\\', 'g', '\t',
                                  '"', 'j',  'k', 'l', 'm', 'n',  'o', 'p'};
  StringBlock block = StringBlock::Find(data);

  EXPECT_EQ(block.unescaped_index, 1u);  // Encounters \n (0x0A)
  EXPECT_EQ(block.quote_index, 3u);      // Encounters the first "
  EXPECT_EQ(block.bs_index, 5u);         // Encounters '\'
}

// 5. UTF-8 high-byte false positive prevention test (very important)
TEST(StringBlockTest, IgnoresHighBitCharacters) {
  // Contains UTF-8 Chinese characters "你" (0xE4, 0xBD, 0xA0) and "好" (0xE5,
  // 0xA5, 0xBD) Used to verify that the underlying `< 0x20` evaluation does not
  // erroneously use signed 8-bit comparison
  alignas(16) uint8_t data[16] = {0xE4, 0xBD, 0xA0, 'a', 'b', 'c', 0xE5, 0xA5,
                                  0xBD, 'x',  'y',  'z', '1', '2', '3',  '4'};
  StringBlock block = StringBlock::Find(data);

  // No bytes > 127 should be misidentified as control characters
  EXPECT_EQ(block.unescaped_index, 16u);
  EXPECT_EQ(block.quote_index, 16u);
  EXPECT_EQ(block.bs_index, 16u);
}
#endif  // defined(__aarch64__) && defined(__ARM_FEATURE_SVE2)