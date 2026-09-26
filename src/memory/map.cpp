
#include "memory/map.h"

#include <cstring>

#include "util/assert.h"

extern "C" char _start[];       // start of kernel image
extern "C" char __kernel_end[]; // end of kernel image

static Memory::RegionList _reserved_regions;
static Memory::RegionList _ram_regions;
static Memory::RegionList _device_regions;

void Memory::dt_parse(const DeviceTree::NodeFrame* node_frame) {
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

      _ram_regions.add_region(rp._address, rp._length);
    }
  }
}

void Memory::RegionList::add_region(uint64_t start, uint64_t size) {
  kprecond(_num_regions < MaxNumRegions - 1);

  _regions[_num_regions] = {start, size};
  _num_regions++;
}

void Memory::record_device_region(uint64_t start, uint64_t size) {
  _device_regions.add_region(start, size);
}

Memory::RegionList* Memory::reserved_regions() {
  return &_reserved_regions;
}

Memory::RegionList* Memory::ram_regions() {
  return &_ram_regions;
}

Memory::RegionList* Memory::device_regions() {
  return &_device_regions;
}

void Memory::init(DeviceTree::FlattenedDeviceTree* fdt) {
  // Reserve the bootloader "hole"
  const uint64_t kernel_start = (uint64_t)_start;
  for (uint64_t i = 0; i < _ram_regions._num_regions; i++) {
    Region* ram_region = &_ram_regions._regions[i];
    if (kernel_start > ram_region->_start && kernel_start < (ram_region->_start + ram_region->_size)) {
      _reserved_regions.add_region(ram_region->_start, (uint64_t)_start - ram_region->_start);
      break;
    }
  }

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

    _reserved_regions.add_region(address, size);
  }

  // Reserve the kernel
  _reserved_regions.add_region((uint64_t)_start, (uint64_t)__kernel_end - (uint64_t)_start);

  // Reserve the Flattened Device Tree
  _reserved_regions.add_region((uint64_t)fdt, parser.totalsize());
}
