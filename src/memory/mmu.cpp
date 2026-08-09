
#include "mmu.h"

#include <cstdint>

// MMU Setup:
//  1. Configure Memory Attribute Indirection (MAIR)
//     This sets what kinds of operations are allowed on the memory with certain
//     attributes.
//
//     Shareability is configured in the page table descriptor(s)

// We want device memory to be non-Gathering, non-Reordering, and non-Early
// Write Acknowledgement.
//
//  Non-Gathering: we want reads/writes to be independent, not merged
//  Non-Reordering: we want operations to happen in the way we specify
//                  them in program order
//  Non-Early Write Acknowledgement: wait until write ends up in device 
//                                   register, not write buffer
//
//  Normal memory doesn't trigger hardware side effects like accessing
//  device memory, so more optimizations can be applied (less restriction).
//  We want: Write-Back Cacheable, Inner Shareable and Non-Transient
//
//  Write-Back: write to cache, wait for cache-line to be evicted before
//              flushing to main memory
//  Write-Through: don't wait for cache-line eviction and write to RAM
//                 eagerly. Much slower.
//  Cacheable: whether the CPU is allowed to keep a local copy in a cache
//  Shareable: Inner/Outer/None? Inner is usually all caches except LLC,
//             and outer is usually LLC
//  Transient: hint that data won't be used for long and is not worth having
//                 in a cache

// Normal Memory. Non-Gathering, Non-Reordering, Non-Eearly Write Acknowledgement
static const uint64_t MAIR_DEVICE_NGNRNE = 0x00;

// Device Memory. Outer+Inner Write-Back, Outer+Inner Read+Write Allocate, Non-Transient
// The read+write allocate means that a cache line is allocated on a read/write miss
static const uint64_t MAIR_NORMAL_WB = 0xFF;

static const uint64_t MAIR_INDEX_DEVICE = 0;
static const uint64_t MAIR_INDEX_NORMAL_WB = 2;

inline static void configure_mair_range(uint64_t attribute, uint64_t index) {
  // Memory Attribute Indirection Reigster (EL1)

  // Each attribute is 8 bits and there are 8 attributes (Attr0-Attr7)
  const uint64_t attribute_value = attribute << (index * 8);
  asm volatile(
   "mrs   x0, MAIR_EL1 \n\t\
    orr   x0, x0, %0   \n\t\
    msr   MAIR_EL1, x0 \n\t\
    isb                \n\t\
    "
    :: "r"(attribute_value) : "memory"
  );
}

void MMU::setup_mair_ranges() {
  configure_mair_range(MAIR_DEVICE_NGNRNE, MAIR_INDEX_DEVICE);
  configure_mair_range(MAIR_NORMAL_WB, MAIR_INDEX_NORMAL_WB);
}

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
