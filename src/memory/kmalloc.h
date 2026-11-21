#ifndef INCLUDE_MEMORY_KMALLOC
#define INCLUDE_MEMORY_KMALLOC

#include <cstdint>

namespace Memory {
  void* kmalloc(uint64_t size);
  void kfree(void* pointer);
};

#endif // INCLUDE_MEMORY_KMALLOC
