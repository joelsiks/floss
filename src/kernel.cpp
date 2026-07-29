
#include "exception.h"
#include "GIC.h"
#include "kstdio.h"
#include "timer.h"
#include "uart.h"

#include "psci.h"

extern "C" void secondary_main(uint64_t cpu_id) {
  kprintf("Running core %d\n", cpu_id);
  GIC::initialize_core_specific();
  Exception::unmask_interrupts();
}

extern "C" void* _secondary_start;

extern "C" void kern_main(void) {
  const uint64_t el = Exception::get_exception_level();
  kprintf("Kernel running at exception level: %d\n", el);

  GIC::initialize();

  //Timer::set_timer();
  //Timer::enable();

  Exception::unmask_interrupts();

  UART::pl011_toggle_rx_interrupts(true);

  const PSCIInfo psci_info = PSCI::information();
  kprintf("PSCI version: %d.%d\n", psci_info._major, psci_info._minor);

  PSCI::boot_core(1, (uint64_t)&_secondary_start, 1);
}
