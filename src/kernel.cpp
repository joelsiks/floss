
#include <cstring>

#include "DeviceTree.h"
#include "exception.h"
#include "GIC.h"
#include "kstdio.h"
#include "memory/map.h"
#include "memory/mmu.h"
#include "psci.h"
#include "timer.h"
#include "uart.h"

extern "C" void secondary_main(uint64_t cpu_id) {
  kprintf("Running core %d\n", cpu_id);
  GIC::initialize_core_specific();
  Exception::unmask_irqs();
}

extern "C" void kern_main(DeviceTree::FlattenedDeviceTree* fdt) {
  // Start by parsing the flattened device tree so that we have MMIO addresses
  // set up before continuing the setup of the OS
  DeviceTree::Status status = DeviceTree::parse_frames(fdt);
  if (status != DeviceTree::Status::Ok) {
    return;
  }

  Memory::Map::init(fdt);

  const uint64_t el = Exception::get_exception_level();
  kprintf("Kernel running at exception level: %d\n", el);

  GIC::initialize();

  Timer::set_timer();
  Timer::enable();

  Exception::unmask_irqs();

  UART::pl011_toggle_rx_interrupts(true);

  MMU::setup_idmap_page_tables();

  PSCI::boot_secondary_cores();
}
