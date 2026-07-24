
#include "exception.h"
#include "GIC.h"
#include "kstdio.h"
#include "timer.h"
#include "uart.h"

extern "C" void kern_main(void) {
  GIC::initialize();

  Timer::set_timer();
  Timer::enable();

  Exception::unmask_interrupts();

  UART::pl011_toggle_rx_interrupts(true);

  const uint64_t el = Exception::get_exception_level();
  kprintf("Kernel running at exception level: %d\n", el);
}
