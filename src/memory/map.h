#ifndef INCLUDE_MEMORY_MAP
#define INCLUDE_MEMORY_MAP

#include "DeviceTree.h"

namespace Memory::Map {
  void dt_parse(const DeviceTree::NodeFrame* node_frame);
};

#endif // INCLUDE_MEMORY_MAP
