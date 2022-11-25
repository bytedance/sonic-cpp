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
#include <cstring>
#include <iostream>
#include <mutex>
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

#ifndef SONIC_ALLOCATOR_DEFAULT_CHUNK_CAPACITY
#define SONIC_ALLOCATOR_DEFAULT_CHUNK_CAPACITY (64 * 1024)
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
        __builtin_ia32_pause();
      }
    }
  }
  void unlock() { lock_.store(false, std::memory_order_release); }
};

#ifdef SONIC_LOCKED_ALLOCATOR
#define LOCK_GUARD std::lock_guard<SpinLock> guard(lock_);
#else
#define LOCK_GUARD
#endif

template <typename BaseAllocator = SimpleAllocator>
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
    size_t refcount;
    bool ownBuffer;
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

  static const size_t kDefaultChunkCapacity =
      SONIC_ALLOCATOR_DEFAULT_CHUNK_CAPACITY;  //!< Default chunk capacity.

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
  explicit MemoryPoolAllocator(size_t chunkSize = kDefaultChunkCapacity,
                               BaseAllocator* baseAllocator = 0)
      : chunk_capacity_(chunkSize),
        baseAllocator_(baseAllocator ? baseAllocator : new BaseAllocator()),
        shared_(static_cast<SharedData*>(
            baseAllocator_ ? baseAllocator_->Malloc(SIZEOF_SHARED_DATA +
                                                    SIZEOF_CHUNK_HEADER)
                           : 0)) {
    sonic_assert(baseAllocator_ != 0);
    sonic_assert(shared_ != 0);
    if (baseAllocator) {
      shared_->ownBaseAllocator = 0;
    } else {
      shared_->ownBaseAllocator = baseAllocator_;
    }
    shared_->chunkHead = GetChunkHead(shared_);
    shared_->chunkHead->capacity = 0;
    shared_->chunkHead->size = 0;
    shared_->chunkHead->next = 0;
    shared_->ownBuffer = true;
    shared_->refcount = 1;
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
                      size_t chunkSize = kDefaultChunkCapacity,
                      BaseAllocator* baseAllocator = 0)
      : chunk_capacity_(chunkSize),
        baseAllocator_(baseAllocator),
        shared_(static_cast<SharedData*>(AlignBuffer(buffer, size))) {
    sonic_assert(size >= SIZEOF_SHARED_DATA + SIZEOF_CHUNK_HEADER);
    shared_->chunkHead = GetChunkHead(shared_);
    shared_->chunkHead->capacity =
        size - SIZEOF_SHARED_DATA - SIZEOF_CHUNK_HEADER;
    shared_->chunkHead->size = 0;
    shared_->chunkHead->next = 0;
    shared_->ownBaseAllocator = 0;
    shared_->ownBuffer = false;
    shared_->refcount = 1;
  }

  MemoryPoolAllocator(const MemoryPoolAllocator& rhs) noexcept
      : chunk_capacity_(rhs.chunk_capacity_),
        baseAllocator_(rhs.baseAllocator_),
        shared_(rhs.shared_) {
    sonic_assert(shared_->refcount > 0);
    ++shared_->refcount;
  }
  MemoryPoolAllocator& operator=(const MemoryPoolAllocator& rhs) noexcept {
    sonic_assert(rhs.shared_->refcount > 0);
    ++rhs.shared_->refcount;
    this->~MemoryPoolAllocator();
    baseAllocator_ = rhs.baseAllocator_;
    chunk_capacity_ = rhs.chunk_capacity_;
    shared_ = rhs.shared_;
    return *this;
  }

  MemoryPoolAllocator(MemoryPoolAllocator&& rhs) noexcept
      : chunk_capacity_(rhs.chunk_capacity_),
        baseAllocator_(rhs.baseAllocator_),
        shared_(rhs.shared_) {
    sonic_assert(rhs.shared_->refcount > 0);
    rhs.shared_ = 0;
  }
  MemoryPoolAllocator& operator=(MemoryPoolAllocator&& rhs) noexcept {
    sonic_assert(rhs.shared_->refcount > 0);
    this->~MemoryPoolAllocator();
    baseAllocator_ = rhs.baseAllocator_;
    chunk_capacity_ = rhs.chunk_capacity_;
    shared_ = rhs.shared_;
    rhs.shared_ = 0;
    return *this;
  }

  //! Destructor.
  /*! This deallocates all memory chunks, excluding the user-supplied buffer.
   */
  ~MemoryPoolAllocator() noexcept {
    if (!shared_) {
      // do nothing if moved
      return;
    }
    if (shared_->refcount > 1) {
      --shared_->refcount;
      return;
    }
    Clear();
    BaseAllocator* a = shared_->ownBaseAllocator;
    if (shared_->ownBuffer) {
      baseAllocator_->Free(shared_);
    }
    delete a;
  }

  //! Deallocates all memory chunks, excluding the first/user one.
  void Clear() noexcept {
    sonic_assert(shared_->refcount > 0);
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
    sonic_assert(shared_->refcount > 0);
    size_t capacity = 0;
    for (ChunkHeader* c = shared_->chunkHead; c != 0; c = c->next)
      capacity += c->capacity;
    return capacity;
  }

  //! Computes the memory blocks allocated.
  /*! \return total used bytes.
   */
  size_t Size() const noexcept {
    sonic_assert(shared_->refcount > 0);
    size_t size = 0;
    for (ChunkHeader* c = shared_->chunkHead; c != 0; c = c->next)
      size += c->size;
    return size;
  }

  //! Whether the allocator is shared.
  /*! \return true or false.
   */
  bool Shared() const noexcept {
    sonic_assert(shared_->refcount > 0);
    return shared_->refcount > 1;
  }

  //! Allocates a memory block. (concept Allocator)
  void* Malloc(size_t size) {
    sonic_assert(shared_->refcount > 0);
    if (!size) return NULL;

    size = SONIC_ALIGN(size);
    LOCK_GUARD;
    if (sonic_unlikely(shared_->chunkHead->size + size >
                       shared_->chunkHead->capacity))
      if (!AddChunk(chunk_capacity_ > size ? chunk_capacity_ : size))
        return NULL;

    void* buffer = GetChunkBuffer(shared_) + shared_->chunkHead->size;
    shared_->chunkHead->size += size;
    return buffer;
  }

  //! Resizes a memory block (concept Allocator)
  void* Realloc(void* originalPtr, size_t originalSize, size_t newSize) {
    if (originalPtr == 0) return Malloc(newSize);

    sonic_assert(shared_->refcount > 0);
    if (newSize == 0) return nullptr;

    originalSize = SONIC_ALIGN(originalSize);
    newSize = SONIC_ALIGN(newSize);

    // Do not shrink if new size is smaller than original
    if (originalSize >= newSize) return originalPtr;

    // Simply expand it if it is the last allocation and there is sufficient
    // space
    {
      LOCK_GUARD;
      if (originalPtr ==
          GetChunkBuffer(shared_) + shared_->chunkHead->size - originalSize) {
        size_t increment = static_cast<size_t>(newSize - originalSize);
        if (shared_->chunkHead->size + increment <=
            shared_->chunkHead->capacity) {
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
    return nullptr;
  }

  //! Frees a memory block (concept Allocator)
  static void Free(void* ptr) noexcept { (void)ptr; }  // Do nothing

  // ! Compare (equality) with another MemoryPoolAllocator
  bool operator==(const MemoryPoolAllocator& rhs) const noexcept {
    sonic_assert(shared_->refcount > 0);
    sonic_assert(rhs.shared_->refcount > 0);
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
    if (!baseAllocator_) {
      shared_->ownBaseAllocator = baseAllocator_ = new BaseAllocator();
    }
    if (ChunkHeader* chunk = static_cast<ChunkHeader*>(
            baseAllocator_->Malloc(SIZEOF_CHUNK_HEADER + capacity))) {
      chunk->capacity = capacity;
      chunk->size = 0;
      chunk->next = shared_->chunkHead;
      shared_->chunkHead = chunk;
      return true;
    }
    return false;
  }

  static inline void* AlignBuffer(void* buf, size_t& size) {
    sonic_assert(buf != 0);
    const uintptr_t mask = sizeof(void*) - 1;
    const uintptr_t ubuf = reinterpret_cast<uintptr_t>(buf);
    if (sonic_unlikely(ubuf & mask)) {
      const uintptr_t abuf = (ubuf + mask) & ~mask;
      sonic_assert(size >= abuf - ubuf);
      buf = reinterpret_cast<void*>(abuf);
      size -= abuf - ubuf;
    }
    return buf;
  }

  size_t chunk_capacity_;  //!< The minimum capacity of chunk when they are
                           //!< allocated.
  BaseAllocator*
      baseAllocator_;   //!< base allocator for allocating memory chunks.
  SharedData* shared_;  //!< The shared data of the allocator
  SpinLock lock_;
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
    return (T*)alloc_->Malloc(n * sizeof(T));
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
