#ifndef INCLUDE_MEMORY_VIRTUALALLOCATOR
#define INCLUDE_MEMORY_VIRTUALALLOCATOR

#include <cstdint>

namespace Memory {

  class VirtualAllocator {
  private:
    uint8_t* _start;
    uint8_t* _end;

  public:
    VirtualAllocator(uint8_t* start, uint8_t* end);
    VirtualAllocator(uint8_t* end);
    ~VirtualAllocator();

    uint8_t* allocate(uint64_t pages);
    void free(uint8_t* start, uint64_t pages);
  };

};

#endif // INCLUDE_MEMORY_VIRTUALALLOCATOR
