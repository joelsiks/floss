
#include "mmu.h"

#include <cstdint>

void MMU::setup_translation_control() {

  // TCR_EL1 is the Translation Control Register. It configures stuff like:
  //  - How many bits are used for virtual addressing
  //  - The granule size/page size
  //  - Shareability
  //  - Whether TLB miss results in a translation fault or actual lookup
  //
  // EL1 & El00 translation regime (organized way of doing something)
  // TTBR_EL1/TTBR_EL0 (Translation Table Base Register)

  uint64_t tcr_el1 = 0;

  // TCR_EL1.IPS  (Intermediate Physical Address Size)
  // Determines the address space size. 48 bits is common, gives 256TB addressble memory, value is 0b101
}
