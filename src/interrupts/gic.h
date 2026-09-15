#ifndef INCLUDE_GIC
#define INCLUDE_GIC

#include <cstdint>

#include "DeviceTree.h"

namespace GIC {
  enum class InterruptPriority : uint8_t {
    Default = 120,
    High = 40,
  };

  enum class InterruptType {
    Secure,
    NonSecure,
    Virtual,
    Hypervisor,
  };

  enum class DriverVersion {
    v2,
    v3
  };

  // Virtual base class that the versions of the GIC implements
  class GICDriver {
  public:
    virtual void initialize() = 0;
    virtual void initialize_core_specific() = 0;
    virtual void initialize_interrupt(int id, InterruptPriority p) = 0;

    // Interface for exceptions.S to ack/EOI an IRQ.
    // GICv3 reads ICC_IAR_EL1 / writes ICC_EOIR_EL1 (sysregs);
    // GICv2 reads GICC_IAR / writes GICC_EOIR (MMIO).
    virtual uint32_t acknowledge_interrupt() = 0;
    virtual void end_of_interrupt(uint32_t id) = 0;
    virtual DriverVersion version() = 0;
  };

  void dt_parse_v2(const DeviceTree::NodeFrame* node_frame);
  void dt_parse_v3(const DeviceTree::NodeFrame* node_frame);

  GICDriver* driver();

  // These APIs need a stable ABI to be called from assembly code
  extern "C" uint32_t acknowledge_interrupt();
  extern "C" void end_of_interrupt(uint32_t id);
};

#endif // INCLUDE_GIC
