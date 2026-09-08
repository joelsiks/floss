#ifndef INCLUDE_CPU
#define INCLUDE_CPU

#include "DeviceTree.h"

namespace CPU {
  void dt_parse(const DeviceTree::NodeFrame* node_frame);

  uint32_t num_cpu_cores();
};

#endif // INCLUDE_CPU
