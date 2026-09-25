
#include "cpu.h"

#include <cstring>

#include "boot/psci.h"
#include "boot/SpinTable.h"
#include "DeviceTree.h"
#include "util/assert.h"

enum class EnableMethod {
  PSCI,
  SpinTable,
  Unknown,
};

// CPU nodes in the DeviceTree that has the enable-method property set to
// "spin-table" also have a property called "cpu-release-addr", which contains
// the address which the CPU spins on in order to wake it up.
// E.g.: cpu-release-addr = <0x00 0xd8>;
struct ReleaseAddress {
  uint64_t _address;
};

static const uint32_t MaxSupportedCPUs = 16;

static ReleaseAddress _release_addresses[MaxSupportedCPUs];
static EnableMethod _global_enable_method = EnableMethod::Unknown;
static uint32_t _num_cpu_cores = 0;

void CPU::dt_parse(const DeviceTree::NodeFrame* node_frame) {
  for (uint32_t i = 0; i < node_frame->_nprops; i++) {
    const DeviceTree::PropFrame* prop = &node_frame->_props[i];

    if (strcmp(prop->_name, "enable-method") == 0) {
      const char* enable_method_str = static_cast<const char*>(prop->_value);

      EnableMethod parsed_enable_method = EnableMethod::Unknown;

      if (strcmp(enable_method_str, "psci") == 0) {
        parsed_enable_method = EnableMethod::PSCI;
      } else if (strcmp(enable_method_str, "spin-table") == 0) {
        parsed_enable_method = EnableMethod::SpinTable;
      } else {
        kpanic("No enable method set. Got: %s\n", enable_method_str);
      }

      if (_global_enable_method == EnableMethod::Unknown) {
        _global_enable_method = parsed_enable_method;
      } else {
        kassert(_global_enable_method == parsed_enable_method,
                "Inconsistent CPU enable-methods are not supported\n");
      }
    } else if (strcmp(prop->_name, "cpu-release-addr") == 0) {
      kprecond(_global_enable_method != EnableMethod::PSCI);
      const uint64_t release_address = DeviceTree::Parser::read_u64(prop->_value);
      _release_addresses[_num_cpu_cores] = { release_address };
    }
  }

  _num_cpu_cores++;
  if (_num_cpu_cores == MaxSupportedCPUs) {
    kpanic("floss currently supports a maximum of %d CPUs\n", MaxSupportedCPUs);
  }
}

uint64_t CPU::id() {
  uint64_t mpidr;
  asm volatile(
   "mrs   %0, MPIDR_EL1   \n\t\
    and   %0, %0, #0xff   \n\t\
    "
    : "=r"(mpidr) :: "memory"
  );

  return mpidr;
}

uint32_t CPU::num_cores() {
  return _num_cpu_cores;
}

// Defined in start.S
extern "C" void* _secondary_start;

void CPU::boot_secondary_cores() {
  kprecond(_global_enable_method != EnableMethod::Unknown);

  // Loop over all cores except the first one (id 0). Those are considered the secondary cores.
  if (_global_enable_method == EnableMethod::PSCI) {
    for (uint32_t i = 1; i < _num_cpu_cores; i++) {
      PSCI::boot_core(i, (uint64_t)&_secondary_start, i);
    }
  } else if (_global_enable_method == EnableMethod::SpinTable) {
    for (uint32_t i = 1; i < _num_cpu_cores; i++) {;
      SpinTable::boot_core(_release_addresses[i]._address, (uint64_t)&_secondary_start);
    }
  }
}
