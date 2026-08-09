
#include "memory/map.h"

#include <cstring>

#include "util/assert.h"

// RAM start and size
static void* RAM_START = nullptr;
static uint64_t RAM_SIZE = 0;

void Memory::Map::dt_parse(const DeviceTree::NodeFrame* node_frame) {
  // Iterate over all the props
  for (uint32_t i = 0; i < node_frame->_nprops; i++) {
    const DeviceTree::PropFrame* prop = &node_frame->_props[i];

    if (strcmp(prop->_name, "reg") == 0) {
      const uint32_t rp_size_bytes = node_frame->_parent_cells.byte_size();
      const uint32_t num_reg_pairs = prop->_len / rp_size_bytes;

      kassert(prop->_len % rp_size_bytes == 0, "Invalid reg length (%d, rp size %d)\n", prop->_len, rp_size_bytes);
      kassert(num_reg_pairs == 1, "Only expecting one reg pair for the memory node\n");

      DeviceTree::RegPair rp;
      DeviceTree::read_reg_pair(&node_frame->_parent_cells, prop->_value, &rp);

      RAM_START = reinterpret_cast<void*>(rp._address);
      RAM_SIZE = rp._length;
    }
  }
}

