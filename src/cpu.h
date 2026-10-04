#ifndef INCLUDE_CPU
#define INCLUDE_CPU

#include "DeviceTree.h"

namespace CPU {
  void dt_parse(const DeviceTree::NodeFrame* node_frame);

  // Retrieves the contiguous logical id of the current core by mapping its
  // MPIDR_EL1 affinity to the corresponding device-tree CPU array entry.
  uint64_t id();
  void print_affinity();

  uint32_t num_cpus();

  void boot_secondary_cores();
};

#endif // INCLUDE_CPU
