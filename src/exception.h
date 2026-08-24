#ifndef INCLUDE_EXCEPTION
#define INCLUDE_EXCEPTION

#include <cstdint>
#include <cstdint>

struct ExceptionFrame;

namespace Exception {
  // Retrieves the current exception level
  uint64_t get_exception_level();

  // Retrieves the cpu/core/PE id of the current core
  uint64_t get_cpuid();

  // These methods mask/unmask the IRQ bit in the DAIF register
  void mask_irqs();
  void unmask_irqs();

  // Handler routines. Entered via assembly in the VBAR table
  extern "C" bool exception_handler(ExceptionFrame* frame_ptr);
  extern "C" uint32_t irq_handler(ExceptionFrame* frame_ptr, uint32_t intid);
};

#endif // INCLUDE_EXCEPTION
