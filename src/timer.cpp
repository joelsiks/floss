
#include <cstdint>

#include "timer.h"

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
