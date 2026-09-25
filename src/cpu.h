#ifndef INCLUDE_CPU
#define INCLUDE_CPU

#include "DeviceTree.h"

namespace CPU {
  void dt_parse(const DeviceTree::NodeFrame* node_frame);

  // Retrieves the cpu/core/PE id of the current core
  uint64_t id();

  uint32_t num_cores();

  void boot_secondary_cores();
};

#endif // INCLUDE_CPU
