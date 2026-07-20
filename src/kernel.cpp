
#include "kstdio.h"
#include "exception.h"

extern "C" void kern_main(void) {
  const uint64_t el = Exception::get_exception_level();
  kprintf("Exception Level: %d\n", el);
}
