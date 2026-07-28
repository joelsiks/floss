#ifndef INCLUDE_GIC
#define INCLUDE_GIC

#include <cstdint>

namespace GIC {
  namespace v3 {
    void initialize_gic_distributor();
    void initialize_gic_redistributors();

    void enable_cpu_interface();
    void enable_cpu_interrupts();
    void set_cpu_priority_mask(uint64_t priority);

    void set_interrupt_priority(int id, uint8_t priority);
    void set_interrupt_group(int id);
    void enable_interrupt(int id);
    void disable_interrupt(int id);

    void set_interrupt_routing(int id, bool any);
  };

  void initialize();
  void initialize_core_specific();
};

#endif // INCLUDE_GIC
