
#include <stdbool.h>

#include "uart.h"

extern "C" void kern_main(void) {
  UART::pl011_send_str("Hello, World!");
  UART::pl011_send_char(UART::pl011_recv());
}
