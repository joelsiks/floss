#ifndef INCLUDE_GIC
#define INCLUDE_GIC

#include <cstdint>

namespace GIC {
  namespace v3 {
    void initialize_gic_distributor();
    void initialize_gic_redistributor();

    void enable_cpu_interface();
    void enable_cpu_interrupts();
    void set_cpu_priority_mask(uint64_t priority);

    void set_interrupt_priority(int id, uint8_t priority);
    void set_interrupt_group(int id);
    void enable_interrupt(int id);
  };

  void initialize();
};

#endif // INCLUDE_GIC
