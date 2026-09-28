
#include "psci.h"

#include <cstdint>
#include <cstring>

#include "kstdio.h"
#include "util/assert.h"

// The method used to call into the PSCI API.
// Depends on which level PSCI is implemented in (EL2/EL3).
enum class PSCIMethod {
  Hypervisor, // hvc
  SecureMonitor, // smc
  Unknown
};

// The default values for these are mandated by the spec.
//   0x84000000-0x8400FFFF range is "SMC32: Standard Service Calls"
//   0xC4000000-0xC400FFFF range is "SMC64: Standard Service Calls"
static uint64_t PSCI_CPU_ON = 0xC4000003;
static const uint64_t PSCI_CPU_OFF = 0x84000002; // 32-bit only
static const uint64_t PSCI_CPU_SUSPEND = 0xC4000001;

static PSCIMethod PSCI_METHOD = PSCIMethod::SecureMonitor; // Sane default

static PSCIMethod psci_method_from_str(const char* method_str) {
  if (strcmp(method_str, "hvc") == 0) {
    return PSCIMethod::Hypervisor;
  } else if (strcmp(method_str, "smc") == 0) {
    return PSCIMethod::SecureMonitor;
  }

  kpanic("Unknown PSCI method '%s'\n", method_str);
  return PSCIMethod::Unknown;
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

int32_t PSCI::boot_core(uint64_t target_cpu, uint64_t entry_point_address, uint64_t context_id) {
  kprecond(PSCI_CPU_ON != 0);

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
    case PSCIMethod::SecureMonitor:
      asm volatile(
       "smc #0"
        : "+r"(r0) // r0 is both input (function identifier) and output (status)
        : "r"(r1), "r"(r2), "r"(r3)
        : "memory"
      );
      break;
    default:
      // Do nothing for now
      break;
  }

  return (int32_t)r0;
}
