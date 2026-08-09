
#include "memory/map.h"

#include <cstring>

#include "util/assert.h"

extern "C" char _start[];       // start of kernel image
extern "C" char __kernel_end[]; // end of kernel image

// RAM start and size
static uint64_t RAM_START = 0;
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

      RAM_START = rp._address;
      RAM_SIZE = rp._length;
    }
  }
}

static uint32_t _num_reserved_regions = 0;
static Memory::Map::Region _reserved_regions[Memory::Map::MaxReservedRegions];

void Memory::Map::reserve_region(uint64_t start, uint64_t size) {
  kprecond(_num_reserved_regions < MaxReservedRegions - 1);
  _reserved_regions[_num_reserved_regions] = {start, size};
  _num_reserved_regions++;
}

Memory::Map::Region* Memory::Map::get_reserved_regions() {
  return _reserved_regions;
}

uint32_t Memory::Map::get_num_reserved_regions() {
  return _num_reserved_regions;
}

void Memory::Map::init(DeviceTree::FlattenedDeviceTree* fdt) {
  // Reserve the bootloader "hole"
  reserve_region(RAM_START, (uint64_t)_start - RAM_START);

  // Check the DeviceTree for any reserved memory regions
  DeviceTree::Parser parser;
  if (parser.init(fdt) != DeviceTree::Status::Ok) {
    kpanic("Failed to initialize parser for Memory Map init\n");
  }

  uint64_t address, size;
  for (uint32_t i = 0;; i++) {
    if (!parser.reserve_entry(i, &address, &size)) {
      break;
    }

    reserve_region(address, address + size);
  }

  // Reserve the kernel
  reserve_region((uint64_t)_start, (uint64_t)__kernel_end - (uint64_t)_start);

  // Reserve the Flattened Device Tree
  reserve_region((uint64_t)fdt, parser.totalsize());
}
