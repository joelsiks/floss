#ifndef INCLUDE_EXCEPTION
#define INCLUDE_EXCEPTION

#include <cstdint>
#include <cstdint>

struct ExceptionFrame;

namespace Exception {
  // Retrieves the current exception level
  uint64_t exception_level();

  // Masks/unmasks the IRQ bit in the DAIF register
  void mask_irqs();
  void unmask_irqs();

  // Handler routines. Entered via assembly in the VBAR table trampolines
  extern "C" bool exception_handler(ExceptionFrame* frame_ptr);
  extern "C" uint32_t irq_handler(uint32_t intid, ExceptionFrame* frame_ptr);
};

#endif // INCLUDE_EXCEPTION
