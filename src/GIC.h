#ifndef INCLUDE_GIC
#define INCLUDE_GIC

#include <cstdint>

namespace GIC {
  void init_gic_distributor();
  void init_gic_redistributor();

  void set_interrupt_priority(int id, int priority);
  void set_interrupt_group(int id);
  void enable_interrupt(int id);

  void set_priority_mask(uint32_t priority);
};

#endif // INCLUDE_GIC
