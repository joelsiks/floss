#ifndef INCLUDE_MEMORY_VIRTUALALLOCATOR
#define INCLUDE_MEMORY_VIRTUALALLOCATOR

#include "locking/SpinLock.h"
#include <cstdint>

namespace Memory {

  class VirtualAllocator {
  private:
    void* _start;
    uint64_t _size;
    SpinLock _lock;

  public:
    VirtualAllocator(void* start, uint64_t size);
    VirtualAllocator(uint64_t size);

    void* allocate(uint64_t pages);
    void free(void* start, uint64_t pages);
  };

  void* allocate_virtual_global(uint64_t pages);
  void free_virtual_global(void* start, uint64_t pages);
};

#endif // INCLUDE_MEMORY_VIRTUALALLOCATOR
