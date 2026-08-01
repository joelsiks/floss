
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
        if (strcmp(current_node, "psci") == 0) {
          if (strcmp(dtp.prop_name(), "cpu_on") == 0) {
            const void* val = dtp.prop_value();
            uint64_t v = DeviceTree::Parser::read_u32(val);
            PSCI::set_cpu_on(v);
          } else if (strcmp(dtp.prop_name(), "method") == 0) {
            PSCI::set_method(reinterpret_cast<const char*>(dtp.prop_value()));
          }
        } else if(strncmp(current_node, "pl011", 5) == 0) {
          if (strcmp(dtp.prop_name(), "reg")) {
            uint64_t v = DeviceTree::Parser::read_u64(dtp.prop_value());
            kprintf("%d %p\n", dtp.prop_len(), v);
          }

          kprintf("prop name %s\n", dtp.prop_name());
        }
        break;
      case DeviceTree::Token::End:
      case DeviceTree::Token::Nop:
        continue;
    }
  }

  GIC::initialize();

  //Timer::set_timer();
  //Timer::enable();

  Exception::unmask_interrupts();

  UART::pl011_toggle_rx_interrupts(true);

  PSCI::boot_core(1, (uint64_t)&_secondary_start, 1);
}
