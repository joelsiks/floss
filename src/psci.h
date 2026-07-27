#ifndef INCLUDE_PSCI
#define INCLUDE_PSCI

#include <cstdint>

// From the Power State Coordination Interface Manual:
//   Operating systems typically perform much of the kernel boot process on one primary core, bringing
//   secondary cores online at a later stage. For systems that support hotplug, the operations involved in
//   booting a core for secondary boot or hotplug, are the same. Therefore, they can be provided as a single
//   interface.
//
//   ...
//
//   1) The supervisory software can request that a core be powered up. The supervisory software
//   must provide an appropriate start address for the Non-secure Exception Level where it will
//   resume operation when it exits the privileged platform firmware. The provision of a start address
//   means the caller can shortcut any bootloader-related code when onlining a core, by providing an
//   entry point directly in its own OS address space. Different addresses can be provided to handle
//   different startup reasons. Alternatively, the supervisory software can use internal per-core data
//   structures for this purpose.

struct PSCIInfo {
  uint32_t _major;
  uint32_t _minor;
};

namespace PSCI {
  // The PSCI functions/API accessed via Secure Monitor Call (SMC) (or Hypervisor Call (HVC) in some cases)
  // has its own calling convention (SMC64/SMC32).

  PSCIInfo information();

  int32_t boot_core(uint64_t target_cpucore_id, uint64_t entry_point_address, uint64_t context_id);
};

#endif // INCLUDE_PSCI
