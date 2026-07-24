
#include <cstdint>

#include "psci.h"

static const uint64_t PSCI_PSCI_VERSION = 0x84000000;
static const uint64_t PSCI_PSCI_CPU_ON = 0x84000003;

static uint32_t psci_get_version() {
  // The "smc" instruction is only available to software executing at EL1 or higher
  register uint64_t __result __asm__("x0");
  asm volatile(
   "mov   x0, %1   \n\t\
    smc   #0       \n\t\
    "
    : "=r"(__result) : "i"(PSCI_PSCI_VERSION) : "memory"
  );

  return __result;
}

uint32_t PSCI::version_major() {
  return psci_get_version();
}

uint32_t PSCI::version_minor() {
  return psci_get_version();
}
