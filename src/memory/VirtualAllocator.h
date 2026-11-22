#ifndef INCLUDE_MEMORY_VIRTUALALLOCATOR
#define INCLUDE_MEMORY_VIRTUALALLOCATOR

#include "locking/SpinLock.h"
#include <cstdint>

namespace Memory {

  class VirtualAllocator {
  private:
    uint64_t _start;
    uint64_t _current;
    uint64_t _size;
    SpinLock _lock;

  public:
    VirtualAllocator(uint64_t start, uint64_t size);
    VirtualAllocator(uint64_t size);

    void* allocate(uint64_t pages);
    void free(void* start, uint64_t pages);
  };

  void* kernel_virtual_alloc(uint64_t pages);
  void kernel_virtual_free(void* start, uint64_t pages);
};

#endif // INCLUDE_MEMORY_VIRTUALALLOCATOR
