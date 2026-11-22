
#include "mmu.h"

#include <cstdint>

#include "kstdio.h"
#include "memory/VirtualAllocator.h"
#include "memory/map.h"
#include "util/assert.h"
#include "util/align.h"

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
//             in a cache

// Device Memory. Non-Gathering, Non-Reordering, Non-Eearly Write Acknowledgement
static const uint64_t MAIR_DEVICE_NGNRNE = 0x00;

// Normal Memory. Outer+Inner Write-Back, Outer+Inner Read+Write Allocate, Non-Transient
// The read+write allocate means that a cache line is allocated on a read/write miss
//
// Encoding is: 0booooiiii
// For 'o' bits all one: Normal memory, Outer Write-Back Non-transient, Read Allocate, Write Allocate
// For 'i' bits all one: Normal memory, Inner Write-Back Non-transient, Read Allocate, Write Allocate
//
// We need this to be architecturally guaranteed that atomic instructions are atomic (!)
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

void MMU::setup_translation_control() {
  // Translation Control Register (TCR_EL1). It configures stuff like:
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
  // Bit  [22]    A1: Selects whether TTBR0_EL1 or TTBR1_EL1 defines the Address Space ID (ASID). The encoding of this bit is:
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
    0b00     << 14 | // TG0
    0b010000 << 16 | // T1SZ
    0b00     << 30 | // TG1
    0b101ULL << 32;  // IPS

  asm volatile(
   "msr   TCR_EL1, %0 \n\t\
    isb               \n\t\
    "
    :: "r"(tcr_value) : "memory"
  );
}

// MMU enable for EL1&0 stage 1 address translation
static const uint64_t SCTLR_MMU_ENABLE = 1 << 0;

// Stage 1 Cacheability control, for data accesses
static const uint64_t SCTLR_CACHEABILITY_ENABLE = 1 << 2;

void MMU::enable_translation() {
  // System Control Register (EL1). We set SCTLR_EL1.M (MMU enable for EL1&0
  // stage 1 address translation).

  uint64_t temp;
  asm volatile(
   "mrs   %0, SCTLR_EL1 \n\t\
    orr   %0, %0, %1    \n\t\
    orr   %0, %0, %2    \n\t\
    msr   SCTLR_EL1, %0 \n\t\
    tlbi  vmalle1       \n\t\
    dsb   sy            \n\t\
    isb                 \n\t\
    "
    : "=&r"(temp)
    : "i"(SCTLR_CACHEABILITY_ENABLE), "i"(SCTLR_MMU_ENABLE)
    : "memory"
  );
}

void MMU::set_ttbr(uint64_t translation_table, TranslationRegime regime) {
  // Translation Table Base Register (EL1) TTBR0_EL1, TTBR1_EL1.
  //
  // We are mapping the lower half for processes (user-space), and the upper
  // half for the kernel (kernel-space). The half is selected by the upper 16
  // bits of the virtual address. If the upper 16 bits are all 0s, TTBR0 is used.
  // If the upper 16 bits are all 1s, TTBR1 is used.
  //
  // Even though we are separating user- vs. kernel-space this way, permission
  // is configured separately in the translation/page tables.

  // I don't think we need a strong dsb or tlbi variant here as we are setting
  // the TTBR register(s) for the first time right now to the identity map,
  // which should behave the same way as without the MMU/virtual memory enabled.
  // If we were ever to update these in the future (e.g., context-switching
  // processes), then we need to make sure that the system is coherent and views
  // the transition as a whole.

  switch (regime) {
    case TranslationRegime::Lower:
      asm volatile(
       "msr   TTBR0_EL1, %0 \n\t\
        isb                 \n\t\
        "
        :: "r"(translation_table) : "memory"
      );
      break;
    case TranslationRegime::Upper:
      asm volatile(
       "msr   TTBR1_EL1, %0 \n\t\
        isb                 \n\t\
        "
        :: "r"(translation_table) : "memory"
      );
      break;
  }
}

// Page Table Entry types. Bit 0 indicates validity (1 = good, 0 = invalid),
// bit 1 indicates type:
// L3:    1 = Page descriptor,  0 = invalid
// L0-L2: 1 = Table descriptor, 0 = Block descriptor
static const uint64_t PTE_TABLE_TYPE = 0b11;
static const uint64_t PTE_BLOCK_TYPE = 0b01;
static const uint64_t PTE_PAGE_TYPE  = 0b11;

// Mask extracting the output address bits [47:12] from a descriptor
static const uint64_t PTE_ADDR_MASK = 0x0000FFFFFFFFF000ULL;

// Bit 0 of a descriptor indicates type and validity:
// 1 = Table descriptor, Block descriptor, or Page descriptor.
// 0 = invalid.
static bool pte_is_valid(const PageTableEntry* pte) {
  return (*pte & 0b1) != 0;
}

// Lower attributes

static const uint64_t PTE_LA_AP_RW_EL1 = 0b00 << 6; // PrivRead, PrivWrite
static const uint64_t PTE_LA_SH_NONE = 0b00 << 8;
static const uint64_t PTE_LA_SH_OUTER = 0b10 << 8;
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


// Statically allocated page tables for identity mapping RAM during bringup
alignas(4096) static PageTableEntry L0_ID_PAGE_TABLE[1];   // 512 GiB
alignas(4096) static PageTableEntry L1_ID_PAGE_TABLE[512]; // 512 GiB
alignas(4096) static PageTableEntry L2_ID_PAGE_TABLE[512 * 16]; // 16 GiB
static const uint32_t MaxNumL3PageTables = 32;
alignas(4096) static PageTableEntry L3_ID_PAGE_TABLES[MaxNumL3PageTables][512];
static uint32_t _num_l3_page_tables = 0;

// Dynamically allocated page tables for the kernel
static PageTableEntry* KernelPageTableBase = nullptr;

// Values for the kernel. TTBR0_EL1 will depend on the currently running process.
// The Translation Table Base Registers should contain the physical address of
// where translation walks should start from, i.e., should point to a L0 page table.
static PageTableEntry TTBR0EL1Value = reinterpret_cast<uint64_t>(L0_ID_PAGE_TABLE);
static PageTableEntry TTBR1EL1Value;

void MMU::setup_core_specific() {
  setup_mair_ranges();

  set_ttbr(TTBR0EL1Value, TranslationRegime::Lower);

  setup_translation_control();

  enable_translation();
}

static PageTableEntry* allocate_identity_l3_page_table() {
  kassert(_num_l3_page_tables < MaxNumL3PageTables, "Ran out of L3 page tables\n");
  PageTableEntry* table = L3_ID_PAGE_TABLES[_num_l3_page_tables];
  _num_l3_page_tables++;

  return table;
}

static PageTableEntry* identity_table_entry(PageTableEntry* table, uint64_t idx) {
  return &table[idx];
}

static void pte_set_mair_attr(PageTableEntry* pte, uint32_t mair_id) {
  kprecond(mair_id < 8);
  *pte |= mair_id << 2;
}

static void pte_point_to_next_level(PageTableEntry* pte, PageTableEntry* next_pte) {
  kassert((~(0xFFFFFFFFFULL << 12) & (uint64_t)next_pte) == 0, "bad bits are set");

  // Next-level table address[47:m], with m = 12 for 4KB granule
  // So bit 47:12, 36 bits, are the address to the next level page table entry.
  // Since it must be 4KB aligned, the bottom 12 bits must be 0
  *pte |= (uint64_t)next_pte;
}

static void pte_point_to_offset(PageTableEntry* pte, uint64_t offset) {
  kassert((offset & 0xFFF) == 0, "offset must be aligned to 4K: %x\n", offset);
  // Output address[47:n]
  // For the 4KB granule size, the level 1 descriptor n is 30, and the level 2 descriptor n is 21.
  *pte |= (uint64_t)offset;
}

static void pte_mark_as_table_descriptor(PageTableEntry* pte) {
  *pte |= PTE_TABLE_TYPE;
}

static void pte_mark_as_block_descriptor_normal(PageTableEntry* pte) {
  *pte |= PTE_UA_UXN_EL0_NO_ACCESS | PTE_UA_PXN_EL1_ACCESS |
          PTE_LA_AF | PTE_LA_SH_INNER | PTE_LA_AP_RW_EL1 |
          PTE_BLOCK_TYPE;
}

static void pte_mark_as_page_descriptor_normal(PageTableEntry* pte) {
  *pte |= PTE_UA_UXN_EL0_NO_ACCESS | PTE_UA_PXN_EL1_ACCESS |
          PTE_LA_AF | PTE_LA_SH_INNER | PTE_LA_AP_RW_EL1 |
          PTE_PAGE_TYPE;
}

static void pte_mark_as_block_descriptor_device(PageTableEntry* pte) {
  *pte |= PTE_UA_UXN_EL0_NO_ACCESS | PTE_UA_PXN_EL1_NO_ACCESS |
          PTE_LA_AF | PTE_LA_SH_NONE | PTE_LA_AP_RW_EL1 |
          PTE_BLOCK_TYPE;
}

static void pte_mark_as_page_descriptor_device(PageTableEntry* pte) {
  *pte |= PTE_UA_UXN_EL0_NO_ACCESS | PTE_UA_PXN_EL1_NO_ACCESS |
          PTE_LA_AF | PTE_LA_SH_NONE | PTE_LA_AP_RW_EL1 |
          PTE_PAGE_TYPE;
}

// Make sure the L1 entry covering l1_idx points to its L2 page table.
static void ensure_identity_l2_page_table(uint64_t l1_idx) {
  PageTableEntry* l1_entry = identity_table_entry(L1_ID_PAGE_TABLE, l1_idx);
  if (pte_is_valid(l1_entry)) {
    return;
  }

  pte_mark_as_table_descriptor(&L1_ID_PAGE_TABLE[l1_idx]);
  pte_point_to_next_level(&L1_ID_PAGE_TABLE[l1_idx], &L2_ID_PAGE_TABLE[512 * l1_idx]);
}

// Make sure the L2 entry covering `l2_idx` points to an L3 page table and
// return that L3 page table. Multiple device regions can share an L2 slot, so
// an existing L3 page table is reused rather than overwritten.
static PageTableEntry* ensure_identity_l3_page_table(uint64_t l1_idx, uint64_t l2_idx) {
  ensure_identity_l2_page_table(l1_idx);

  PageTableEntry* const l2_entry = identity_table_entry(L2_ID_PAGE_TABLE, l2_idx);
  if (pte_is_valid(l2_entry)) {
    return reinterpret_cast<PageTableEntry*>(*l2_entry & PTE_ADDR_MASK);
  }

  PageTableEntry* const l3_table = allocate_identity_l3_page_table();
  *l2_entry = reinterpret_cast<uint64_t>(l3_table) | PTE_TABLE_TYPE;
  return l3_table;
}

static void map_device_l2_block(uint64_t phys) {
  kprecond((phys % L2EntrySize) == 0);

  const uint64_t l2_idx = phys / L2EntrySize;

  ensure_identity_l2_page_table(phys / L1EntrySize);

  pte_mark_as_block_descriptor_device(identity_table_entry(L2_ID_PAGE_TABLE, l2_idx));
  pte_point_to_offset(&L2_ID_PAGE_TABLE[l2_idx], phys);
  pte_set_mair_attr(&L2_ID_PAGE_TABLE[l2_idx], MAIR_INDEX_DEVICE);

  //kprintf("VMSA: Identity mapping Device L2 [%p, %p)\n", phys, phys + L2_ENTRY_SIZE);
}

static void map_device_l3_page(uint64_t phys) {
  kprecond((phys % L3EntrySize) == 0);

  const uint64_t l2_idx = phys / L2EntrySize;
  const uint64_t l3_idx = (phys % L2EntrySize) / L3EntrySize;

  PageTableEntry* l3_table = ensure_identity_l3_page_table(phys / L1EntrySize, l2_idx);

  pte_mark_as_page_descriptor_device(identity_table_entry(l3_table, l3_idx));
  pte_point_to_offset(&l3_table[l3_idx], phys);
  pte_set_mair_attr(&l3_table[l3_idx], MAIR_INDEX_DEVICE);
  //kprintf("VMSA: Identity mapping Device L3 [%p, %p)\n", phys, phys + L3_ENTRY_SIZE);
}

// Map a device region using the smallest mappings possible: L2 (2MB) blocks
// for the parts that are 2MB aligned, and L3 (4K) pages for everything else.
//
// Device regions are validated to not overlap each other or RAM, which means a
// region that contains a full 2MB block owns that entire L2 slot. No other
// region can therefore map pages into it, so a single left-to-right pass can
// never mix a block and pages within one L2 slot.
static void map_device_region(const Memory::Region* region) {
  // Device memory must be mapped at page granularity, so round the region
  // outwards to whole 4K pages.
  const uint64_t start = align_down(region->_start, L3EntrySize);
  const uint64_t end = align_up(region->_start + region->_size, L3EntrySize);

  for (uint64_t addr = start; addr < end;) {
    const bool use_block = (addr % L2EntrySize) == 0 && (end - addr) >= L2EntrySize;

    if (use_block) {
      map_device_l2_block(addr);
      addr += L2EntrySize;
    } else {
      map_device_l3_page(addr);
      addr += L3EntrySize;
    }
  }
}

void MMU::map_ram_identity() {
  Memory::validate_device_regions();

  uint64_t highest_region = 0;

  // 1. Start by setting up the single L0 page table entry by marking it as a
  // Table descriptor. Then point it to the next level (L1 PTE).
  pte_mark_as_table_descriptor(&L0_ID_PAGE_TABLE[0]);
  pte_point_to_next_level(&L0_ID_PAGE_TABLE[0], L1_ID_PAGE_TABLE);

  // 2. Map all RAM as Normal memory
  Memory::RegionList* ram_regions = Memory::ram_regions();

  // Each RAM region might not be an integer multiple of 1 GiB, but maybe 2 MiB
  // or 4 KiB. If the region is 1 GiB, map an entire L1 entry, otherwise see if
  // we can map many 2 MiB entries, and if not, map many 4 KiB entries.
  for (uint64_t i = 0; i < ram_regions->_num_regions; i++) {
    Memory::Region* region = &ram_regions->_regions[i];
    kprintf("RAM region: [%p, %p), %p\n", region->_start, region->_start + region->_size, region->_size);

    const uint64_t region_top = align_up(region->_start + region->_size, L3EntrySize);
    if (region_top > highest_region) {
      highest_region = region_top;
    }

    if (region->_size % L1EntrySize == 0) {
      // Map as L1 Block descriptors

      const uint64_t ram_start_l1_idx = region->_start / L1EntrySize;
      const uint64_t ram_l1_mappings = region->_size / L1EntrySize;
      for (uint64_t i = 0; i < ram_l1_mappings; i++) {
        const uint64_t l1_idx = ram_start_l1_idx + i;

        // Normal WB identity mappings
        pte_mark_as_block_descriptor_normal(identity_table_entry(L1_ID_PAGE_TABLE, l1_idx));
        pte_point_to_offset(&L1_ID_PAGE_TABLE[l1_idx], l1_idx * L1EntrySize);
        pte_set_mair_attr(&L1_ID_PAGE_TABLE[l1_idx], MAIR_INDEX_NORMAL_WB);
        //kprintf("VMSA: Identity mapping Normal L1 [%p, %p)\n", l1_idx * L1_ENTRY_SIZE, (l1_idx + 1) * L1_ENTRY_SIZE);
      }

    } else if (region->_size % L2EntrySize == 0) {
      // Map as L2 Block descriptors inside L1 Table descriptors

      // Map all L1 entries in the region as Table descriptors
      const uint64_t start_l1_idx = region->_start / L1EntrySize;
      const uint64_t l1_mappings = (region->_size + L1EntrySize - 1) / L1EntrySize;
      for (uint64_t i = 0; i < l1_mappings; i++) {
        const uint64_t l1_idx = start_l1_idx + i;
        pte_mark_as_table_descriptor(&L1_ID_PAGE_TABLE[l1_idx]);
        pte_point_to_next_level(&L1_ID_PAGE_TABLE[l1_idx], &L2_ID_PAGE_TABLE[512 * l1_idx]);
      }

      // Then map all L2 entries as Block descriptors
      const uint64_t start_l2_idx = region->_start / L2EntrySize;
      const uint64_t l2_mappings = region->_size / L2EntrySize;
      for (uint64_t i = 0; i < l2_mappings; i++) {
        const uint64_t l2_idx = start_l2_idx + i;
        pte_mark_as_block_descriptor_normal(identity_table_entry(L2_ID_PAGE_TABLE, l2_idx));
        pte_point_to_offset(&L2_ID_PAGE_TABLE[l2_idx], l2_idx * L2EntrySize);
        pte_set_mair_attr(&L2_ID_PAGE_TABLE[l2_idx], MAIR_INDEX_NORMAL_WB);
        //kprintf("VMSA: Identity mapping Normal L2 [%p, %p)\n", l2_idx * L2_ENTRY_SIZE, (l2_idx + 1) * L2_ENTRY_SIZE);
      }
    } else {
      kpanic("Identity map with 4K pages is not implemented yet\n");
      kassert(region->_size % L3EntrySize == 0, "Invalid alignment/size of ram region: %z\n", region->_size);
    }
  }

  // 3. Map Device regions as Device
  Memory::RegionList* device_regions = Memory::device_regions();

  for (uint32_t i = 0; i < device_regions->_num_regions; i++) {
    const Memory::Region* region = &device_regions->_regions[i];
    const uint64_t region_top = align_up(region->_start + region->_size, L3EntrySize);
    if (region_top > highest_region) {
      highest_region = region_top;
    }

    kprintf("Device region: [%p, %p), %p\n", region->_start, region->_start + region->_size, region->_size);
    map_device_region(region);
  }

  // TODO: The Access Flag should not be set here once exceptions are set up handling page faults
  setup_core_specific();

  Memory::kernel_virtual_alloc(align_up(highest_region, L3EntrySize));
}

static PageTableEntry allocate_page_table() {
  // Allocate one physical frame for the page table
  Memory::Frame physical_frame;
  if (!Memory::FrameAllocator::allocate_frame(&physical_frame)) {
    return 0;
  }

  return reinterpret_cast<PageTableEntry>(physical_frame._address);
}

static void zero_page_table(PageTableEntry* page_table) {
  // Zero-initialize the entire physical frame covering the page table
  memset(reinterpret_cast<void*>(page_table), 0, 4096);
}

static void ensure_l0_entry(uint64_t idx) {
  if (KernelPageTableBase == 0) {
    // Allocate a new page table for the L0 level. We only ever need one so when
    // this is allocated it won't have to be done again. The L0 entry covers
    // 512 * 512 GiB = 256 TiB, and we only have 128 TiB to cover, since the VAS
    // is split into lower/upper half of 128 TiB each.

    PageTableEntry page_table = allocate_page_table();
    TTBR1EL1Value = page_table;
    KernelPageTableBase = reinterpret_cast<PageTableEntry*>(page_table);

    // Zero-initialize the entire page we just allocated so that all page table
    // entries on it will become invalid
    zero_page_table(&page_table);
  }

  kprecond(KernelPageTableBase != nullptr);

  PageTableEntry* const pte = &KernelPageTableBase[idx];

  if (!pte_is_valid(pte)) {
    pte_mark_as_table_descriptor(pte);
  }
}

static void ensure_l1_entry() {}
static void ensure_l2_entry() {}
static void ensure_l3_entry() {}

static const uint64_t uppermask = 0x0000ffffffffffff;

bool MMU::map_page(Memory::Frame physical_frame, void *virtual_page) {
  const uint64_t virtual_page_bits = reinterpret_cast<uint64_t>(virtual_page);
  const uint64_t virtual_address = virtual_page_bits & uppermask;

  // Start by checking if the arguments are valid
  if ((virtual_address % L3EntrySize) != 0 || (physical_frame._address % L3EntrySize) != 0) {
    return false;
  }

  TranslationRegime regime;
  if ((virtual_page_bits & ~uppermask) == 0) {
    regime = TranslationRegime::Lower;
  } else if ((virtual_page_bits & ~uppermask) == ~uppermask) {
    regime = TranslationRegime::Upper;
  } else {
    kassert(false, "Upper 16 bits are not all ones or zeros: %p\n", virtual_page);
  }

  const uint64_t l0_idx = virtual_address / L0EntrySize;
  ensure_l0_entry(l0_idx);

  const uint64_t l1_idx = virtual_address / L1EntrySize;
  const uint64_t l2_idx = virtual_address / L2EntrySize;
  const uint64_t l3_idx = (virtual_address % L2EntrySize) / L3EntrySize;


  if ((L1_ID_PAGE_TABLE[l1_idx] & 0b11) == PTE_BLOCK_TYPE) {
    return false;
  }

  ensure_identity_l2_page_table(l1_idx);
  PageTableEntry* const l2_entry = identity_table_entry(L2_ID_PAGE_TABLE, l2_idx);
  PageTableEntry* l3_table;

  if (pte_is_valid(*l2_entry)) {
    if ((*l2_entry & 0b11) != PTE_TABLE_TYPE) {
      return false;
    }
    l3_table = reinterpret_cast<PageTableEntry*>(*l2_entry & PTE_ADDR_MASK);
  } else {
    l3_table = allocate_identity_l3_page_table();
    *l2_entry = reinterpret_cast<uint64_t>(l3_table) | PTE_TABLE_TYPE;
  }

  PageTableEntry* const entry = identity_table_entry(l3_table, l3_idx);
  if (pte_is_valid(*entry)) {
    return false;
  }

  // Point the page table entry at the physical frame and set attributes
  pte_mark_as_page_descriptor_normal(entry);
  pte_point_to_offset(entry, physical_frame._address);
  pte_set_mair_attr(entry, MAIR_INDEX_NORMAL_WB);

  return true;
}

uint64_t MMU::get_pte_for_address(uintptr_t address) {

  uint32_t l1_idx = address / L1EntrySize;

  const uint64_t l1_entry = L1_ID_PAGE_TABLE[l1_idx];
  const uint64_t l1_type = l1_entry & 0b11;

  if (l1_type == 0) {
    return 0;
  } else if (l1_type == PTE_BLOCK_TYPE) {
    // If it is block type, we're done.
    return l1_entry;
  } else if (l1_type == PTE_TABLE_TYPE) {
    // If it is table type, we go down a level

    const uint64_t l2_entry = *(uint64_t*)(l1_entry & (0xFFFFFFFFF << 12));
    const uint64_t l2_type = l2_entry & 0b11;
    kprintf("Got L2 entry: %p\n", l2_entry);

    if (l2_type == 0) {
      return 0;
    } else if (l2_type == PTE_BLOCK_TYPE) {
      return l2_entry;
    } else if (l2_type == PTE_TABLE_TYPE) {
      const uint64_t l3_entry = *(uint64_t*)(l2_entry & (0xFFFFFFFFF << 12));
      kprintf("Got L3 entry: %p\n", l3_entry);

      return l3_entry;
    }
  }

  return 0;
}
