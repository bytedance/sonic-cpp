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

#include "sonic/allocator.h"

#include "gtest/gtest.h"

namespace {

using namespace sonic_json;

#ifdef SONIC_MEMSTAT
#define MEMSTAT_ISEMPTY() EXPECT_TRUE(MemStat::Instance().stat.empty())
#define MEMSTAT_NOTEMPTY() EXPECT_FALSE(MemStat::Instance().stat.empty())
#else
#define MEMSTAT_ISEMPTY()
#define MEMSTAT_NOTEMPTY()
#endif

TEST(Allocator, Free) {
  SimpleAllocator a;
  MEMSTAT_ISEMPTY();
  void *ptr = a.Malloc(24);
  MEMSTAT_NOTEMPTY();
  ptr = a.Realloc(ptr, 24, 48);
  MEMSTAT_NOTEMPTY();
  ptr = a.Realloc(ptr, 48, 96);
  a.Free(ptr);
  MEMSTAT_ISEMPTY();
  EXPECT_NE(ptr, nullptr);
}

}  // namespace