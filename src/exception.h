
#ifndef INLCUDE_EXCEPTION
#define INLCUDE_EXCEPTION

#include <cstdint>
#include <cstdint>

struct ExceptionFrame;

namespace Exception {
  uint64_t get_exception_level();

  extern "C" bool exception_handler(ExceptionFrame* frame_ptr);
  extern "C" uint32_t irq_handler(ExceptionFrame* frame_ptr, uint32_t intid);
};

#endif // INCLUDE_EXCEPTION
