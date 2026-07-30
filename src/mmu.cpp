
#include "mmu.h"

void MMU::setup_translation_control() {

  // TCR_EL1 is the Translation Control Register. It configures stuff like:
  //  - How many bits are used for virtual addressing
  //  - The granule size/page size
  //  - Shareability
  //  - Whether TLB miss results in a translation fault or actual lookup
  //
  // EL1&0 translation regime (organized way of doing something)
  // TTBR_EL1/TTBR_EL0 (Translation Table Base Register)
}
