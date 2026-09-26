#ifndef INCLUDE_MEMORY_MAP
#define INCLUDE_MEMORY_MAP

#include "DeviceTree.h"

namespace Memory {
  void dt_parse(const DeviceTree::NodeFrame* node_frame);

  struct Region {
    uint64_t _start;
    uint64_t _size;
  };

  struct RegionList {
    static const uint32_t MaxNumRegions = 16;

    uint64_t _num_regions{0};
    Region   _regions[MaxNumRegions];

    void add_region(uint64_t start, uint64_t size);
  };

  RegionList* reserved_regions();
  RegionList* ram_regions();
  RegionList* device_regions();

  void record_device_region(uint64_t start, uint64_t size);

  void init(DeviceTree::FlattenedDeviceTree* fdt);
};

#endif // INCLUDE_MEMORY_MAP
