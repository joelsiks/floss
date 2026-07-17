
#include <cstdarg>
#include <cstdbool>

#include "kstdio.h"

extern "C" void kern_main(void) {
  kprintf("Hello %d\n", 3);
  kprintf("Hello %d\n", 32);
  kprintf("Hello %d\n", -1234);
}
