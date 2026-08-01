
#include "exception.h"
#include "GIC.h"
#include "kstdio.h"
#include "timer.h"
#include "uart.h"
#include "libcstubs.h"

#include "DeviceTree.h"

#include "psci.h"

extern "C" void secondary_main(uint64_t cpu_id) {
  kprintf("Running core %d\n", cpu_id);
  GIC::initialize_core_specific();
  Exception::unmask_interrupts();
}

extern "C" void* _secondary_start;

extern "C" void kern_main(DeviceTree::FlattenedDeviceTree* fdt) {
  const uint64_t el = Exception::get_exception_level();
  kprintf("Kernel running at exception level: %d\n", el);

  GIC::initialize();

  //Timer::set_timer();
  //Timer::enable();

  Exception::unmask_interrupts();

  UART::pl011_toggle_rx_interrupts(true);

  DeviceTree::Parser dtp;
  if (dtp.init(fdt) != DeviceTree::Status::Ok) {
    // TODO: Probably some form of kernel panic instead...
    kprintf("Failed to initialize FDT");
    return;
  }

  const char* current_node = nullptr;
  while (dtp.next() != DeviceTree::Token::End) {

    switch (dtp.current()) {
      case DeviceTree::Token::BeginNode:
        current_node = dtp.node_name();
        break;
      case DeviceTree::Token::EndNode:
        current_node = nullptr;
        break;
      case DeviceTree::Token::Prop:
        //kprintf("%s %s %d %p\n", current_node, dtp.prop_name(), dtp.prop_len(), dtp.prop_value());
        if (strcmp(current_node, "psci") == 0) {
          if (strcmp(dtp.prop_name(), "cpu_on") == 0) {
            PSCI::initialize_cpu_on(reinterpret_cast<uint64_t>(dtp.prop_value()));
          } else if (strcmp(dtp.prop_name(), "method") == 0) {
            PSCI::initialize_method(reinterpret_cast<const char*>(dtp.prop_value()));
          }
        }
        break;
    }
  }

  PSCI::boot_core(1, (uint64_t)&_secondary_start, 1);
}
