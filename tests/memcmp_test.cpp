/*
 * Copyright 2023 ByteDance Inc.
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

#include <memory>
#include <random>
#include <string>

#include "gtest/gtest.h"
#include "include/sonic/internal/arch/sonic_cpu_feature.h"

#if defined(SONIC_HAVE_AVX2) && !defined(SONIC_DYNAMIC_DISPATCH)
#include "include/sonic/internal/arch/avx2/base.h"

namespace {

using namespace sonic_json::internal::avx2;

static std::string random_string(int str_len) {
  // const char * strs =
  // "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890!@#$%^&*()这是一个字符串";
  std::string re;

  std::random_device rd;
  std::mt19937 gen(rd());
  for (int i = 0; i < str_len; ++i) {
    char c = gen() % 26 + 'a';
    re.append(1, c);
  }
  return re;
}

bool is_correct(int a, int b) {
  if (a < 0) return b < 0;
  if (a > 0) return b > 0;
  return a == b;
}

TEST(InlinedMemcmp, Basic) {
  EXPECT_EQ(0, InlinedMemcmp("", "", 0));
  EXPECT_EQ(0, InlinedMemcmp("123", "1", 0));
  EXPECT_EQ(0, InlinedMemcmp("123", "1", 1));
  EXPECT_EQ(-1, InlinedMemcmp("12345678901234567890123456789012345",
                              "22345678901234567890123456789012345", 35));
  for (int i = 0; i < 1024; ++i) {
    std::string str1 = random_string(i);
    std::string str2 = random_string(i);
    EXPECT_EQ(str1.size(), str2.size());
    EXPECT_TRUE(
        is_correct(std::memcmp(str1.data(), str2.data(), str1.size()),
                   InlinedMemcmp(str1.data(), str2.data(), str1.size())))
        << "str1 is: " << str1 << std::endl
        << "str2 is: " << str2 << std::endl
        << "std::memcmp is: "
        << std::memcmp(str1.data(), str2.data(), str1.size()) << std::endl
        << "InlinedMemcmp is: "
        << InlinedMemcmp(str1.data(), str2.data(), str1.size()) << std::endl;
  }

  for (int i = 1; i <= 1024; ++i) {
    std::string str = random_string(i);
    for (int j = 0; j < i; ++j) {
      std::string str1 = str;
      std::string str2 = str;
      EXPECT_EQ(0, InlinedMemcmp(str1.data(), str2.data(), str1.size()));
      str1[j] = '1';
      str2[j] = '2';
      EXPECT_TRUE(InlinedMemcmp(str1.data(), str2.data(), str1.size()) < 0);
      str1[j] = '2';
      str2[j] = '1';
      EXPECT_TRUE(InlinedMemcmp(str1.data(), str2.data(), str1.size()) > 0);
    }
  }
}

TEST(InlinedMemcmp, CrossPage) {
  for (int i = 1; i <= 1024; ++i) {
    std::string str = random_string(i);
    auto a_ptr = std::unique_ptr<char[], void (*)(char*)>(
        static_cast<char*>(aligned_alloc(4096, 4096 * 2)),
        [](char* ptr) { free(ptr); });
    auto b_ptr = std::unique_ptr<char[]>(new char[i]);
    char* a = a_ptr.get() + 4095;
    char* b = b_ptr.get();
    for (int j = 0; j < i; ++j) {
      std::memcpy(a, str.data(), i);
      std::memcpy(b, str.data(), i);
      EXPECT_EQ(0, InlinedMemcmp(a, b, i));
      a[j] = '1';
      b[j] = '2';
      EXPECT_TRUE(InlinedMemcmp(a, b, i) < 0);
      a[j] = '2';
      b[j] = '1';
      EXPECT_TRUE(InlinedMemcmp(a, b, i) > 0);
    }
  }
}

void success_helper(const void* a, const void* b, size_t s) {
  EXPECT_TRUE(InlinedMemcmpEq(a, b, s))
      << "a is: " << std::string((char*)a, s) << std::endl
      << "b is: " << std::string((char*)b, s) << std::endl;
}

void failed_helper(const void* a, const void* b, size_t s) {
  EXPECT_FALSE(InlinedMemcmpEq(a, b, s))
      << "a is: " << std::string((char*)a, s) << std::endl
      << "b is: " << std::string((char*)b, s) << std::endl;
}

TEST(InlinedMemcmpEq, Basic) {
  {
    std::string str = random_string(1024);
    for (size_t i = 1; i < 1024; ++i) {
      auto a = std::unique_ptr<char[]>(new char[i]);
      auto b = std::unique_ptr<char[]>(new char[i]);
      std::memcpy(a.get(), str.data(), i);
      std::memcpy(b.get(), str.data(), i);
      success_helper(a.get(), b.get(), i);
      for (size_t j = i - 1; j > 0; --j) {
        a[j] = 'x';
        b[j] = 'y';
        failed_helper(a.get(), b.get(), j + 1);
      }
    }
  }
}

TEST(InlinedMemcmpEq, CrossPage) {
  {
    std::string str = random_string(1024);
    for (size_t i = 1; i < 1024; ++i) {
      auto a_ptr = std::unique_ptr<char[], void (*)(char*)>(
          static_cast<char*>(aligned_alloc(4096, 4096 * 2)),
          [](char* ptr) { free(ptr); });
      auto b = std::unique_ptr<char[]>(new char[i]);
      char* a = a_ptr.get() + 4095;
      std::memcpy(a, str.data(), i);
      std::memcpy(b.get(), str.data(), i);
      success_helper(a, b.get(), i);
      for (size_t j = i - 1; j > 0; --j) {
        a[j] = 'x';
        b[j] = 'y';
        failed_helper(a, b.get(), j + 1);
      }
    }
  }
}

}  // namespace
#endif
