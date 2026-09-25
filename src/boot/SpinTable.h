#ifndef INCLUDE_BOOT_SPINTABLE
#define INCLUDE_BOOT_SPINTABLE

#include <cstdint>

namespace SpinTable {
  void boot_core(uint64_t spin_address, uint64_t entry_point_address);
};

#endif // INCLUDE_BOOT_SPINTABLE
