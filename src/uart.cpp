
#include "uart.h"

// Memory Mapped Device Register for the Universal Asynchronous Receiver-Transmitter (UART)
struct MMDR_UART {
  volatile uint32_t DR; // 0x00, Data Register,
    // - Bit 5 (TXFF) — transmit FIFO full. Set ⇒ wait before writing.
    // - Bit 4 (RXFE) — receive FIFO empty. Set ⇒ no byte available yet.
  char _unused0[20];
  volatile uint32_t FR; // 0x18, Flag Register
};

// TODO: This should really be found using the Device Tree.
static MMDR_UART* uart = reinterpret_cast<MMDR_UART*>(0x09000000);

static void pl011_wait_tx_complete() {
  while ((uart->DR & 0b100000) != 0) { }
}

static void pl011_wait_rx_complete() {
  while ((uart->FR & 0b010000) != 0) { }
}

void UART::pl011_send_char(char c) {
  pl011_wait_tx_complete();

  if (c == '\n') {
    uart->DR = (uint32_t)'\r';
    pl011_wait_tx_complete();
  }

  uart->DR = c;
  pl011_wait_tx_complete();
}

void UART::pl011_send_str(const char* str) {
  pl011_wait_tx_complete();

  const char* current = str;
  while (*current != '\0') {
    const char s = *current;
    if (s == '\n') {
      uart->DR = (uint32_t)'\r';
      pl011_wait_tx_complete();
    }

    uart->DR = s;
    pl011_wait_tx_complete();

    current++;
  }
}

char UART::pl011_recv() {
  pl011_wait_rx_complete();
  return uart->DR;
}
