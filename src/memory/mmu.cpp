
#include "mmu.h"

#include <cstdint>

#include "util/assert.h"

extern "C" char _start[];       // start of kernel image
extern "C" char __kernel_end[]; // end of kernel image

// MMU Setup:
//  1. Configure Memory Attribute Indirection (MAIR)
//     Sets what kinds of operations are allowed on memory with certain attributes
//
//     Note: Shareability is configured in the page table descriptor(s)

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

inline static void configure_mair_range(const uint64_t attribute, uint64_t index) {
  // Memory Attribute Indirection Reigster (EL1)

  // Each attribute is 8 bits and there are 8 attributes (Attr0-Attr7)
  const uint64_t attribute_value = attribute << (index * 8);
  uint64_t temp;
  asm volatile(
   "mrs   %0, MAIR_EL1 \n\t\
    orr   %0, %0, %1   \n\t\
    msr   MAIR_EL1, %0 \n\t\
    isb                \n\t\
    "
    : "=&r"(temp) : "r"(attribute_value) : "memory"
  );
  // The & in "=&r" tells the compiler not to reuse this register
}

void MMU::setup_mair_ranges() {
  configure_mair_range(MAIR_DEVICE_NGNRNE, MAIR_INDEX_DEVICE);
  configure_mair_range(MAIR_NORMAL_WB, MAIR_INDEX_NORMAL_WB);
}

void MMU::set_ttbr(uint64_t translation_table, uint32_t exception_level) {
  // Translation Table Base Register (EL1) TTBR0_EL1, TTBR1_EL1
  // 0 is supposed to be user-space (EL0), and 1 is supposed to be kernel-space (EL1),
  // but is gated by the upper 16 bits being either all 0s (TTRB0) or all 1s (TTBR1),
  // not by exception level. Permission is configured differently.

  // I don't think we need a strong dsb or tlbi variant here as we are setting
  // the TTBR register(s) for the first time right now to the identity map,
  // which should behave the same way as without the MMU/virtual memory enabled.
  // If we were ever to update these in the future (context-switching processes,
  // user-mode <-> kernel-mode), then we need to make sure that the system is
  // coherent and views the transition as a whole.
  if (exception_level == 0) {
    asm volatile(
     "msr   TTBR0_EL1, %0 \n\t\
      isb                 \n\t\
      "
      :: "r"(translation_table) : "memory"
    );
  } else if (exception_level == 1) {
    asm volatile(
     "msr   TTBR1_EL1, %0 \n\t\
      isb                 \n\t\
      "
      :: "r"(translation_table) : "memory"
    );
  } else {
    kpanic("Invalid exception level: %d\n", exception_level);
  }
}

struct PageTableEntry { uint64_t _entry; };

alignas(4096) static PageTableEntry L0_ID_PAGE_TABLE[512];
alignas(4096) static PageTableEntry L1_ID_PAGE_TABLE[512];

static uint64_t* table_entry(PageTableEntry* table, uint64_t idx) {
 return &table[idx]._entry;
}

static void pte_set_mair_attr(PageTableEntry* table, uint64_t idx, uint32_t mair_id) {
  kprecond(mair_id < 8);
  table[idx]._entry |= mair_id << 2;
}

static void pte_point_to_next_level(PageTableEntry* table, uint64_t idx, PageTableEntry* next_entry) {
  kassert((~(0xFFFFFFFFFULL<< 12) & (uint64_t)next_entry) == 0, "bad bits are set");

  // Next-level table address[47:m], with m = 12 for 4KB granule
  // So bit 47:12, 36 bits, are the address to the next level page table entry.
  // Since it must be 4KB aligned, the bottom 12 bits must be 0
  table[idx]._entry |= (uint64_t)next_entry;
}

static void pte_point_to_offset(PageTableEntry* table, uint64_t idx, uint64_t offset) {
  kassert((offset & 0xFFF) == 0, "offset must be aligned to 4K: %x\n", offset);
  // Output address[47:n]
  // For the 4KB granule size, the level 1 descriptor n is 30, and the level 2 descriptor n is 21.
  table[idx]._entry |= (uint64_t)offset;
}

static const uint64_t L1_ENTRY_SIZE = 1 << 30;
static const uint64_t NBITS_VAS = 48;

// Type of Page Table Entry
// Bit 0 indicates validity (1 = good, 0 = invalid), bit 1 indicates type:
// L3:    1 = page descriptor,  0 = invalid
// L0-L2: 1 = table descriptor, 0 = block descriptor
static const uint64_t PTE_TABLE_TYPE = 0b11;
static const uint64_t PTE_BLOCK_TYPE = 0b01;
static const uint64_t PTE_PAGE_TYPE  = 0b11;

// Lower attributes

static const uint64_t PTE_LA_AP_RW_EL1 = 0b00 << 6; // PrivRead, PrivWrite
static const uint64_t PTE_LA_SH_NONE = 0b00 << 8;
static const uint64_t PTE_LA_SH_INNER = 0b11 << 8;

// The AF in a Block descriptor and Page descriptor indicates one of the following:
// If the value is 0, then the memory region has not been accessed since the value of AF was last set to 0.
// If the value is 1, then the memory region has been accessed since the value of AF was last set to 0.
static const uint64_t PTE_LA_AF = 0b1 << 10;
static const uint64_t PTE_LA_AF_OFF = 0b1 << 10;

// Upper attributes

// Privileged Execute-never
static const uint64_t PTE_UA_PXN_EL1_ACCESS = (uint64_t)0b0 << 53;
static const uint64_t PTE_UA_PXN_EL1_NO_ACCESS = (uint64_t)0b1 << 53;

// Unprivileged Execute-never
static const uint64_t PTE_UA_UXN_EL0_ACCESS = (uint64_t)0b0 << 54;
static const uint64_t PTE_UA_UXN_EL0_NO_ACCESS = (uint64_t)0b1 << 54;

void MMU::setup_idmap_page_tables() {
  // Start by inserting the MAIR values
  setup_mair_ranges();

  // TODO: Detect the size of the kernel and adjust this accordingly

  // Start by setting up the single L0 page table entry by marking it as a Table
  // Descriptor type entry. Then point it to the next level (L1 PTE).
  *table_entry(L0_ID_PAGE_TABLE, 0) |= PTE_TABLE_TYPE;
  pte_point_to_next_level(L0_ID_PAGE_TABLE, 0, L1_ID_PAGE_TABLE);

  // TODO: The Access Flag should not be set here once exceptions are set up handling page faults


  // Device mapping
  *table_entry(L1_ID_PAGE_TABLE, 0) |=
    PTE_UA_UXN_EL0_NO_ACCESS | PTE_UA_PXN_EL1_NO_ACCESS |
    PTE_LA_AF | PTE_LA_SH_NONE | PTE_LA_AP_RW_EL1 |
    PTE_BLOCK_TYPE;
  pte_point_to_offset(L1_ID_PAGE_TABLE, 0, 0);
  pte_set_mair_attr(L1_ID_PAGE_TABLE, 0, MAIR_INDEX_DEVICE);

  // Normal mapping
  *table_entry(L1_ID_PAGE_TABLE, 1) |=
    PTE_UA_UXN_EL0_NO_ACCESS | PTE_UA_PXN_EL1_ACCESS |
    PTE_LA_AF | PTE_LA_SH_INNER | PTE_LA_AP_RW_EL1 |
    PTE_BLOCK_TYPE;
  pte_point_to_offset(L1_ID_PAGE_TABLE, 1, L1_ENTRY_SIZE);
  pte_set_mair_attr(L1_ID_PAGE_TABLE, 1, MAIR_INDEX_NORMAL_WB);

  set_ttbr(reinterpret_cast<uint64_t>(L0_ID_PAGE_TABLE), 0);

  setup_translation_control();

  enable();
}

void MMU::setup_translation_control() {
  // TCR_EL1 is the Translation Control Register. It configures stuff like:
  //  - How many bits are used for virtual addressing
  //  - The granule size/page size
  //  - Shareability
  //  - Whether TLB miss results in a translation fault or actual lookup

  // Bits [5:0]   T0SZ: VA 2^(64 - T0SZ), size of the memory region addressed by TTBR0_EL1
  // Bit  [7]     EPD0: This bit controls whether a translation table walk is performed
  //                    on a TLB miss, for an address that is translated using TTBR0_EL1
  //                    0 = walk on TLB miss, 1 = no walk (translation fault is generated)
  // Bits [9:8]   IRGN0: Inner cacheability attribute for memory associated with translation table walks using TTBR0_EL1
  // Bits [11:10] ORGN0: Outer cacheability attribute for memory associated with translation table walks using TTBR0_EL1
  //                     0b00 Normal memory, Inner/Outer Non-cacheable
  //                     0b01 Normal memory, Inner/Outer Write-Back Read-Allocate Write-Allocate Cacheable
  //                     0b10 Normal memory, Inner/Outer Write-Through Read-Allocate No Write-Allocate Cacheable
  //                     0b11 Normal memory, Inner/Outer Write-Back Read-Allocate No Write-Allocate Cacheable
  // Bits [13:12] SH0: Shareability attribute for memory associated with translation table walks using TTBR0_EL1
  //                   0b00 Non-shareable
  //                   0b10 Outer Shareable
  //                   0b11 Inner Shareable
  // Bits [15:14] TG0: Granule size for the TTBR0_EL1
  //                   0b00 4KB
  //                   0b01 64KB
  //                   0b10 16KB
  // Bits [21:16] T1SZ: VA 2^(64 - T0SZ), size of the memory region addressed by TTBR1_EL1
  // Bit  [22]    A1: Selects whether TTBR0_EL1 or TTBR1_EL1 defines the ASID. The encoding of this bit is:
  //                  0b0 TTBR0_EL1.ASID defines the ASID
  //                  0b1 TTBR1_EL1.ASID defines the ASID
  // Bit  [23]    EPD1: This bit controls whether a translation table walk is performed
  //                    on a TLB miss, for an address that is translated using TTBR1_EL1
  //                    0 = walk on TLB miss, 1 = no walk (translation fault is generated)
  // Bits [25:24] IRGN1
  // Bits [27:26] ORGN1
  // Bits [29:28] SH1
  // Bits [31:30] TG1
  // Bits [34:32] IPS: Intermediate Physical Address Size
  //                   0b101 48 bits, 256TB
  // Bit  [36]    AS: ASID Size. 0 = 8-bit, 1 = 16-bit
  // Bit  [37]    TBI0: Top Byte ignored Indicates whether the top byte of an address is used for address match for the
  //                    TTBR0_EL1 region, or ignored and used for tagged addresses.
  //                    0b0 Top Byte used in the address calculation
  //                    0b1 Top Byte ignored in the address calculation
  // Bit  [38]    TBI1: =/=, ut for TTBR1_EL1
  const uint64_t tcr_value =
    0b010000 << 0  | // T0SZ
    0b01     << 8  | // IRGN0
    0b01     << 10 | // ORGN0
    0b11     << 12 | // SH0
    0b010000 << 16 | // T1SZ
    0b0      << 23;  // EPD1

  asm volatile(
   "msr   TCR_EL1, %0 \n\t\
    isb               \n\t\
    "
    :: "r"(tcr_value) : "memory"
  );
}

// MMU enable for EL1&0 stage 1 address translation
static const uint64_t SCTLR_MMU_ENABLE = 1 << 0;

void MMU::enable() {
  // System Control Register (EL1)
  // We set SCTLR_EL1.M (MMU enable for EL1&0 stage 1 address translation)

  uint64_t temp;
  asm volatile(
   "mrs   %0, SCTLR_EL1 \n\t\
    orr   %0, %0, %1    \n\t\
    msr   SCTLR_EL1, %0 \n\t\
    tlbi  vmalle1       \n\t\
    dsb   sy            \n\t\
    isb                 \n\t\
    "
    : "=&r"(temp) : "i"(SCTLR_MMU_ENABLE) : "memory"
  );
}
