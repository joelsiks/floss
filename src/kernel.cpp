
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
  // In QEMU, if translation is disabled, the default memory type is Device, which
  // requires alignment checking, even though SCTLR_EL1.A is set to 0. To get around
  // this, we setup the identity map of virtual memory at the very start. This feels
  // hacky, but it solves the problem right now. Another solution is to compile with
  // "-mstrict-align", or just make sure everything that is handled early on in the
  // kernel is aligned to 8 bytes.
  //
  // In QEMU >=v9.0.0
  // See: https://gitlab.com/qemu-project/qemu/-/commit/59754f85ed35cbd5f4bf2663ca2136c78d5b2413

  MMU::setup_idmap_page_tables();

  // Start by parsing the flattened device tree so that we have MMIO addresses
  // set up before continuing the setup of the OS
  DeviceTree::Status status = DeviceTree::parse_frames(fdt);
  if (status != DeviceTree::Status::Ok) {
    return;
  }

  UART::initialize();

  kprintf("Booting the floss kernel :)\n");
  kprintf("====================================\n");

  Memory::Map::init(fdt);

  const uint64_t el = Exception::exception_level();
  kprintf("Kernel running at exception level: %d\n", el);

  GIC::driver()->initialize();

  // Initialize specific interrupts
  GIC::driver()->initialize_interrupt(UART::intid(), GIC::InterruptPriority::Default);
  GIC::driver()->initialize_interrupt(Timer::intid(GIC::InterruptType::NonSecure), GIC::InterruptPriority::Default);

  Timer::set_timer();
  Timer::enable();

  // Call the secondary_main for the boot core
  secondary_main(0);

  // Then spin up and boot all other cores, which will call secondary_main
  // as well
  PSCI::boot_secondary_cores();
}
