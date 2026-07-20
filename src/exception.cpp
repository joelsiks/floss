
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
};

extern "C" void Exception::exception_handler(ExceptionFrame* frame_ptr) {
  kprintf("\nGot an exception: %p\n", frame_ptr);
  kprintf("Register dump:\n");
  kprintf("  x0:  %p\n", frame_ptr->x0);
  kprintf("  x1:  %p\n", frame_ptr->x1);
  kprintf("  x2:  %p\n", frame_ptr->x2);
  kprintf("  x3:  %p\n", frame_ptr->x3);
  kprintf("  x4:  %p\n", frame_ptr->x4);
  kprintf("  x5:  %p\n", frame_ptr->x5);
  kprintf("  x6:  %p\n", frame_ptr->x6);
  kprintf("  x7:  %p\n", frame_ptr->x7);
  kprintf("  x8:  %p\n", frame_ptr->x8);
  kprintf("  x9:  %p\n", frame_ptr->x9);
  kprintf("  x10: %p\n", frame_ptr->x10);
  kprintf("  x11: %p\n", frame_ptr->x11);
  kprintf("  x12: %p\n", frame_ptr->x12);
  kprintf("  x13: %p\n", frame_ptr->x13);
  kprintf("  x14: %p\n", frame_ptr->x14);
  kprintf("  x15: %p\n", frame_ptr->x15);
  kprintf("  x16: %p\n", frame_ptr->x16);
  kprintf("  x17: %p\n", frame_ptr->x17);
  kprintf("  x18: %p\n", frame_ptr->x18);
  kprintf("  x29: %p\n", frame_ptr->x29);
  kprintf("  x30: %p\n", frame_ptr->x30);
  kprintf("  xzr: %p\n", frame_ptr->xzr);
}
