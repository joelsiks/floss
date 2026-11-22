#ifndef INCLUDE_MEMORY_MMU
#define INCLUDE_MEMORY_MMU

#include <cstdint>

#include "memory/FrameAllocator.h"

// floss' configuration are: 4 KiB translation granule, 48-bit virtual address
// space (VAS), kernel using the upper half of the 48-bit VAS, and tasks/processes
// using the lower half of the 48-bit VAS.

// During setup, all of RAM is identity mapped so that the kernel can continue
// executing noramlly when the MMU is enabled. This means that a page table
// hierarchy is installed for TTBR0_EL1, which is the lower half of the address
// space.
//
// Later on, we want the kernel to operate on the upper half of the address space,
// whose page table hierarchy is tracked in TTBR1_EL1. This allows TTBR1_EL1 to
// stay the same while we swap TTBR0 depending on the process/ASID of whatever is
// presently running on the core.
//
// With a 48-bit VAS (256 TiB), we get:
// Upper half: 0xffff000000000000 - 0xffffffffffffffff (128 TiB)
// Lower half: 0x0000000000000000 - 0x0000ffffffffffff (128 TiB)
//
// To make life easy for us, we will direct map RAM to the start of the kernel's
// upper half of the address space. To do this, we will need a dynamically allocated
// page table, which we can get after setting up an allocator for physical frames.

// A page table, which makes up one 512-entry array of page table entries, occupies
// 4096 bytes, so one page table covers a full physical frame. When setting up
// a dynamically allocated page table hierarchy, we allocate one physical frame
// for every page table, which covers 512-entries of whatever level it is installed
// on (L0, L1, L2, L3).
//
// A page table entry in a page table is marked as one specific descriptor. There
// are three types of descriptors: page descriptor (only valid for L3), table
// descriptor (L0-L2), and block descriptor (L0-L2).

namespace MMU {

  enum class TranslationRegime {
    Lower,
    Upper
  };

  void setup_mair_ranges();
  void setup_translation_control();
  void enable_translation();

  void set_ttbr(uint64_t translation_table, TranslationRegime regime);

  void setup_core_specific();

  void map_ram_identity();
  bool map_page(Memory::Frame physical_frame,  void* virtual_page);

  uint64_t get_pte_for_address(uintptr_t address);
};

#endif // INCLUDE_MEMORY_MMU
