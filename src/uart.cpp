
#include "uart.h"

// Memory Mapped Device Register for the Universal Asynchronous Receiver-Transmitter (UART)
struct MMDR_UART {
  volatile uint32_t DR;   // 0x00, Data Register,
                          // - Bit 5 (TXFF) — transmit FIFO full. Set ⇒ wait before writing.
                          // - Bit 4 (RXFE) — receive FIFO empty. Set ⇒ no byte available yet.
  char _unused0[20];
  volatile uint32_t FR;   // 0x18, Flag Register
  char _unused1[21];
  volatile uint32_t IFLS; // 0x34 Interrupt Fifo Level Select
  volatile uint32_t IMSC; // 0x38 Interrupt Mask Set Clear
  volatile uint32_t RIS;  // 0x3C Raw Interrupt Status Register
  volatile uint32_t MIS;  // 0x40 Masked Interrupt Status Register
};

// TODO: This should really be found using the Device Tree.
static MMDR_UART* uart = reinterpret_cast<MMDR_UART*>(0x09000000);

// Bits for the Interrupt Mask Set Clear register
static const uint32_t IMSC_RXIM_BIT = 1 << 4;
static const uint32_t IMSC_TXIM_BIT = 1 << 5;

static void toggle_imsc_mask(bool on, uint32_t bit) {
  // Read-Modify-Write. Other bits might be set and we only want to update
  // the bit that we're concerned with here.

  const uint32_t old_mask = uart->IMSC;

  const uint32_t new_mask = on
      ? old_mask | bit
      : old_mask & ~bit;

  uart->IMSC = new_mask;
}

void UART::pl011_toggle_rx_interrupts(bool on) {
  toggle_imsc_mask(on, IMSC_RXIM_BIT);
}

void UART::pl011_toggle_tx_interrupts(bool on) {
  toggle_imsc_mask(on, IMSC_TXIM_BIT);
}

static void pl011_wait_poll_tx_complete() {
  while ((uart->FR & 0b100000) != 0) { }
}

static void pl011_wait_poll_rx_complete() {
  while ((uart->FR & 0b010000) != 0) { }
}

void UART::pl011_send_char(char c) {
  pl011_wait_poll_tx_complete();

  if (c == '\n') {
    uart->DR = (uint32_t)'\r';
    pl011_wait_poll_tx_complete();
  }

  uart->DR = c;
  pl011_wait_poll_tx_complete();
}

void UART::pl011_send_str(const char* str) {
  pl011_wait_poll_tx_complete();

  const char* current = str;
  while (*current != '\0') {
    const char s = *current;
    if (s == '\n') {
      uart->DR = (uint32_t)'\r';
      pl011_wait_poll_tx_complete();
    }

    uart->DR = s;
    pl011_wait_poll_tx_complete();

    current++;
  }
}

char UART::pl011_recv_sync() {
  pl011_wait_poll_rx_complete();
  return uart->DR;
}
