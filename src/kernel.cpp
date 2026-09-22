
#include <cstring>

#include "DeviceTree.h"
#include "exception.h"
#include "interrupts/gic.h"
#include "kstdio.h"
#include "memory/map.h"
#include "memory/mmu.h"
#include "psci.h"
#include "timer.h"
#include "uart.h"

extern "C" void secondary_main(uint64_t cpu_id) {
  kprintf("Running core %d\n", cpu_id);
  GIC::driver()->initialize_core_specific();
  Exception::unmask_irqs();
}

extern "C" void kern_main(DeviceTree::FlattenedDeviceTree* fdt) {
  // Start by parsing the flattened device tree so that we have MMIO addresses
  // set up before continuing the setup of the OS
  DeviceTree::Status status = DeviceTree::parse_frames(fdt);
  if (status != DeviceTree::Status::Ok) {
    return;
  }

  UART::initialize();

  Memory::Map::init(fdt);

  const uint64_t el = Exception::get_exception_level();
  kprintf("Kernel running at exception level: %d\n", el);

  GIC::driver()->initialize();

  // Initialize specific interrupts
  GIC::driver()->initialize_interrupt(UART::intid(), GIC::InterruptPriority::Default);
  GIC::driver()->initialize_interrupt(Timer::intid(GIC::InterruptType::NonSecure), GIC::InterruptPriority::Default);

  Timer::set_timer();
  Timer::enable();

  MMU::setup_idmap_page_tables();

  // Call the secondary_main for the boot core
  secondary_main(0);

  // Then spin up and boot all other cores, which will call secondary_main
  // as well
  PSCI::boot_secondary_cores();
}
