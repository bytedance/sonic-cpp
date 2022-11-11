
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

#include "sonic/internal/itoa.h"

#include <climits>

#include "gtest/gtest.h"

namespace {

using namespace sonic_json;
using namespace sonic_json::internal;

void TestU64toa(const std::string& expect, uint64_t val) {
  // reserved buffer should be 32 bytes
  char buf[32] = {0};
  char* out = U64toa(buf, val);
  *out = '\0';
  EXPECT_STREQ(expect.data(), buf);
  EXPECT_EQ(out - buf, expect.size());
}

void TestI64toa(const std::string& expect, int64_t val) {
  // reserved buffer should be 32 bytes
  char buf[32] = {0};
  char* out = I64toa(buf, val);
  *out = '\0';
  EXPECT_STREQ(expect.data(), buf);
  EXPECT_EQ(out - buf, expect.size());
}

TEST(U64toa, Basic) {
  TestU64toa("0", 0);
  TestU64toa("1", 1);
  TestU64toa("12", 12);
  TestU64toa("123", 123);
  TestU64toa("1234", 1234);
  TestU64toa("12345", 12345);
  TestU64toa("123456", 123456);
  TestU64toa("1234567", 1234567);
  TestU64toa("12345678", 12345678);
  TestU64toa("123456789", 123456789);
  TestU64toa("1234567890", 1234567890);
  TestU64toa("12345678901", 12345678901);
  TestU64toa("123456789012", 123456789012);
  TestU64toa("1234567890123", 1234567890123);
  TestU64toa("12345678901234", 12345678901234);
  TestU64toa("123456789012345", 123456789012345);
  TestU64toa("1234567890123456", 1234567890123456ull);
  TestU64toa("12345678901234567", 12345678901234567ull);
  TestU64toa("123456789012345678", 123456789012345678ull);
  TestU64toa("1234567890123456789", 1234567890123456789ull);
  TestU64toa("12345678901234567890", 12345678901234567890ull);
  TestU64toa("18446744073709551615", UINT64_MAX);
}

TEST(I64toa, Basic) {
  TestI64toa("0", 0);
  TestI64toa("1", 1);
  TestI64toa("-12", -12);
  TestI64toa("123", 123);
  TestI64toa("-1234", -1234);
  TestI64toa("9223372036854775807", INT64_MAX);
  TestI64toa("-9223372036854775808", INT64_MIN);
}

}  // namespace
