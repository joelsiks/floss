
#include "exception.h"
#include "kstdio.h"

uint64_t Exception::get_exception_level() {
  uint64_t el;
  asm("mrs %0, CurrentEL" : "=r" (el));
  el = el >> 2 & 0b11;
  return el;
}

// The ExceptionFrame contains the saved/dumped contents of the registers when
// the exception was thrown.
struct ExceptionFrame {
  uint64_t x0;
  uint64_t x1;
  uint64_t x2;
  uint64_t x3;
  uint64_t x4;
  uint64_t x5;
  uint64_t x6;
  uint64_t x7;
  uint64_t x8;
  uint64_t x9;
  uint64_t x10;
  uint64_t x11;
  uint64_t x12;
  uint64_t x13;
  uint64_t x14;
  uint64_t x15;
  uint64_t x16;
  uint64_t x17;
  uint64_t x18;
  uint64_t x29;
  uint64_t x30;
  uint64_t xzr;

  void print_frame() {
    kprintf("Register dump:\n");
    kprintf("  x0:  %p\n", x0);
    kprintf("  x1:  %p\n", x1);
    kprintf("  x2:  %p\n", x2);
    kprintf("  x3:  %p\n", x3);
    kprintf("  x4:  %p\n", x4);
    kprintf("  x5:  %p\n", x5);
    kprintf("  x6:  %p\n", x6);
    kprintf("  x7:  %p\n", x7);
    kprintf("  x8:  %p\n", x8);
    kprintf("  x9:  %p\n", x9);
    kprintf("  x10: %p\n", x10);
    kprintf("  x11: %p\n", x11);
    kprintf("  x12: %p\n", x12);
    kprintf("  x13: %p\n", x13);
    kprintf("  x14: %p\n", x14);
    kprintf("  x15: %p\n", x15);
    kprintf("  x16: %p\n", x16);
    kprintf("  x17: %p\n", x17);
    kprintf("  x18: %p\n", x18);
    kprintf("  x29: %p\n", x29);
    kprintf("  x30: %p\n", x30);
    kprintf("  xzr: %p\n", xzr);
  }
};

extern "C" bool Exception::exception_handler(ExceptionFrame* frame_ptr) {

  // We should read the ESR_EL1 (exception syndrome register) to figure out
  // what kind of exception has occurred.
  uint64_t syndrome = 0;
  asm ("mrs %0, ESR_EL1" : "=r" (syndrome));

  // bits [31, 26] represent the "exception class", i.e., what kind of exception
  // has occurred.
  const uint64_t ec = (syndrome >> 26) & 0b111111;
  const bool bad_ec = ec == 0;

  kprintf("The EC is: %d\n", ec);

  kprintf("\nGot an exception: %p\n", frame_ptr);
  frame_ptr->print_frame();

  return bad_ec;
}

extern "C" uint32_t Exception::irq_handler(ExceptionFrame* frame_ptr, uint32_t intid) {
  kprintf("\nGot an interrupt: %p, INTID: %d\n", frame_ptr, intid);
  frame_ptr->print_frame();

  if (intid == 30) {
    asm volatile(
     "mrs   x0, CNTFRQ_EL0    \n\t\
      msr   CNTP_TVAL_EL0, x0 \n\t\
      "
      ::: "memory"
    );
  }

  return intid;
}
