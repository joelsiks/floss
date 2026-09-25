
#include "SpinTable.h"

void SpinTable::boot_core(uint64_t spin_address, uint64_t entry_point_address) {
  // Boots up another core that is spin-loading the value at spin_address until it
  // becomes non-zero. It is "waiting" in a low powered state with the "wfe" instruction.
  // When writing the address that we want to boot the core in at spin_address,
  // we make sure that memory write is observed by all other cores with a dsb,
  // then "wake" the core(s) up with a "sev" instruction so that they are prompted
  // to check their exit condition.
  asm volatile(
   "str %1, [%0] \n\t\
    dsb sy       \n\t\
    sev          \n\t\
    " :: "r"(spin_address), "r"(entry_point_address) : "memory");
}
