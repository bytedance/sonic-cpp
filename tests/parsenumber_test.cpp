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

#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "sonic/dom/dynamicnode.h"
#include "sonic/dom/generic_document.h"

namespace {

using namespace sonic_json;

void TestParseSigned(int64_t num, const std::string& input) {
  Document doc;
  doc.Parse(input.data(), input.size());
  EXPECT_FALSE(doc.HasParseError()) << input;
  EXPECT_TRUE(doc.IsInt64()) << input;
  EXPECT_EQ(num, doc.GetInt64()) << input;
}

void TestParseUnsigned(uint64_t num, const std::string& input) {
  Document doc;
  doc.Parse(input.data(), input.size());
  EXPECT_FALSE(doc.HasParseError()) << input;
  EXPECT_TRUE(doc.IsUint64()) << input;
  EXPECT_EQ(num, doc.GetUint64()) << input;
}

void TestParseDouble(double num, const std::string& input) {
  {
    Document doc;
    doc.Parse(input.data(), input.size());
    EXPECT_FALSE(doc.HasParseError()) << input;
    EXPECT_TRUE(doc.IsDouble()) << input;
    EXPECT_DOUBLE_EQ(num, doc.GetDouble()) << input;
  }
  // test native atof
  { EXPECT_DOUBLE_EQ(num, internal::AtofNative(input.data(), input.size())); }
}

void TestParseError(const std::string& input, size_t off, SonicError err) {
  Document doc;
  doc.Parse(input.data(), input.size());
  EXPECT_TRUE(doc.HasParseError()) << input;
  EXPECT_EQ(doc.GetParseError(), err) << input;
  // TODO: test offset
  (void)(off);
}

void TestParseInf(size_t off, const std::string& input) {
  TestParseError(input, off, kParseErrorInfinity);
}

void TestParseInval(size_t off, const std::string& input) {
  TestParseError(input, off, kParseErrorInvalidChar);
}

TEST(ParserTest, ParseNumber) {
  TestParseUnsigned(0ULL, "0");
  TestParseUnsigned(123ULL, "123");
  TestParseUnsigned(9223372036854775807ULL,
                    "9223372036854775807");  // LLONG_MAX
  TestParseUnsigned(18446744073709551615ULL,
                    "18446744073709551615");  // ULLONG_MAX

  TestParseSigned(-1LL, "-1");
  TestParseSigned(-123LL, "-123");
  TestParseSigned(-9223372036854775808ULL,
                  "-9223372036854775808");  // LLONG_MIN

  TestParseDouble(-0.0, "-0.0");
  TestParseDouble(0.0, "0.0");
  TestParseDouble(0.1, "0.1");
  TestParseDouble(0.01, "0.01");
  TestParseDouble(-0.001, "-0.001");
  TestParseDouble(1e-6, "0.000001");
  TestParseDouble(-1.0, "-1.0");
  TestParseDouble(1.0, "1.00");
  TestParseDouble(-1.0, "-1.000");
  TestParseDouble(-1.1, "-1.1");
  TestParseDouble(10.1, "10.1");
  TestParseDouble(1000.0001, "1000.0001");
  TestParseDouble(-123e0, "-123e0");
  TestParseDouble(-0.0e+0, "-0.0e+0");
  TestParseDouble(1.23e-99, "1.23e-99");
  TestParseDouble(5.70899e+45, "5.70899e+45");
  TestParseDouble(1.01412e+31, "1.01412e+31");
  TestParseDouble(35184372088832.00390625, "35184372088832.00390625");
  TestParseDouble(72057594037927935E0, "72057594037927935E0");
  TestParseDouble(0.12345678901234567, "0.12345678901234567");
  TestParseDouble(
      18446744073709551616.0,
      "18446744073709551616");  // ULLONG_MAX + 1, should overflow into double
  TestParseDouble(
      -9223372036854775809.0,
      "-9223372036854775809");  // LLONG_MIN - 1, should overflow into double
  TestParseDouble(0.0, "123e-100000");
  TestParseDouble(1234567890123456789012345.0, "1234567890123456789012345");
  TestParseDouble(-1234567890123456789012345.0, "-1234567890123456789012345");
  TestParseDouble(-1234567890123456789012345.0, "-1234567890123456789012345");
}

void ParseFloatInFiles() {
  std::vector<std::string> files = {"./testdata/num/float-1.txt",
                                    "./testdata/num/float-8.txt"};

  for (auto& f : files) {
    std::ifstream is(f);
    if (is.is_open()) {
      std::string number;
      while (std::getline(is, number)) {
        if (number.size() > 0 && number[0] != '#' && number[0] != '\n' &&
            number[0] != '\t' && number[0] != '\b') {
          double df = std::atof(number.c_str());
          TestParseDouble(df, number.c_str());
        }
      }
    } else {
      std::cout << "Error open files: " << f << std::endl;
    }
    is.close();
  }
}

TEST(ParserTest, ParseTestData) { ParseFloatInFiles(); }

TEST(ParserTest, ParseFloatExponent) {
  // zeros
  TestParseDouble(0, "0e0");
  TestParseDouble(0, "0e+0");
  TestParseDouble(0, "0e-0");
  TestParseDouble(0, "-0e0");
  TestParseDouble(0, "-0e+0");
  TestParseDouble(0, "0.0e0");
  TestParseDouble(0, "0.0e0123");
  TestParseDouble(0, "-0.00e+0456");
  TestParseDouble(0, "-0e+456");

  // zero exponets
  TestParseDouble(1, "1e0");
  TestParseDouble(12, "12e-00");

  // basic
  TestParseDouble(-1.2, "-12e-1");
  TestParseDouble(123, "12.3e+1");
  TestParseDouble(1e23, "1e23");
  TestParseDouble(1e-6, "1e-6");
  TestParseDouble(-2e-6, "-2e-6");
  TestParseDouble(-2e10, "-2.0E+10");
  TestParseDouble(1.2345e41, "12345E37");
  TestParseDouble(-1.2345e41, "-12345E37");
  TestParseDouble(-1.2345e100, "-1.2345E100");
  TestParseDouble(-1.2345e-100, "-1.2345E-100");
}

TEST(ParserTest, LargeExponent) {
  TestParseDouble(0, "0e+12345678");
  TestParseDouble(0, "-0e+12345678");

  TestParseDouble(0, "1e-12345678");
  TestParseDouble(0, "-1e-12345678");

  TestParseDouble(1.7976931348623157e308, "1.7976931348623157e+308");
  TestParseDouble(-1.7976931348623157e308, "-1.7976931348623157e+308");
  TestParseDouble(4.630813248087435e+307, "4.630813248087435e+307");
}

TEST(ParserTest, LongMantissa) {
  TestParseDouble(22.22222222222222,
                  ("22.22" + std::string(4000, '2')).c_str());
  TestParseDouble(22.22222222222222, "22.22222222222222");
  TestParseDouble(22.22222222222223, "22.22222222222223");
}

TEST(ParserTest, RoundUp) {
  TestParseDouble(1, "1.00000000000000011102230246251565404236316680908203125");
  TestParseDouble(1, "1.00000000000000011102230246251565404236316680908203124");

  TestParseDouble(1.0000000000000002,
                  "1.00000000000000011102230246251565404236316680908203126");
  TestParseDouble(1.0000000000000002,
                  ("1.00000000000000011102230246251565404236316680908203125" +
                   std::string(10000, '0') + "1")
                      .c_str());

  TestParseDouble(1.0000000000000004,
                  "1.00000000000000033306690738754696212708950042724609375");

  TestParseDouble(1.0905441441816093e+30, "1090544144181609348671888949248");
  TestParseDouble(1.0905441441816094e+30, "1090544144181609348835077142190");
}

TEST(ParserTest, ParseInvalidNumber) {
  TestParseInf(7, "1e+9999");
  TestParseInf(26, "-1234567890123456789e+9999");
  TestParseInf(26, "-1234567890123456789e+9999.");

  TestParseInval(0, "+");
  TestParseInval(0, "+0");  // +0 is not allowed in JSON RFC
  TestParseInval(0, "e");
  TestParseInval(0, "E");
  TestParseInval(0, ".");
  TestParseInval(1, "-");
  TestParseInval(1, "00");
  TestParseInval(1, "01");
  TestParseInval(2, "0.");
  TestParseInval(1, "0-");
  TestParseInval(2, "0e");
  TestParseInval(3, "0e-");
  TestParseInval(5, "0.0e+");
  TestParseInval(4, "1.0e");
  TestParseInval(6, "-1.0e+");
  TestParseInval(6, "-1.0e-");
  TestParseInval(8, "-1234567x");
  TestParseInval(9, "-123456.7x");
  TestParseInval(8, "-1234567.");
  TestParseInval(
      8, "1234567 123");  // Only support parse single JSON value one time
}

}  // namespace
