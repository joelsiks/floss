#ifndef INCLUDE_MEMORY_MAP
#define INCLUDE_MEMORY_MAP

#include "DeviceTree.h"

namespace Memory::Map {
  void dt_parse(const DeviceTree::NodeFrame* node_frame);

  struct Region {
    uint64_t _start;
    uint64_t _size;
  };

  const uint32_t MaxReservedRegions = 16;

  void reserve_region(uint64_t start, uint64_t size);
  Region* get_reserved_regions();
  uint32_t get_num_reserved_regions();

  void init(DeviceTree::FlattenedDeviceTree* fdt);
};

#endif // INCLUDE_MEMORY_MAP
