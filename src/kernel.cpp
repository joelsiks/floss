
#include "GIC.h"
#include "kstdio.h"
#include "exception.h"

extern "C" void kern_main(void) {
  GIC::init_gic_distributor();
  GIC::init_gic_redistributor();

  const uint64_t el = Exception::get_exception_level();
  kprintf("Exception Level: %d\n", el);

  uint64_t timer = 0;

  asm ("mrs %0, CNTP_TVAL_EL0" : "=r" (timer));
  kprintf("Timer: %d\n", timer);

  asm ("mrs %0, CNTP_TVAL_EL0" : "=r" (timer));
  kprintf("Timer: %d\n", timer);
}
