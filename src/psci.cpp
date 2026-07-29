
#include <cstdint>

#include "psci.h"

static const uint64_t PSCI_PSCI_VERSION = 0x84000000;
static const uint64_t PSCI_PSCI_CPU_ON = 0xC4000003;

static uint32_t psci_get_version() {
  // Implements the "VERSION" function identifier.

  // This method needs to be "hardened" to read the Device Tree to see if PSCI
  // is implemented in EL2 or EL3, and thus if we should use "hvc" or "smc" to
  // call into the PSCI API.
  register uint32_t __result __asm__("w0");
  asm volatile(
   "mov   x0, %1 \n\t\
    hvc   #0     \n\t\
    "
    : "=r"(__result) : "i"(PSCI_PSCI_VERSION) : "memory"
  );

  return __result;
}

PSCIInfo PSCI::information() {
  const uint32_t version = psci_get_version();

  // The Major version is in bits [31:16]
  const uint32_t major = static_cast<uint32_t>(version >> 16);
  // The Minor version is in bits [15:0]
  const uint32_t minor = static_cast<uint32_t>(version & 0xFFFF);

  return PSCIInfo{major, minor};
}

int32_t PSCI::boot_core(uint64_t target_cpu, uint64_t entry_point_address, uint64_t context_id) {
  // Implements the "CPU_ON" function identifier.

  // Power up a core. This call is used to power up cores that either:
  //   Have not yet been booted into the calling supervisory software.
  //   Have been previously powered down with a CPU_OFF call.
  //
  // We'll mainly use this for the first reason, to power up cores that have
  // not yet been booted.

  // Three uint64_t arguments if we're using the SMC64 CC. Arguments are passed
  // in x1-x17, with the "Function Identifier" in x0.

  // Function identifier
  register uint64_t r0 __asm__("x0") = PSCI_PSCI_CPU_ON;
  // First argument is the target_cpu (copy of the MPIDR register)
  register uint64_t r1 __asm__("x1") = target_cpu;
  // Second argument is the entry_point_address (which we know statically)
  register uint64_t r2 __asm__("x2") = entry_point_address;
  // Third argument: When the core identified by target_cpu first
  register uint64_t r3 __asm__("x3") = context_id;

  asm volatile(
   "hvc #0"
    : "+r"(r0) // r0 is both input (function identifier) and output (status)
    : "r"(r1), "r"(r2), "r"(r3)
    : "memory"
  );

  return (int32_t)r0;
}
