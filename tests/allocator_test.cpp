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

#include <atomic>
#include <limits>
#include <thread>

#include "gtest/gtest.h"
#include "sonic/dom/dynamicnode.h"
#include "sonic/internal/stack.h"
#include "sonic/writebuffer.h"

// Let huge-allocation OOM tests return null under ASAN instead of aborting.
// Dead code in non-ASAN builds; ASAN_OPTIONS still overrides it.
extern "C" __attribute__((used)) const char* __asan_default_options() {
  return "allocator_may_return_null=1";
}

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
  void* ptr = a.Malloc(24);
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
  void* ptr = a.Malloc(8);
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
    int* p = ma.allocate(1);
    ASSERT_NE(p, nullptr);
    ma.deallocate(p, 1);
  }
}

TEST(Stack, ReservePreservesContents) {
  // Verify that Reserve correctly grows the buffer and preserves existing
  // data, and that Reserve(smaller) is a no-op.
  sonic_json::internal::Stack s(8);

  s.Push<char>('A');
  s.Push<char>('B');
  ASSERT_EQ(2u, s.Size());
  ASSERT_EQ('B', *s.Top<char>());

  // Reserve with current cap: must be a no-op.
  size_t old_cap = s.Capacity();
  s.Reserve(old_cap);
  EXPECT_EQ(old_cap, s.Capacity()) << "Reserve(<=cap) must not change Capacity";
  EXPECT_EQ('B', *s.Top<char>()) << "Reserve no-op must not touch Top";

  // Reserve with larger cap: must grow and preserve data.
  s.Reserve(old_cap * 4);
  EXPECT_GE(s.Capacity(), old_cap * 4)
      << "Reserve must grow to at least new_cap";
  EXPECT_EQ(2u, s.Size()) << "Reserve must not change Size";
  EXPECT_EQ('B', *s.Top<char>()) << "Reserve must preserve existing data";
}

// Use a large-but-bounded request that fails via allocator bookkeeping
// instead of asking ASan's malloc interceptor for absurd virtual sizes.
// This still exceeds the default pool chunk capacity and exercises the
// "failed allocation sets hadOom" path without polluting test output.
struct FailAfterFirstChunkAllocator {
  bool allow_ctor = true;
  void* Malloc(size_t n) {
    if (allow_ctor) {
      allow_ctor = false;
      return std::malloc(n);
    }
    return nullptr;
  }
  void* Realloc(void*, size_t, size_t) { return nullptr; }
  static void Free(void* p) { std::free(p); }
};

struct FailAllAllocator {
  void* Malloc(size_t) { return nullptr; }
  void* Realloc(void*, size_t, size_t) { return nullptr; }
  static void Free(void* p) { std::free(p); }
};

TEST(Allocator, MemoryPoolAllocatorHadOomSignalsFailedMalloc) {
  FailAfterFirstChunkAllocator base;
  MemoryPoolAllocator<FailAfterFirstChunkAllocator> pool(8, &base);
  EXPECT_FALSE(pool.HadOom());
  void* p = pool.Malloc(16);
  EXPECT_EQ(p, nullptr);
  EXPECT_TRUE(pool.HadOom());
  pool.ClearOom();
  EXPECT_FALSE(pool.HadOom());
}

TEST(Allocator, MemoryPoolAllocatorConstructorOomIsQueryableAndSafe) {
  FailAllAllocator base;
  MemoryPoolAllocator<FailAllAllocator> pool(8, &base);

  EXPECT_TRUE(pool.HadOom());
  EXPECT_EQ(nullptr, pool.Malloc(8));
  EXPECT_TRUE(pool.HadOom());
}

TEST(Allocator, MemoryPoolAllocatorUserBufferRejectsTooSmallAfterAlignment) {
  alignas(void*) char storage[64];
  MemoryPoolAllocator<> pool(storage + 1, 56);

  EXPECT_TRUE(pool.HadOom());
  EXPECT_EQ(0u, pool.Capacity());
  EXPECT_EQ(nullptr, pool.Malloc(8));

  MemoryPoolAllocator<> tiny(storage + 1, 1);
  EXPECT_TRUE(tiny.HadOom());
  EXPECT_EQ(0u, tiny.Capacity());
}

TEST(Allocator, MemoryPoolAllocatorRejectsOversizedMallocWithoutWraparound) {
  MemoryPoolAllocator<> pool(8);

  EXPECT_EQ(nullptr, pool.Malloc(std::numeric_limits<size_t>::max()));
  EXPECT_TRUE(pool.HadOom());
}

TEST(Allocator, MemoryPoolAllocatorRejectsChunkHeaderOverflow) {
  MemoryPoolAllocator<> pool(8);

  EXPECT_EQ(nullptr,
            pool.Malloc(std::numeric_limits<size_t>::max() - sizeof(void*)));
  EXPECT_TRUE(pool.HadOom());
}

TEST(Allocator, DNodeContainerOverflowMarksPoolOom) {
  MemoryPoolAllocator<> alloc;
  DNode<MemoryPoolAllocator<>> arr(kArray);

  alloc.ClearOom();
  arr.Reserve(std::numeric_limits<size_t>::max(), alloc);
  EXPECT_TRUE(alloc.HadOom());
  EXPECT_EQ(0u, arr.Capacity());

  DNode<MemoryPoolAllocator<>> obj(kObject);
  alloc.ClearOom();
  obj.MemberReserve(std::numeric_limits<size_t>::max(), alloc);
  EXPECT_TRUE(alloc.HadOom());
  EXPECT_EQ(0u, obj.Capacity());
}

TEST(Allocator, AdaptiveChunkPolicyHandlesHighBitNeedsWithoutShiftOverflow) {
  AdaptiveChunkPolicy cp(1024);

  EXPECT_EQ(size_t{1} << 63, cp.ChunkSize(size_t{1} << 63));
  EXPECT_EQ((size_t{1} << 63) + 1, cp.ChunkSize((size_t{1} << 63) + 1));
}

// Both MemoryPoolAllocator ctors place SharedData (incl. atomic<bool>
// hadOom) into raw storage.  Assert the flag reads false on a freshly
// constructed allocator before any Malloc call — guards against a
// regression where hadOom is left in an indeterminate state by ctor init.
TEST(Allocator, MemoryPoolAllocatorHadOomStartsFalseOnConstruction) {
  {
    MemoryPoolAllocator<> pool;
    EXPECT_FALSE(pool.HadOom());
  }
  {
    // Buffer ctor path: user-supplied storage, AlignBuffer instead of Malloc.
    alignas(alignof(std::max_align_t)) unsigned char buf[4096];
    MemoryPoolAllocator<> pool(buf, sizeof(buf));
    EXPECT_FALSE(pool.HadOom());
  }
}

TEST(Allocator, MemoryPoolAllocatorHadOomSharedAcrossCopies) {
  // Flag lives on SharedData so refcounted copies see coherent state.
  FailAfterFirstChunkAllocator base;
  MemoryPoolAllocator<FailAfterFirstChunkAllocator> a(8, &base);
  MemoryPoolAllocator<FailAfterFirstChunkAllocator> b(
      a);  // shares SharedData with a
  EXPECT_FALSE(a.HadOom());
  EXPECT_FALSE(b.HadOom());
  (void)b.Malloc(16);
  EXPECT_TRUE(a.HadOom());  // a sees b's failure
  EXPECT_TRUE(b.HadOom());
  a.ClearOom();
  EXPECT_FALSE(b.HadOom());
}

// Writer on one refcounted copy, reader on another. The per-instance
// SpinLock does not synchronize different copies, so hadOom must be
// atomic for this to be race-free.
TEST(Allocator, MemoryPoolAllocatorHadOomIsThreadSafeAcrossCopies) {
  FailAfterFirstChunkAllocator base;
  MemoryPoolAllocator<FailAfterFirstChunkAllocator> a(8, &base);
  MemoryPoolAllocator<FailAfterFirstChunkAllocator> b(a);  // shares SharedData

  std::atomic<bool> stop{false};
  std::thread writer([&] {
    for (int i = 0; i < 200 && !stop.load(); ++i) {
      (void)a.Malloc(16);  // sets hadOom
    }
  });
  std::thread reader([&] {
    for (int i = 0; i < 200 && !stop.load(); ++i) {
      (void)b.HadOom();  // observes hadOom
    }
  });
  writer.join();
  reader.join();
  stop.store(true);

  EXPECT_TRUE(b.HadOom());
  a.ClearOom();
  EXPECT_FALSE(b.HadOom());
}

TEST(Stack, ConstructorOomLeavesConsistentState) {
  // If the ctor's initial Reserve() fails, cap_ must not lie about the
  // (absent) buffer.  Otherwise Grow()'s guard `top_+cnt >= buf_+cap_` reads
  // as `1 >= cap_ + 0` and skips the re-allocation entirely, letting a
  // subsequent Push() dereference a null top_.
  constexpr size_t kHuge = (size_t{1} << 62);
  sonic_json::internal::Stack s(kHuge);

  if (s.Begin<char>() == nullptr) {
    EXPECT_EQ(0u, s.Capacity())
        << "Capacity must be 0 when the ctor could not allocate a buffer";
  }

  // And a subsequent Push() must still work — Grow() re-allocates on demand.
  s.Push<char>('X');
  ASSERT_NE(s.Begin<char>(), nullptr);
  EXPECT_EQ(1u, s.Size());
  EXPECT_EQ('X', *s.Top<char>());
}

TEST(Stack, PushSizeReportsOomAndDoesNotAdvanceTop) {
  sonic_json::internal::Stack s(8);
  ASSERT_FALSE(s.HadOom());
  ASSERT_NE(nullptr, s.Begin<char>());
  ASSERT_EQ(0u, s.Size());

  constexpr size_t kHuge = (size_t{1} << 62);
  char* p = s.PushSize<char>(kHuge);
  EXPECT_EQ(nullptr, p);
  EXPECT_TRUE(s.HadOom());
  EXPECT_EQ(0u, s.Size());
  EXPECT_EQ(8u, s.Capacity());

  s.ClearOom();
  EXPECT_FALSE(s.HadOom());
  p = s.PushSize<char>(1);
  ASSERT_NE(nullptr, p);
  *p = 'Y';
  EXPECT_EQ(1u, s.Size());
  EXPECT_EQ('Y', *s.Top<char>());
}

TEST(Stack, PushStringOverflowReportsOomAndDoesNotAdvanceTop) {
  sonic_json::internal::Stack s(8);
  ASSERT_FALSE(s.HadOom());
  ASSERT_NE(nullptr, s.Begin<char>());

  EXPECT_FALSE(s.Push("x", std::numeric_limits<size_t>::max()));
  EXPECT_TRUE(s.HadOom());
  EXPECT_EQ(0u, s.Size());
}

TEST(Stack, TypedPushAlignsAfterBytePush) {
  sonic_json::internal::Stack s(8);
  ASSERT_TRUE(s.Push<char>('x'));

  ASSERT_TRUE(s.Push<uint64_t>(0x0102030405060708ULL));
  const auto* p = s.Top<uint64_t>();
  EXPECT_EQ(0u, reinterpret_cast<uintptr_t>(p) % alignof(uint64_t));
  EXPECT_EQ(0x0102030405060708ULL, *p);
}

TEST(Stack, PushSizeAlignsAfterBytePush) {
  sonic_json::internal::Stack s(8);
  ASSERT_TRUE(s.Push<char>('x'));

  uint64_t* p = s.PushSize<uint64_t>(1);
  ASSERT_NE(nullptr, p);
  EXPECT_EQ(0u, reinterpret_cast<uintptr_t>(p) % alignof(uint64_t));
  *p = 0x1112131415161718ULL;
  EXPECT_EQ(p, s.Top<uint64_t>());
  EXPECT_EQ(0x1112131415161718ULL, *s.Top<uint64_t>());
}

TEST(WriteBuffer, ReserveOverflowReportsOom) {
  WriteBuffer wb(8);
  EXPECT_FALSE(wb.HadOom());

  EXPECT_FALSE(wb.Reserve(std::numeric_limits<size_t>::max()));
  EXPECT_TRUE(wb.HadOom());
  EXPECT_EQ(kErrorNoMem, wb.GetError());
  EXPECT_EQ(0u, wb.Size());
}

}  // namespace
