#ifndef INCLUDE_TIMER
#define INCLUDE_TIMER

#include "DeviceTree.h"
#include "interrupts/gic.h"

namespace Timer {
  void dt_parse(const DeviceTree::NodeFrame* node_frame);
  uint32_t intid(GIC::InterruptType interrupt_type);

  void enable();
  void disable();
  void set_timer();
};

#endif // INCLUDE_TIMER
