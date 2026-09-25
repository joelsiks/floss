
#include "interrupts/gic.h"

#include <cstring>

#include "DeviceTree.h"
#include "interrupts/gicv2.h"
#include "interrupts/gicv3.h"
#include "util/assert.h"

static GIC::DriverV3 _driver_v3;
static GIC::DriverV2 _driver_v2;

static GIC::GICDriver* _driver = nullptr;

struct GICRegs {
  static const uint32_t NumRegPairs = 2;
  DeviceTree::RegPair _pairs[NumRegPairs];
};

static GICRegs parse_dt(const DeviceTree::NodeFrame* node_frame) {
  GICRegs regs;

  for (uint32_t i = 0; i < node_frame->_nprops; i++) {
    const DeviceTree::PropFrame* prop = &node_frame->_props[i];

    if (strcmp(prop->_name, "reg") == 0) {
      const void* current_value = prop->_value;
      const uint32_t rp_size_bytes = node_frame->_parent_cells.byte_size();
      const uint32_t num_reg_pairs = prop->_len / rp_size_bytes;

      kprecond(num_reg_pairs >= GICRegs::NumRegPairs);
      kassert(prop->_len % rp_size_bytes == 0, "Invalid reg length (%d, rp size %d)\n", prop->_len, rp_size_bytes);

      // Right now we only care about the first two
      for (uint32_t j = 0; j < GICRegs::NumRegPairs; j++) {
        DeviceTree::read_reg_pair(&node_frame->_parent_cells, current_value, &regs._pairs[j]);
        current_value = (const char*)current_value + rp_size_bytes;
      }
    }
  }

  return regs;
}

void GIC::dt_parse_v2(const DeviceTree::NodeFrame* node_frame) {
  kprecond(_driver == nullptr);
  _driver = &_driver_v2;

  GICRegs regs = parse_dt(node_frame);
  const uintptr_t phys_gicd_base = DeviceTree::translate_address(node_frame, regs._pairs[0]._address);
  const uintptr_t phys_gicc_base = DeviceTree::translate_address(node_frame, regs._pairs[1]._address);
  _driver_v2.set_gicd_base(phys_gicc_base);
  _driver_v2.set_gicc_base(phys_gicd_base);
}

void GIC::dt_parse_v3(const DeviceTree::NodeFrame* node_frame) {
  kprecond(_driver == nullptr);
  _driver = &_driver_v3;

  GICRegs regs = parse_dt(node_frame);
  const uintptr_t phys_gicd_base = DeviceTree::translate_address(node_frame, regs._pairs[0]._address);
  const uintptr_t phys_gicr_base = DeviceTree::translate_address(node_frame, regs._pairs[1]._address);
  _driver_v3.set_gicd_base(phys_gicd_base);
  _driver_v3.set_gicr_base(phys_gicr_base);
}

GIC::GICDriver* GIC::driver() {
  kassert(_driver != nullptr, "GIC Driver has not been initialized. Bad device tree compatible string?");
  return _driver;
}

uint32_t GIC::acknowledge_interrupt() {
  return _driver->acknowledge_interrupt();
}

void GIC::end_of_interrupt(uint32_t id) {
  _driver->end_of_interrupt(id);
}
