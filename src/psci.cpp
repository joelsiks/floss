
#include <cstdint>
#include <cstring>

#include "psci.h"
#include "util/assert.h"

static uint64_t PSCI_CPU_ON{0};
static PSCIMethod PSCI_METHOD;

void PSCI::initialize_cpu_on(uint64_t cpu_on) {
  PSCI_CPU_ON = cpu_on;
}

static PSCIMethod psci_method_from_str(const char* method_str) {
  if (strcmp(method_str, "hvc")) {
    return PSCIMethod::Hypervisor;
  } else if (strcmp(method_str, "smc")) {
    return PSCIMethod::Supervisor;
  }

  // TODO: Panic
  kpanic("Unknown PSCI method '%s'\n", method_str);
  return PSCIMethod::Supervisor;
}

void PSCI::initialize_method(const char* method_str) {
  PSCI_METHOD = psci_method_from_str(method_str);
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
  register uint64_t r0 __asm__("x0") = PSCI_CPU_ON;
  // First argument is the target_cpu (copy of the MPIDR register)
  register uint64_t r1 __asm__("x1") = target_cpu;
  // Second argument is the entry_point_address (which we know statically)
  register uint64_t r2 __asm__("x2") = entry_point_address;
  // Third argument: When the core identified by target_cpu first
  register uint64_t r3 __asm__("x3") = context_id;

  switch (PSCI_METHOD) {
    case PSCIMethod::Hypervisor:
      asm volatile(
       "hvc #0"
        : "+r"(r0) // r0 is both input (function identifier) and output (status)
        : "r"(r1), "r"(r2), "r"(r3)
        : "memory"
      );
      break;
    case PSCIMethod::Supervisor:
      asm volatile(
       "smc #0"
        : "+r"(r0) // r0 is both input (function identifier) and output (status)
        : "r"(r1), "r"(r2), "r"(r3)
        : "memory"
      );
      break;
  }

  return (int32_t)r0;
}
