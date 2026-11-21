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

  void setup_idmap_page_tables();

  void setup_translation_control();

  void enable();

  void setup_core_specific();

  uint64_t get_pte_for_address(uintptr_t address);

  bool map_page(Memory::Frame physical_frame,  void* virtual_page);
};

#endif // INCLUDE_MEMORY_MMU
