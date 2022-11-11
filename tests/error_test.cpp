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

#include "sonic/error.h"

#include <gtest/gtest.h>

#include <limits>

#include "sonic/sonic.h"

namespace {
using namespace sonic_json;
TEST(Errors, Parse) {
  std::string json = R"("hello\g")";
  Document dom;
  dom.Parse(json);
  SonicError err = dom.GetParseError();
  EXPECT_EQ(err, kParseErrorEscapedFormat);
  EXPECT_EQ(dom.GetErrorOffset(), 6);
  std::cout << ErrorMsg(err) << std::endl;
}

TEST(Errors, Serialze) {
  Document dom;
  dom.SetDouble(std::numeric_limits<double>::infinity());
  WriteBuffer wb;
  SonicError err = dom.Serialize(wb);
  EXPECT_EQ(err, kSerErrorInfinity);
  std::cout << ErrorMsg(err) << std::endl;
}
};  // namespace