
#include "exception.h"
#include "GIC.h"
#include "kstdio.h"
#include "timer.h"
#include "uart.h"

#include "DeviceTree.h"

#include "psci.h"

extern "C" void secondary_main(uint64_t cpu_id) {
  kprintf("Running core %d\n", cpu_id);
  GIC::initialize_core_specific();
  Exception::unmask_interrupts();
}

extern "C" void* _secondary_start;

// TODO: Move this to some better place
extern "C" void memset(void* ptr, int c, uint64_t n) {
  // TODO: Add an assert that 0 <= c <= 255
  uint8_t* const memory = reinterpret_cast<uint8_t*>(ptr);
  for (uint64_t i = 0; i < n; i++) {
    memory[i] = (uint8_t)c;
  }
}

extern "C" void kern_main(DeviceTree::FlattenedDeviceTree* fdt) {
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

  DeviceTree::Parser dtp;
  if (dtp.init(fdt) != DeviceTree::Status::Ok) {
    // TODO: Probably some form of kernel panic instead...
    kprintf("Failed to initialize FDT");
    return;
  }

  while (dtp.next() != DeviceTree::Token::End) {
    switch (dtp.current()) {
      case DeviceTree::Token::BeginNode:
        kprintf("BeginNode '%s'\n", dtp.node_name());
        break;
      case DeviceTree::Token::Prop:
        kprintf("Prop '%s' (len=%d)\n", dtp.prop_name(), dtp.prop_len());
        break;
      default:
        kprintf("Unhandled token type\n");
    }
  }
}
