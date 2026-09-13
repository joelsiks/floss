
#include "timer.h"

#include <cstdint>
#include <cstring>

#include "util/assert.h"


static DeviceTree::Interrupts _timer_interrupts;

void Timer::dt_parse(const DeviceTree::NodeFrame* node_frame) {
  for (uint32_t i = 0; i < node_frame->_nprops; i++) {
    const DeviceTree::PropFrame* prop = &node_frame->_props[i];

    if (strcmp(prop->_name, "interrupts") == 0) {
      _timer_interrupts.reset();

      const uint32_t interrupt_cells = node_frame->_own_cells.lookup_interrupt_cells();
      const uint32_t num_interrupts = prop->_len / (interrupt_cells * sizeof(uint32_t));
      kprecond(num_interrupts <= _timer_interrupts.Capacity);

      for (uint32_t j = 0; j < num_interrupts; j++) {
        _timer_interrupts.register_intid(DeviceTree::read_interrupt_id(prop, j, interrupt_cells));
      }
    }
  }
}

uint32_t Timer::intid(GIC::InterruptType interrupt_type) {
  switch (interrupt_type) {
    case GIC::InterruptType::Secure:
      return _timer_interrupts.get(0);
      break;
    case GIC::InterruptType::NonSecure:
      return _timer_interrupts.get(1);
      break;
    case GIC::InterruptType::Virtual:
      return _timer_interrupts.get(2);
      break;
    case GIC::InterruptType::Hypervisor:
      return _timer_interrupts.get(3);
      break;
    default:
      ShouldNotReachHere();
      return 0;
  }
}

static void toggle_timer(bool on) {
  const uint64_t value = on ? 1 : 0;
  asm volatile(
   "mov   x0, %0           \n\t\
    msr   CNTP_CTL_EL0, x0 \n\t\
    "
    : : "r"(value) : "memory"
  );
}

void Timer::enable() {
  toggle_timer(true);
}

void Timer::disable() {
  toggle_timer(false);
}

void Timer::set_timer() {
  asm volatile(
   "mrs   x0, CNTFRQ_EL0    \n\t\
    msr   CNTP_TVAL_EL0, x0 \n\t\
    " ::: "memory"
  );
}
