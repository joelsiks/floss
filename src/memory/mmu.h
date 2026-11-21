#ifndef INCLUDE_MEMORY_MMU
#define INCLUDE_MEMORY_MMU

#include <cstdint>

#include "memory/FrameAllocator.h"

namespace MMU {

  enum class TranslationRegime {
    Lower,
    Upper
  };

  void setup_mair_ranges();
  void set_ttbr(uint64_t translation_table, TranslationRegime regime);
  void setup_translation_control();
  void enable();

  void setup_core_specific();

  void map_ram_identity();
  bool map_page(Memory::Frame physical_frame,  void* virtual_page);

  uint64_t get_pte_for_address(uintptr_t address);
};

#endif // INCLUDE_MEMORY_MMU
