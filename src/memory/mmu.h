#ifndef INCLUDE_MMU
#define INCLUDE_MMU

#include <cstdint>

namespace MMU {
  void setup_mair_ranges();
  void set_ttbr(uint64_t translation_table, uint32_t exception_level);

  void setup_idmap_page_tables();

  void setup_translation_control();

  void enable();
};

#endif // INCLUDE_MMU
