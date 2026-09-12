
#include "timer.h"

#include <cstdint>
#include <cstring>

#include "util/assert.h"

// Index - type
// 0 - secure physical
// 1 - non-secure physical
// 2 - virtual
// 3 - hypervisor
static const uint32_t TimerIntidsCapacity = 5;
static uint32_t num_timer_intids = 0;
static uint32_t timer_intids[TimerIntidsCapacity];

void Timer::dt_parse(const DeviceTree::NodeFrame* node_frame) {
  for (uint32_t i = 0; i < node_frame->_nprops; i++) {
    const DeviceTree::PropFrame* prop = &node_frame->_props[i];

    if (strcmp(prop->_name, "interrupts") == 0) {
      const uint32_t num_interrutps = prop->_len / (DeviceTree::InterruptCells * sizeof(uint32_t));

      for (uint32_t j = 0; j < num_interrutps; j++) {
        kprecond(num_timer_intids <= TimerIntidsCapacity);
        timer_intids[j] = DeviceTree::read_interrupt_id(prop, j);
        num_timer_intids++;
      }
    }
  }
}

uint32_t Timer::intid(GIC::InterruptType interrupt_type) {
  switch (interrupt_type) {
    case GIC::InterruptType::Secure:
      kprecond(num_timer_intids >= 1);
      return timer_intids[0];
      break;
    case GIC::InterruptType::NonSecure:
      kprecond(num_timer_intids >= 2);
      return timer_intids[1];
      break;
    case GIC::InterruptType::Virtual:
      kprecond(num_timer_intids >= 3);
      return timer_intids[2];
      break;
    case GIC::InterruptType::Hypervisor:
      kprecond(num_timer_intids >= 4);
      return timer_intids[3];
      break;
    default:
      kpanic("Should not reach here");
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
