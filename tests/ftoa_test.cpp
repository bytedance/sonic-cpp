// Copyright 2018 Ulf Adams
//
// The contents of this file may be used under the terms of the Apache License,
// Version 2.0.
//
//    (See accompanying file LICENSE-Apache or copy at
//     http://www.apache.org/licenses/LICENSE-2.0)
//
// Alternatively, the contents of this file may be used under the terms of
// the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE-Boost or copy at
//     https://www.boost.org/LICENSE_1_0.txt)
//
// Unless required by applicable law or agreed to in writing, this software
// is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.

// This file may have been modified by ByteDance authors. All ByteDance
// Modifications are Copyright 2022 ByteDance Authors.

#include "sonic/internal/ftoa.h"

#include <gtest/gtest.h>

#include <cmath>
#include <string>

// NOTE: the test case should as the ECMAScript Language Specification
// reference: https://tc39.es/ecma262/#sec-numeric-types-number-tostring
namespace {

using namespace sonic_json::internal;
using namespace sonic_json;

static void TestF64toa(const std::string& expect, double val) {
  char out[32];
  int len = F64toa(out, val);
  out[len] = '\0';
  EXPECT_STREQ(expect.data(), out);
  EXPECT_EQ(expect.size(), len);
}

static double int64Bits2Double(uint64_t bits) {
  double f;
  memcpy(&f, &bits, sizeof(double));
  return f;
}

static double ieeeParts2Double(const bool sign, const uint32_t ieeeExponent,
                               const uint64_t ieeeMantissa) {
  assert(ieeeExponent <= 2047);
  assert(ieeeMantissa <= ((uint64_t)1 << 53) - 1);
  return int64Bits2Double(((uint64_t)sign << 63) |
                          ((uint64_t)ieeeExponent << 52) | ieeeMantissa);
}

TEST(F64toa, Basic) {
  TestF64toa("0.0", 0.0);
  TestF64toa("-0.0", -0.0);
  TestF64toa("1.0", 1.0);
  TestF64toa("-1.0", -1.0);
  TestF64toa("1.23", 1.23);
}

TEST(F64toa, MinAndMax) {
  // Max Normal
  TestF64toa("1.7976931348623157e+308", int64Bits2Double(0x7fefffffffffffff));
  // Min Normal
  TestF64toa("2.2250738585072014e-308", int64Bits2Double(0x0010000000000000));
  // Min Subnormal
  TestF64toa("5e-324", int64Bits2Double(0x1));
}

TEST(F64toa, LotsOfTrailingZeros) {
  TestF64toa("2.9802322387695312e-8", 2.98023223876953125e-8);
}

TEST(F64toa, Regression) {
  TestF64toa("-21098088986959630.0", -2.109808898695963E16);
  TestF64toa("4.940656e-318", 4.940656e-318);
  TestF64toa("1.18575755e-316", 1.18575755e-316);
  TestF64toa("2.989102097996e-312", 2.989102097996e-312);
  TestF64toa("9060801153433600.0", 9.0608011534336E15);
  TestF64toa("9060801153433600.0", 9.060801153433601E15);
  TestF64toa("4708356024711512000.0", 4.708356024711512E18);
  TestF64toa("9409340012568248000.0", 9.409340012568248E18);
  TestF64toa("1.2345678", 1.2345678);
}

TEST(F64toa, LooksLikePow5) {
  // These numbers have a mantissa that is a multiple of the largest power of 5
  // that fits, and an exponent that causes the computation for q to result in
  // 22, which is a corner case for Ryu.
  TestF64toa("5.764607523034235e+39", int64Bits2Double(0x4830F0CF064DD592));
  TestF64toa("1.152921504606847e+40", int64Bits2Double(0x4840F0CF064DD592));
  TestF64toa("2.305843009213694e+40", int64Bits2Double(0x4850F0CF064DD592));
}

TEST(F64toa, OutputLength) {
  TestF64toa("1.0", 1);  // already tested in Basic
  TestF64toa("1.2", 1.2);
  TestF64toa("1.23", 1.23);
  TestF64toa("1.234", 1.234);
  TestF64toa("1.2345", 1.2345);
  TestF64toa("1.23456", 1.23456);
  TestF64toa("1.234567", 1.234567);
  TestF64toa("1.2345678", 1.2345678);  // already tested in Regression
  TestF64toa("1.23456789", 1.23456789);
  TestF64toa("1.234567895", 1.234567895);  // 1.234567890 would be trimmed
  TestF64toa("1.2345678901", 1.2345678901);
  TestF64toa("1.23456789012", 1.23456789012);
  TestF64toa("1.234567890123", 1.234567890123);
  TestF64toa("1.2345678901234", 1.2345678901234);
  TestF64toa("1.23456789012345", 1.23456789012345);
  TestF64toa("1.234567890123456", 1.234567890123456);
  TestF64toa("1.2345678901234567", 1.2345678901234567);

  // Test 32-bit chunking
  TestF64toa("4.294967294", 4.294967294);  // 2^32 - 2
  TestF64toa("4.294967295", 4.294967295);  // 2^32 - 1
  TestF64toa("4.294967296", 4.294967296);  // 2^32
  TestF64toa("4.294967297", 4.294967297);  // 2^32 + 1
  TestF64toa("4.294967298", 4.294967298);  // 2^32 + 2
}

// Test min, max shift values in shiftright128
TEST(F64toa, MinMaxShift) {
  const uint64_t maxMantissa = ((uint64_t)1 << 53) - 1;

  // 32-bit opt-size=0:  49 <= dist <= 50
  // 32-bit opt-size=1:  30 <= dist <= 50
  // 64-bit opt-size=0:  50 <= dist <= 50
  // 64-bit opt-size=1:  30 <= dist <= 50
  TestF64toa("1.7800590868057611e-307", ieeeParts2Double(false, 4, 0));
  // 32-bit opt-size=0:  49 <= dist <= 49
  // 32-bit opt-size=1:  28 <= dist <= 49
  // 64-bit opt-size=0:  50 <= dist <= 50
  // 64-bit opt-size=1:  28 <= dist <= 50
  TestF64toa("2.8480945388892175e-306",
             ieeeParts2Double(false, 6, maxMantissa));
  // 32-bit opt-size=0:  52 <= dist <= 53
  // 32-bit opt-size=1:   2 <= dist <= 53
  // 64-bit opt-size=0:  53 <= dist <= 53
  // 64-bit opt-size=1:   2 <= dist <= 53
  TestF64toa("2.446494580089078e-296", ieeeParts2Double(false, 41, 0));
  // 32-bit opt-size=0:  52 <= dist <= 52
  // 32-bit opt-size=1:   2 <= dist <= 52
  // 64-bit opt-size=0:  53 <= dist <= 53
  // 64-bit opt-size=1:   2 <= dist <= 53
  TestF64toa("4.8929891601781557e-296",
             ieeeParts2Double(false, 40, maxMantissa));

  // 32-bit opt-size=0:  57 <= dist <= 58
  // 32-bit opt-size=1:  57 <= dist <= 58
  // 64-bit opt-size=0:  58 <= dist <= 58
  // 64-bit opt-size=1:  58 <= dist <= 58
  TestF64toa("18014398509481984.0", ieeeParts2Double(false, 1077, 0));
  // 32-bit opt-size=0:  57 <= dist <= 57
  // 32-bit opt-size=1:  57 <= dist <= 57
  // 64-bit opt-size=0:  58 <= dist <= 58
  // 64-bit opt-size=1:  58 <= dist <= 58
  TestF64toa("36028797018963964.0", ieeeParts2Double(false, 1076, maxMantissa));
  // 32-bit opt-size=0:  51 <= dist <= 52
  // 32-bit opt-size=1:  51 <= dist <= 59
  // 64-bit opt-size=0:  52 <= dist <= 52
  // 64-bit opt-size=1:  52 <= dist <= 59
  TestF64toa("2.900835519859558e-216", ieeeParts2Double(false, 307, 0));
  // 32-bit opt-size=0:  51 <= dist <= 51
  // 32-bit opt-size=1:  51 <= dist <= 59
  // 64-bit opt-size=0:  52 <= dist <= 52
  // 64-bit opt-size=1:  52 <= dist <= 59
  TestF64toa("5.801671039719115e-216",
             ieeeParts2Double(false, 306, maxMantissa));

  // https://github.com/ulfjack/ryu/commit/19e44d16d80236f5de25800f56d82606d1b0b9#commitcomment-30146483
  // 32-bit opt-size=0:  49 <= dist <= 49
  // 32-bit opt-size=1:  44 <= dist <= 49
  // 64-bit opt-size=0:  50 <= dist <= 50
  // 64-bit opt-size=1:  44 <= dist <= 50
  TestF64toa("3.196104012172126e-27",
             ieeeParts2Double(false, 934, 0x000FA7161A4D6E0Cu));
}

TEST(F64toa, Integers) {
  // exact format
  TestF64toa("0.0", 0);
  TestF64toa("1.0", 1);
  TestF64toa("12.0", 12);
  TestF64toa("123.0", 123);
  TestF64toa("1234.0", 1234);
  TestF64toa("12345.0", 12345);
  TestF64toa("123456.0", 123456);
  TestF64toa("1234567.0", 1234567);
  TestF64toa("12345678.0", 12345678);
  TestF64toa("123456789.0", 123456789);
  TestF64toa("1234567890.0", 1234567890);
  TestF64toa("12345678901.0", 12345678901);
  TestF64toa("123456789012.0", 123456789012);
  TestF64toa("1234567890123.0", 1234567890123);
  TestF64toa("12345678901234.0", 12345678901234);
  TestF64toa("123456789012345.0", 123456789012345);
  TestF64toa("1234567890123456.0", 1234567890123456ull);
  // rounding integer
  TestF64toa("12345678901234568.0", (double)12345678901234567ull);
  TestF64toa("123456789012345680.0", (double)123456789012345678ull);
  TestF64toa("1234567890123456800.0", (double)1234567890123456789ull);
  TestF64toa("12345678901234567000.0", (double)12345678901234567890ull);
}

TEST(F64toa, ConnerCase) {
  // corner case for integer
  TestF64toa("9007199254740991.0", 9007199254740991.0);  // 2^53-1
  TestF64toa("9007199254740992.0", 9007199254740992.0);  // 2^53
  TestF64toa("9007199254740992.0", 9007199254740993.0);

  // corner case for format:
  // exponent format [0, 1e-6) and [1e21, +INF)
  // decimal format [1e-6, 1e21)
  TestF64toa("1.000000000000001e+21", 1.000000000000001e+21);
  TestF64toa("1e+21", 1.0e+21);
  TestF64toa("999999999999999900000.0", 9.999999999999999e+20);
  TestF64toa("0.000001", 1.0e-6);
  TestF64toa("0.000001000000000000001", 1.000000000000001e-6);
  TestF64toa("9.99999999999999e-7", 9.99999999999999e-7);
}

}  // namespace
