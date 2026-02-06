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
  ASSERT_NE(ptr, nullptr);
  a.Free(ptr);
  ptr = nullptr;
  MEMSTAT_ISEMPTY();
}

TEST(Allocator, SimpleAllocatorEdgeCases) {
  SimpleAllocator a;

  // Malloc(0) should return nullptr.
  EXPECT_EQ(a.Malloc(0), nullptr);

  // Realloc(..., new_size=0) should free and return nullptr.
  void *ptr = a.Malloc(8);
  ASSERT_NE(ptr, nullptr);
  EXPECT_EQ(a.Realloc(ptr, 8, 0), nullptr);
}

TEST(Allocator, AdaptiveChunkPolicyGrowth) {
  AdaptiveChunkPolicy cp(1024);

  // grow to next power-of-two (bounded by SONIC_ALLOCATOR_MAX_CHUNK_CAPACITY)
  EXPECT_EQ(cp.ChunkSize(3000), 4096u);
  EXPECT_EQ(cp.ChunkSize(5000), 8192u);

  const size_t max_cap = SONIC_ALLOCATOR_MAX_CHUNK_CAPACITY;
  // When request exceeds max_cap, returned size must still satisfy need.
  const size_t huge_need = max_cap * 2;
  EXPECT_EQ(cp.ChunkSize(huge_need), huge_need);

  // But internal min_chunk_size_ should be capped at max_cap.
  EXPECT_EQ(cp.ChunkSize(max_cap - 1), max_cap);
}

TEST(Allocator, MemoryPoolAllocatorMoveAndMapAllocator) {
  // Moved-from allocator should be a no-op on destruction.
  {
    MemoryPoolAllocator<> a;
    MemoryPoolAllocator<> b(std::move(a));
    (void)b;
  }

  // Exercise MapAllocator::deallocate via MemoryPoolAllocator::Free (no-op).
  {
    MemoryPoolAllocator<> pool;
    MapAllocator<int, MemoryPoolAllocator<>> ma(&pool);
    int *p = ma.allocate(1);
    ASSERT_NE(p, nullptr);
    ma.deallocate(p, 1);
  }
}

}  // namespace
