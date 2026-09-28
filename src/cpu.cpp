
#include "cpu.h"

#include <cstring>

#include "boot/psci.h"
#include "boot/SpinTable.h"
#include "DeviceTree.h"
#include "util/assert.h"
#include "kstdio.h"

enum class EnableMethod {
  PSCI,
  SpinTable,
  Unknown,
};

struct CpuInformation {
  // Unique CPU/thread id for the CPU/threads represented by the CPU DT node
  uint64_t _reg;

  // How to enable/boot/start the CPU
  EnableMethod _enable_method{EnableMethod::Unknown};

  // CPU nodes in the DT that have the enable-method property, and have it set
  // to "spin-table" also have a property called "cpu-release-addr", which
  // contains the address which the CPU spins on in order to wake it up.
  // E.g.: cpu-release-addr = <0x00 0xd8>;
  uint64_t _release_address;
};

static const uint32_t MaxSupportedCpus = 16;
static CpuInformation _cpu_information[MaxSupportedCpus];
static uint32_t _num_cpus = 0;

void CPU::dt_parse(const DeviceTree::NodeFrame* node_frame) {
  kprecond(_num_cpus < MaxSupportedCpus);
  CpuInformation* info = &_cpu_information[_num_cpus];

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

      info->_enable_method = parsed_enable_method;
    } else if (strcmp(prop->_name, "cpu-release-addr") == 0) {
      const uint64_t release_address = DeviceTree::Parser::read_u64(prop->_value);
      info->_release_address = release_address;
    } else if(strcmp(prop->_name, "reg") == 0)  {
      const uint32_t rp_size_bytes = node_frame->_parent_cells.byte_size();
      kassert(prop->_len % rp_size_bytes == 0, "Invalid reg length (%d, rp size %d)\n", prop->_len, rp_size_bytes);

      const uint32_t num_reg_pairs = prop->_len / rp_size_bytes;
      kassert(num_reg_pairs == 1, "Zero or more than one reg value for CPUs is not supported\n");

      DeviceTree::RegPair rp;
      DeviceTree::read_reg_pair(&node_frame->_parent_cells, prop->_value, &rp);

      info->_reg = rp._address;
    }
  }

  _num_cpus++;
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

uint32_t CPU::num_cpus() {
  return _num_cpus;
}

// Defined in start.S
extern "C" void* _secondary_start;

void CPU::boot_secondary_cores() {
  // Loop over all the secondary cores and boot them up, which are all cores
  // except the first one (id 0).
  for (uint32_t i = 1; i < _num_cpus; i++) {
    const CpuInformation* info = &_cpu_information[i];

    switch (info->_enable_method) {
      case EnableMethod::PSCI: {
          const int32_t result = PSCI::boot_core(info->_reg, (uint64_t)&_secondary_start, i);
          if (result < 0) {
            kprintf("Error in PSCI:boot_core(%z, %p): %d\n", info->_reg, (uint64_t)&_secondary_start, result);
          }
        }
        break;
      case EnableMethod::SpinTable:
        SpinTable::boot_core(info->_release_address, (uint64_t)&_secondary_start);
        break;
      case EnableMethod::Unknown:
        kpanic("CPU (%z) with unknown enable method\n", info->_reg);
        break;
    }
  }
}
