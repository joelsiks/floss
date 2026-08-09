
#include <cstring>

#include "DeviceTree.h"
#include "exception.h"
#include "GIC.h"
#include "kstdio.h"
#include "psci.h"
#include "uart.h"

extern "C" void secondary_main(uint64_t cpu_id) {
  kprintf("Running core %d\n", cpu_id);
  GIC::initialize_core_specific();
  Exception::unmask_interrupts();
}

extern "C" void* _secondary_start;

extern "C" void kern_main(DeviceTree::FlattenedDeviceTree* fdt) {
  // Start by parsing the flattened device tree so that we have MMIO addresses
  // set up before continuing the setup of the OS
  DeviceTree::Status status = DeviceTree::parse_frames(fdt);
  if (status != DeviceTree::Status::Ok) {
    return;
  }

  const uint64_t el = Exception::get_exception_level();
  kprintf("Kernel running at exception level: %d\n", el);

  GIC::initialize();

  //Timer::set_timer();
  //Timer::enable();

  Exception::unmask_interrupts();

  UART::pl011_toggle_rx_interrupts(true);

  PSCI::boot_core(1, (uint64_t)&_secondary_start, 1);
}
