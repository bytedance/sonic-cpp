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

#include "sonic/writebuffer.h"

#include <gtest/gtest.h>

namespace {

using namespace sonic_json;

TEST(WriteBuffer, CopyControl) {
  WriteBuffer wb;
  wb.Push("hello", 5);
  WriteBuffer wb2(std::move(wb));
  EXPECT_EQ(wb2.Size(), 5);
  EXPECT_TRUE(wb.Empty());

  WriteBuffer wb3;
  wb3 = std::move(wb2);
  EXPECT_EQ(wb3.Size(), 5);
  EXPECT_TRUE(wb.Empty());
}

TEST(WriteBuffer, PushPop) {
  WriteBuffer wb;
  wb.Push("hello", 5);
  EXPECT_EQ(*wb.Top<char>(), 'o');
  wb.Push<char>(' ');
  for (const auto c : {'w', 'o', 'r', 'l', 'd'}) {
    wb.Push<char>(c);
  }
  EXPECT_EQ(*wb.Top<char>(), 'd');
  EXPECT_EQ(wb.Size(), 11);

  wb.Pop<char>(5);
  EXPECT_EQ(*wb.Top<char>(), ' ');
  EXPECT_EQ(wb.Size(), 6);
}

TEST(WriteBuffer, Reserve) {
  WriteBuffer wb;
  wb.Reserve(300);
  EXPECT_EQ(wb.Capacity(), 300);
  // donothing when reserve cap is smaller than current
  wb.Reserve(1);
  EXPECT_EQ(wb.Capacity(), 300);
  wb.Push(std::string(500, 'x').data(), 500);
  EXPECT_TRUE(wb.Size() <= wb.Capacity());
}

TEST(WriteBuffer, ToString) {
  {
    WriteBuffer wb;
    wb.Push("hello", 5);
    EXPECT_STREQ(wb.ToString(), "hello");
  }
  {
    WriteBuffer wb;
    wb.Push("", 0);
    EXPECT_STREQ(wb.ToString(), "");
  }
  {
    WriteBuffer wb;
    wb.Push<char>('c');
    EXPECT_STREQ(wb.ToString(), "c");
  }
  {
    const WriteBuffer cwb;
    EXPECT_STREQ(cwb.ToString(), "");
  }
  {
    WriteBuffer wb;
    wb.Push<char>('c');
    const WriteBuffer cwb = std::move(wb);
    EXPECT_STREQ(cwb.ToString(), "c");
  }
}
}  // namespace
