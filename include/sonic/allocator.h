// Tencent is pleased to support the open source community by making RapidJSON
// available.
//
// Copyright (C) 2015 THL A29 Limited, a Tencent company, and Milo Yip.
// Modification in 2022.
//
// Licensed under the MIT License (the "License"); you may not use this file
// except in compliance with the License. You may obtain a copy of the License
// at
//
// http://opensource.org/licenses/MIT
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations under
// the License.
//
// This file may have been modified by ByteDance authors. All ByteDance
// Modifications are Copyright 2022 ByteDance Authors.

#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <mutex>
#include <new>
#include <unordered_map>

#include "sonic/macro.h"

#define SONIC_DEFAULT_ALLOCATOR sonic_json::MemoryPoolAllocator<>

namespace sonic_json {

class SimpleAllocator {
 public:
  void* Malloc(size_t size) {
    void* ptr = nullptr;
    if (size != 0) {
      ptr = std::malloc(size);
    }
#ifdef SONIC_UNITTEST
    if (ptr != nullptr) {
      std::memset(ptr, 'z', size);
    }
#endif
    return ptr;
  }

  void* Realloc(void* old_ptr, size_t old_size, size_t new_size) {
    (void)(old_size);
    if (new_size == 0) {
      Free(old_ptr);
      return nullptr;
    }
    void* new_ptr = std::realloc(old_ptr, new_size);
    return new_ptr;
  }

  static void Free(void* ptr) { std::free(ptr); }

  bool operator==(const SimpleAllocator&) const { return true; }
  bool operator!=(const SimpleAllocator&) const { return false; }

 public:
  static constexpr bool kNeedFree = true;
};

#ifndef SONIC_ALIGN
#define SONIC_ALIGN(x) \
  (((x) + static_cast<size_t>(7u)) & ~static_cast<size_t>(7u))
#endif

// SpinLock is copied from https://rigtorp.se/spinlock/
class SpinLock {
 private:
  std::atomic<bool> lock_ = {false};

 public:
  void lock() {
    for (;;) {
      if (!lock_.exchange(true, std::memory_order_acquire)) {
        break;
      }
      while (lock_.load(std::memory_order_relaxed)) {
        // use pause or yield instruction will slow down lock acquisition
        // on contended locks.
#ifndef SONIC_SPINLOCK_NO_PAUSE

#if defined(__x86_64__) || defined(_M_AMD64)
        __builtin_ia32_pause();
#elif defined(__aarch64__) || defined(_M_ARM64)
        asm volatile("yield");
#endif

#endif
      }
    }
  }
  void unlock() { lock_.store(false, std::memory_order_release); }
};

#ifdef SONIC_LOCKED_ALLOCATOR
#define LOCK_GUARD std::lock_guard<SpinLock> guard(shared_->lock)
#else
#define LOCK_GUARD
#endif

#ifdef SONIC_ADAPTIVE_MEMORYPOOL
#define SONIC_MEMPOOL_CHUNK_POLICY AdaptiveChunkPolicy
#endif

#ifndef SONIC_MEMPOOL_CHUNK_POLICY
#define SONIC_MEMPOOL_CHUNK_POLICY SimpleChunkPolicy
#endif

#ifndef SONIC_ALLOCATOR_DEFAULT_CHUNK_CAPACITY
#define SONIC_ALLOCATOR_DEFAULT_CHUNK_CAPACITY (64 * 1024)
#endif

#ifndef SONIC_ALLOCATOR_MAX_CHUNK_CAPACITY
#define SONIC_ALLOCATOR_MAX_CHUNK_CAPACITY \
  SONIC_ALLOCATOR_DEFAULT_CHUNK_CAPACITY
#endif

#ifndef SONIC_ALLOCATOR_MIN_CHUNK_CAPACITY
#ifdef SONIC_ADAPTIVE_MEMORYPOOL
#define SONIC_ALLOCATOR_MIN_CHUNK_CAPACITY (1024)
#else
#define SONIC_ALLOCATOR_MIN_CHUNK_CAPACITY SONIC_ALLOCATOR_MAX_CHUNK_CAPACITY
#endif
#endif

class SimpleChunkPolicy {
 public:
  SimpleChunkPolicy(size_t chunk_cap = SONIC_ALLOCATOR_MAX_CHUNK_CAPACITY)
      : min_chunk_size_(chunk_cap) {}

  inline size_t ChunkSize(size_t need_alloc_size) {
    return min_chunk_size_ > need_alloc_size ? min_chunk_size_
                                             : need_alloc_size;
  }

 private:
  size_t min_chunk_size_;
};

class AdaptiveChunkPolicy {
 public:
  AdaptiveChunkPolicy(size_t chunk_cap = SONIC_ALLOCATOR_MIN_CHUNK_CAPACITY)
      : min_chunk_size_(chunk_cap) {}

  inline size_t ChunkSize(size_t need_alloc_size) {
    if (min_chunk_size_ < need_alloc_size &&
        min_chunk_size_ < SONIC_ALLOCATOR_MAX_CHUNK_CAPACITY) {
      size_t p = NextPowerOfTwoSaturated(need_alloc_size);
      min_chunk_size_ = p < SONIC_ALLOCATOR_MAX_CHUNK_CAPACITY
                            ? p
                            : SONIC_ALLOCATOR_MAX_CHUNK_CAPACITY;
    }
    return min_chunk_size_ > need_alloc_size ? min_chunk_size_
                                             : need_alloc_size;
  }

 private:
  static inline size_t NextPowerOfTwoSaturated(size_t size) {
    if (size <= 1) {
      return 1;
    }
    --size;
    for (size_t shift = 1; shift < sizeof(size_t) * 8; shift <<= 1) {
      size |= size >> shift;
    }
    if (size == std::numeric_limits<size_t>::max()) {
      return std::numeric_limits<size_t>::max();
    }
    return size + 1;
  }

  size_t min_chunk_size_;
};

template <typename BaseAllocator = SimpleAllocator,
          typename ChunkPolicy = SONIC_MEMPOOL_CHUNK_POLICY>
class MemoryPoolAllocator {
  //! Chunk header for perpending to each chunk.
  /*! Chunks are stored as a singly linked list.
   */
  struct ChunkHeader {
    size_t capacity;  //!< Capacity of the chunk in bytes (excluding the header
                      //!< itself).
    size_t size;      //!< Current size of allocated memory in bytes.
    ChunkHeader* next;  //!< Next chunk in the linked list.
  };

  struct SharedData {
    ChunkHeader* chunkHead;  //!< Head of the chunk linked-list. Only the head
                             //!< chunk serves allocation.
    BaseAllocator*
        ownBaseAllocator;  //!< base allocator created by this object.
    std::atomic<size_t> refcount;
    bool ownBuffer;
    //!< Sticky OOM flag shared across refcounted copies.  Atomic because
    //!< the per-instance SpinLock does not synchronize different copies.
    std::atomic<bool> hadOom;
    SpinLock lock;
  };

  static const size_t SIZEOF_SHARED_DATA = SONIC_ALIGN(sizeof(SharedData));
  static const size_t SIZEOF_CHUNK_HEADER = SONIC_ALIGN(sizeof(ChunkHeader));

  static inline ChunkHeader* GetChunkHead(SharedData* shared) {
    return reinterpret_cast<ChunkHeader*>(reinterpret_cast<uint8_t*>(shared) +
                                          SIZEOF_SHARED_DATA);
  }
  static inline uint8_t* GetChunkBuffer(SharedData* shared) {
    return reinterpret_cast<uint8_t*>(shared->chunkHead) + SIZEOF_CHUNK_HEADER;
  }

 public:
  static const bool kNeedFree =
      false;  //!< Tell users that no need to call Free() with this allocator.
              //!< (concept Allocator)
  static const bool kRefCounted =
      true;  //!< Tell users that this allocator is reference counted on copy

  //! Constructor with chunkSize.
  /*! \param chunkSize The size of memory chunk. The default is
     kDefaultChunkSize. \param baseAllocator The allocator for allocating memory
     chunks.
  */
  explicit MemoryPoolAllocator(
      size_t chunkSize = SONIC_ALLOCATOR_MIN_CHUNK_CAPACITY,
      BaseAllocator* baseAllocator = 0)
      : cp_(chunkSize),
        baseAllocator_(baseAllocator ? baseAllocator
                                     : new (std::nothrow) BaseAllocator()),
        shared_(0),
        ownBaseAllocatorWhenInvalid_(baseAllocator == 0) {
    if (!baseAllocator_) {
      ownBaseAllocatorWhenInvalid_ = false;
      return;
    }
    shared_ = static_cast<SharedData*>(
        baseAllocator_->Malloc(SIZEOF_SHARED_DATA + SIZEOF_CHUNK_HEADER));
    if (!shared_) {
      return;
    }
    InitializeShared(baseAllocator ? 0 : baseAllocator_, true, 0);
    ownBaseAllocatorWhenInvalid_ = false;
  }

  //! Constructor with user-supplied buffer.
  /*! The user buffer will be used firstly. When it is full, memory pool
     allocates new chunk with chunk size.

      The user buffer will not be deallocated when this allocator is destructed.

      \param buffer User supplied buffer.
      \param size Size of the buffer in bytes. It must at least larger than
     sizeof(ChunkHeader). \param chunkSize The size of memory chunk. The default
     is kDefaultChunkSize. \param baseAllocator The allocator for allocating
     memory chunks.
  */
  MemoryPoolAllocator(void* buffer, size_t size,
                      size_t chunkSize = SONIC_ALLOCATOR_MIN_CHUNK_CAPACITY,
                      BaseAllocator* baseAllocator = 0)
      : cp_(chunkSize),
        baseAllocator_(baseAllocator ? baseAllocator
                                     : new (std::nothrow) BaseAllocator()),
        shared_(nullptr),
        ownBaseAllocatorWhenInvalid_(baseAllocator == 0) {
    if (sonic_unlikely(buffer == nullptr)) {
      return;
    }
    shared_ = static_cast<SharedData*>(AlignBuffer(buffer, size));
    if (sonic_unlikely(shared_ == nullptr ||
                       size < SIZEOF_SHARED_DATA + SIZEOF_CHUNK_HEADER)) {
      shared_ = nullptr;
      return;
    }
    InitializeShared(baseAllocator ? 0 : baseAllocator_, false,
                     size - SIZEOF_SHARED_DATA - SIZEOF_CHUNK_HEADER);
    shared_->chunkHead->capacity =
        size - SIZEOF_SHARED_DATA - SIZEOF_CHUNK_HEADER;
    ownBaseAllocatorWhenInvalid_ = false;
  }

  MemoryPoolAllocator(const MemoryPoolAllocator& rhs) noexcept
      : cp_(rhs.cp_),
        baseAllocator_(rhs.shared_ ? rhs.baseAllocator_ : 0),
        shared_(rhs.shared_),
        ownBaseAllocatorWhenInvalid_(false) {
    if (shared_) {
      sonic_assert(shared_->refcount.load(std::memory_order_acquire) > 0);
      shared_->refcount.fetch_add(1, std::memory_order_acq_rel);
    }
  }
  MemoryPoolAllocator& operator=(const MemoryPoolAllocator& rhs) noexcept {
    if (this == &rhs) {
      return *this;
    }
    if (rhs.shared_) {
      sonic_assert(rhs.shared_->refcount.load(std::memory_order_acquire) > 0);
      rhs.shared_->refcount.fetch_add(1, std::memory_order_acq_rel);
    }
    Release();
    baseAllocator_ = rhs.shared_ ? rhs.baseAllocator_ : 0;
    cp_ = rhs.cp_;
    shared_ = rhs.shared_;
    ownBaseAllocatorWhenInvalid_ = false;
    return *this;
  }

  MemoryPoolAllocator(MemoryPoolAllocator&& rhs) noexcept
      : cp_(rhs.cp_),
        baseAllocator_(rhs.baseAllocator_),
        shared_(rhs.shared_),
        ownBaseAllocatorWhenInvalid_(rhs.ownBaseAllocatorWhenInvalid_) {
    sonic_assert(!rhs.shared_ ||
                 rhs.shared_->refcount.load(std::memory_order_acquire) > 0);
    rhs.shared_ = 0;
    rhs.baseAllocator_ = 0;
    rhs.ownBaseAllocatorWhenInvalid_ = false;
  }
  MemoryPoolAllocator& operator=(MemoryPoolAllocator&& rhs) noexcept {
    if (this == &rhs) {
      return *this;
    }
    sonic_assert(!rhs.shared_ ||
                 rhs.shared_->refcount.load(std::memory_order_acquire) > 0);
    Release();
    baseAllocator_ = rhs.baseAllocator_;
    cp_ = rhs.cp_;
    shared_ = rhs.shared_;
    ownBaseAllocatorWhenInvalid_ = rhs.ownBaseAllocatorWhenInvalid_;
    rhs.shared_ = 0;
    rhs.baseAllocator_ = 0;
    rhs.ownBaseAllocatorWhenInvalid_ = false;
    return *this;
  }

  //! Destructor.
  /*! This deallocates all memory chunks, excluding the user-supplied buffer.
   */
  ~MemoryPoolAllocator() noexcept { Release(); }

 private:
  void Release() noexcept {
    if (!shared_) {
      if (ownBaseAllocatorWhenInvalid_) {
        delete baseAllocator_;
      }
      baseAllocator_ = nullptr;
      ownBaseAllocatorWhenInvalid_ = false;
      return;
    }
    if (shared_->refcount.load(std::memory_order_acquire) > 1 &&
        shared_->refcount.fetch_sub(1, std::memory_order_acq_rel) > 1) {
      shared_ = nullptr;
      baseAllocator_ = nullptr;
      ownBaseAllocatorWhenInvalid_ = false;
      return;
    }
    Clear();
    BaseAllocator* a = shared_->ownBaseAllocator;
    using AtomicBool = std::atomic<bool>;
    using AtomicSize = std::atomic<size_t>;
    shared_->lock.~SpinLock();
    shared_->hadOom.~AtomicBool();
    shared_->refcount.~AtomicSize();
    if (shared_->ownBuffer) {
      baseAllocator_->Free(shared_);
    }
    delete a;
    shared_ = nullptr;
    baseAllocator_ = nullptr;
    ownBaseAllocatorWhenInvalid_ = false;
  }

 public:
  //! Deallocates all memory chunks, excluding the first/user one.
  void Clear() noexcept {
    if (!shared_) {
      return;
    }
    sonic_assert(shared_->refcount.load(std::memory_order_acquire) > 0);
    LOCK_GUARD;
    for (;;) {
      ChunkHeader* c = shared_->chunkHead;
      if (!c->next) {
        break;
      }
      shared_->chunkHead = c->next;
      baseAllocator_->Free(c);
    }
    shared_->chunkHead->size = 0;
  }

  //! Computes the total capacity of allocated memory chunks.
  /*! \return total capacity in bytes.
   */
  size_t Capacity() const noexcept {
    if (!shared_) {
      return 0;
    }
    sonic_assert(shared_->refcount.load(std::memory_order_acquire) > 0);
    LOCK_GUARD;
    size_t capacity = 0;
    for (ChunkHeader* c = shared_->chunkHead; c != 0; c = c->next)
      capacity += c->capacity;
    return capacity;
  }

  //! Computes the memory blocks allocated.
  /*! \return total used bytes.
   */
  size_t Size() const noexcept {
    if (!shared_) {
      return 0;
    }
    sonic_assert(shared_->refcount.load(std::memory_order_acquire) > 0);
    LOCK_GUARD;
    size_t size = 0;
    for (ChunkHeader* c = shared_->chunkHead; c != 0; c = c->next)
      size += c->size;
    return size;
  }

  //! Whether the allocator is shared.
  /*! \return true or false.
   */
  bool Shared() const noexcept {
    if (!shared_) {
      return false;
    }
    sonic_assert(shared_->refcount.load(std::memory_order_acquire) > 0);
    return shared_->refcount.load(std::memory_order_acquire) > 1;
  }

  //! Allocates a memory block. (concept Allocator)
  void* Malloc(size_t size) {
    if (!shared_) {
      return NULL;
    }
    sonic_assert(shared_->refcount.load(std::memory_order_acquire) > 0);
    if (!size) return NULL;

    if (sonic_unlikely(!AlignSize(size, &size))) {
      SetOom();
      return NULL;
    }
    LOCK_GUARD;
    if (sonic_unlikely(size > shared_->chunkHead->capacity -
                                  shared_->chunkHead->size)) {
      if (!AddChunk(cp_.ChunkSize(size))) {
        SetOom();
        return NULL;
      }
    }

    void* buffer = GetChunkBuffer(shared_) + shared_->chunkHead->size;
    shared_->chunkHead->size += size;
    return buffer;
  }

  //! Resizes a memory block (concept Allocator)
  void* Realloc(void* originalPtr, size_t originalSize, size_t newSize) {
    if (originalPtr == 0) return Malloc(newSize);

    if (!shared_) {
      return nullptr;
    }
    sonic_assert(shared_->refcount.load(std::memory_order_acquire) > 0);
    if (newSize == 0) return nullptr;

    if (sonic_unlikely(!AlignSize(originalSize, &originalSize) ||
                       !AlignSize(newSize, &newSize))) {
      SetOom();
      return nullptr;
    }

    // Do not shrink if new size is smaller than original
    if (originalSize >= newSize) return originalPtr;

    // Simply expand it if it is the last allocation and there is sufficient
    // space
    {
      LOCK_GUARD;
      if (originalPtr ==
          GetChunkBuffer(shared_) + shared_->chunkHead->size - originalSize) {
        size_t increment = static_cast<size_t>(newSize - originalSize);
        if (increment <=
            shared_->chunkHead->capacity - shared_->chunkHead->size) {
          shared_->chunkHead->size += increment;
          return originalPtr;
        }
      }
    }

    // Realloc process: allocate and copy memory, do not free original buffer.
    if (void* newBuffer = Malloc(newSize)) {
      if (originalSize) std::memcpy(newBuffer, originalPtr, originalSize);
      return newBuffer;
    }
    // Mark OOM even on the Malloc-copy fallback so the flag is set
    // regardless of which internal path actually failed.
    SetOom();
    return nullptr;
  }

  // Lets callers distinguish an OOM from a logical null (e.g. Malloc(0)).
  bool HadOom() const {
    if (!shared_) {
      return true;
    }
    sonic_assert(shared_->refcount.load(std::memory_order_acquire) > 0);
    return shared_->hadOom.load(std::memory_order_acquire);
  }
  void ClearOom() {
    if (!shared_) {
      return;
    }
    sonic_assert(shared_->refcount.load(std::memory_order_acquire) > 0);
    shared_->hadOom.store(false, std::memory_order_release);
  }

  void MarkOom() { SetOom(); }

  //! Frees a memory block (concept Allocator)
  static void Free(void* ptr) noexcept { (void)ptr; }  // Do nothing

  // ! Compare (equality) with another MemoryPoolAllocator
  bool operator==(const MemoryPoolAllocator& rhs) const noexcept {
    sonic_assert(!shared_ ||
                 shared_->refcount.load(std::memory_order_acquire) > 0);
    sonic_assert(!rhs.shared_ ||
                 rhs.shared_->refcount.load(std::memory_order_acquire) > 0);
    return shared_ == rhs.shared_;
  }
  // ! Compare (inequality) with another MemoryPoolAllocator
  bool operator!=(const MemoryPoolAllocator& rhs) const noexcept {
    return !operator==(rhs);
  }

 private:
  //! Creates a new chunk.
  /*! \param capacity Capacity of the chunk in bytes.
      \return true if success.
  */
  bool AddChunk(size_t capacity) {
    if (!shared_) {
      return false;
    }
    if (capacity > std::numeric_limits<size_t>::max() - SIZEOF_CHUNK_HEADER) {
      SetOom();
      return false;
    }
    if (!baseAllocator_) {
      baseAllocator_ = new (std::nothrow) BaseAllocator();
      if (!baseAllocator_) {
        SetOom();
        return false;
      }
      shared_->ownBaseAllocator = baseAllocator_;
    }
    if (ChunkHeader* chunk = static_cast<ChunkHeader*>(
            baseAllocator_->Malloc(SIZEOF_CHUNK_HEADER + capacity))) {
      chunk->capacity = capacity;
      chunk->size = 0;
      chunk->next = shared_->chunkHead;
      shared_->chunkHead = chunk;
      return true;
    }
    SetOom();
    return false;
  }

  void InitializeShared(BaseAllocator* ownBaseAllocator, bool ownBuffer,
                        size_t capacity) {
    new (&shared_->hadOom) std::atomic<bool>(false);
    new (&shared_->refcount) std::atomic<size_t>(1);
    new (&shared_->lock) SpinLock();
    shared_->ownBaseAllocator = ownBaseAllocator;
    shared_->chunkHead = GetChunkHead(shared_);
    shared_->chunkHead->capacity = capacity;
    shared_->chunkHead->size = 0;
    shared_->chunkHead->next = 0;
    shared_->ownBuffer = ownBuffer;
  }

  static inline bool AlignSize(size_t size, size_t* aligned) {
    if (size > std::numeric_limits<size_t>::max() - 7) {
      return false;
    }
    *aligned = SONIC_ALIGN(size);
    return true;
  }

  void SetOom() {
    if (shared_) {
      shared_->hadOom.store(true, std::memory_order_release);
    }
  }

  static inline void* AlignBuffer(void* buf, size_t& size) {
    sonic_assert(buf != 0);
    const uintptr_t mask = sizeof(void*) - 1;
    const uintptr_t ubuf = reinterpret_cast<uintptr_t>(buf);
    if (sonic_unlikely(ubuf & mask)) {
      const uintptr_t abuf = (ubuf + mask) & ~mask;
      const size_t delta = static_cast<size_t>(abuf - ubuf);
      if (sonic_unlikely(delta > size)) {
        size = 0;
        return nullptr;
      }
      buf = reinterpret_cast<void*>(abuf);
      size -= delta;
    }
    return buf;
  }

  // size_t chunk_capacity_;  //!< The minimum capacity of chunk when they are
  //!< allocated.
  ChunkPolicy cp_;  //! chunk capacity policy
  BaseAllocator*
      baseAllocator_;   //!< base allocator for allocating memory chunks.
  SharedData* shared_;  //!< The shared data of the allocator
  bool ownBaseAllocatorWhenInvalid_;
};

template <typename T, typename BaseAllocatorType>
class MapAllocator {
 public:
  typedef size_t size_type;
  typedef ptrdiff_t difference_type;
  typedef T* pointer;
  typedef const T* const_pointer;
  typedef T& reference;
  typedef const T& const_reference;
  typedef T value_type;

  MapAllocator(BaseAllocatorType* a) : alloc_(a) {}
  MapAllocator(const MapAllocator& rhs) : alloc_(rhs.alloc_) {}

  pointer allocate(size_type n, const void* = nullptr) {
    if (alloc_ == nullptr || n == 0) return nullptr;
    if (n > std::numeric_limits<size_type>::max() / sizeof(T)) {
      return nullptr;
    }
    return static_cast<pointer>(alloc_->Malloc(n * sizeof(T)));
  }

  void deallocate(void* p, size_type) { alloc_->Free(p); }

  pointer address(reference x) const { return &x; }
  const_pointer address(const_reference x) const { return &x; }

  MapAllocator<T, BaseAllocatorType>& operator=(const MapAllocator& rhs) {
    alloc_ = rhs.alloc_;
    return *this;
  }

  void construct(pointer p, const T& val) { new ((T*)p) T(val); }
  void destroy(pointer p) { p->~T(); }
  size_type max_size() const { return size_t(-1); }

  template <typename U>
  struct rebind {
    typedef MapAllocator<U, BaseAllocatorType> other;
  };

  template <typename U>
  MapAllocator(const MapAllocator<U, BaseAllocatorType>& rhs)
      : alloc_(rhs.alloc_) {}

  template <typename U>
  MapAllocator<T, BaseAllocatorType>& operator=(
      const MapAllocator<U, BaseAllocatorType>& rhs) {
    alloc_ = rhs.alloc_;
    return *this;
  }

  BaseAllocatorType* alloc_;
};

}  // namespace sonic_json
