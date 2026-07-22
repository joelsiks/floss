
#include "GIC.h"
#include "kstdio.h"
#include "exception.h"

extern "C" void kern_main(void) {
  GIC::init_gic_distributor();
  GIC::init_gic_redistributor();

  GIC::set_interrupt_priority(30, 90);
  GIC::set_interrupt_group(30);
  GIC::enable_interrupt(30);

  GIC::set_priority_mask(0xFF);

  const uint64_t el = Exception::get_exception_level();
  kprintf("Exception Level: %d\n", el);
}
