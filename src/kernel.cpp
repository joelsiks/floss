
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

  char c = UART::pl011_recv_sync();
  kprintf("Got character: %d\n", c);

  //const uint64_t el = Exception::get_exception_level();
  //kprintf("Exception Level: %d\n", el);
}
