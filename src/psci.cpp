
#include "psci.h"

#include <cstdint>
#include <cstring>

#include "cpu.h"
#include "util/assert.h"

// The method used to call into the PSCI API.
// Depends on which level PSCI is implemented in (EL2/EL3).
enum class PSCIMethod {
  Hypervisor, // hvc
  Supervisor  // smc
};

static uint64_t PSCI_CPU_ON = 0;
static PSCIMethod PSCI_METHOD = PSCIMethod::Supervisor; // Sane default

static PSCIMethod psci_method_from_str(const char* method_str) {
  if (strcmp(method_str, "hvc") == 0) {
    return PSCIMethod::Hypervisor;
  } else if (strcmp(method_str, "smc") == 0) {
    return PSCIMethod::Supervisor;
  }

  kpanic("Unknown PSCI method '%s'\n", method_str);
  return PSCIMethod::Supervisor;
}

void PSCI::dt_parse(const DeviceTree::NodeFrame* node_frame) {
  for (uint32_t i = 0; i < node_frame->_nprops; i++) {
    const DeviceTree::PropFrame* prop = &node_frame->_props[i];
    if (strcmp(prop->_name, "cpu_on") == 0) {
      PSCI_CPU_ON = DeviceTree::Parser::read_prop(prop);
    } else if (strcmp(prop->_name, "method") == 0) {
      PSCI_METHOD = psci_method_from_str(reinterpret_cast<const char*>(prop->_value));
    }
  }
}

// Defined in start.S
extern "C" void* _secondary_start;

void PSCI::boot_secondary_cores() {
  const uint32_t num_cpus = CPU::num_cpu_cores();

  // Loop over all secondary cores, which are all cores except the first one (id 0)
  for (uint32_t i = 1; i < num_cpus; i++) {
    PSCI::boot_core(i, (uint64_t)&_secondary_start, i);
  }
}

int32_t PSCI::boot_core(uint64_t target_cpu, uint64_t entry_point_address, uint64_t context_id) {
  // Implements the "CPU_ON" function identifier.

  // Power up a core. This call is used to power up cores that either:
  //   Have not yet been booted into the calling supervisory software.
  //   Have been previously powered down with a CPU_OFF call.
  //
  // We'll mainly use this for the first reason, to power up cores that have not
  // yet been booted.

  // Three uint64_t arguments if we're using the SMC64 CC. Arguments are passed
  // in x1-x17, with the function identifier in x0.

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
