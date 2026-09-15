#ifndef INCLUDE_GICV2
#define INCLUDE_GICV2

#include <cstdint>

#include "interrupts/gic.h"

namespace GIC {
  class DriverV2 final : public GICDriver {
  private:
    void initialize_gic_distributor();

    void enable_cpu_interface();
    void enable_cpu_interrupts();
    void set_cpu_priority_mask(uint64_t priority);

    void set_interrupt_priority(int id, InterruptPriority priority);
    void set_interrupt_group(int id);
    void enable_interrupt(int id);
    void disable_interrupt(int id);

    void set_interrupt_routing(int id);

  public:
    void set_gicd_base(uintptr_t base);
    void set_gicc_base(uintptr_t base);

    void initialize() override;
    void initialize_core_specific() override;

    void initialize_interrupt(int id, InterruptPriority priority = InterruptPriority::Default) override;

    uint32_t acknowledge_interrupt() override;
    void end_of_interrupt(uint32_t id) override;

    inline DriverVersion version() override {
      return DriverVersion::v2;
    }
  };
};

#endif // INCLUDE_GICV2
