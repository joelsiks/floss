
#include "uart.h"

static volatile uint32_t* uart_base = (uint32_t*)0x09000000;

static const uint32_t DR_OFFSET = 0x00; // Data Register
static const uint32_t FR_OFFSET = 0x18; // Flag Register
// - Bit 5 (TXFF) — transmit FIFO full. Set ⇒ wait before writing.
// - Bit 4 (RXFE) — receive FIFO empty. Set ⇒ no byte available yet.

static volatile uint32_t* reg(uint32_t offset) {
  return uart_base + (offset / sizeof(*uart_base));
}

static void pl011_wait_tx_complete() {
  while ((*reg(FR_OFFSET) & 0b100000) != 0) { }
}

static void pl011_wait_rx_complete() {
  while ((*reg(FR_OFFSET) & 0b010000) != 0) { }
}

void UART::pl011_send_char(char c) {
  volatile uint32_t* const dr = reg(DR_OFFSET);

  pl011_wait_tx_complete();

  if (c == '\n') {
    *dr = (uint32_t)'\r';
    pl011_wait_tx_complete();
  }

  *dr = c;
  pl011_wait_tx_complete();
}

void UART::pl011_send_str(const char* str) {
  volatile uint32_t* const dr = reg(DR_OFFSET);

  pl011_wait_tx_complete();

  const char* current = str;
  while (*current != '\0') {
    const char s = *current;
    if (s == '\n') {
      *dr = (uint32_t)'\r';
      pl011_wait_tx_complete();
    }

    *dr = s;
    pl011_wait_tx_complete();

    current++;
  }
}

char UART::pl011_recv() {
  pl011_wait_rx_complete();
  return *reg(DR_OFFSET);
}
