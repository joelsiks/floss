
#include "uart.h"

#include <cstring>

#include "DeviceTree.h"
#include "util/assert.h"

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

static MMDR_UART* uart = nullptr;

static uint32_t gic_intid = 0;

void UART::dt_parse(const DeviceTree::NodeFrame* node_frame) {
  // Iterate over all the props
  for (uint32_t i = 0; i < node_frame->_nprops; i++) {
    const DeviceTree::PropFrame* prop = &node_frame->_props[i];

    if (strcmp(prop->_name, "reg") == 0) {
      const void* current_value = prop->_value;
      const uint32_t rp_size_bytes = node_frame->_parent_cells.byte_size();
      const uint32_t num_reg_pairs = prop->_len / rp_size_bytes;

      kassert(prop->_len % rp_size_bytes == 0, "Invalid reg length (%d, rp size %d)\n", prop->_len, rp_size_bytes);

      DeviceTree::RegPair rp;
      for (uint32_t j = 0; j < num_reg_pairs; j++) {
        DeviceTree::read_reg_pair(&node_frame->_parent_cells, current_value, &rp);
        if (j == 0) {
          uart = reinterpret_cast<MMDR_UART*>(rp._address);
        }

        current_value = (const char*)current_value + rp_size_bytes;
      }
    } else if (strcmp(prop->_name, "interrupts")) {
       const uint32_t type = DeviceTree::Parser::read_u32(prop->_value);
       const uint32_t number = DeviceTree::Parser::read_u32((const uint8_t*)prop->_value + 4);
       // flags = read_u32(... + 8);  // trigger type, ignore for now
       if (type == 0) {
         gic_intid = 32 + number; // SPI
       } else if (type == 1) {
         gic_intid = 16 + number; // PPI
       }
    }
  }
}

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
  pl011_toggle_rx_interrupts(false);
  pl011_wait_poll_rx_complete();
  char c = uart->DR;
  pl011_toggle_rx_interrupts(true);
  return c;
}

char UART::pl011_recv_async() {
  return uart->DR;
}

UART::ReceiveBuffer::ReceiveBuffer()
  : _start(0),
    _end(0),
    _empty(true),
    _ring_buffer() {}

void UART::ReceiveBuffer::buffer_char(char c) {
  _ring_buffer[_start] = c;

  const bool was_same = _start == _end;

  // Increment start
  _start = (_start + 1) % BufferSize;

  if (was_same && !_empty) {
    // If start and end pointed to the same place and the buffer was not empty,
    // then we "leak" an element by incrementing the end
    _end = _start;
  }

  // We just added something to the buffer, so it is no longer empty
  _empty = false;
}

bool UART::ReceiveBuffer::read_char(char& out_c) {
  if (_empty) {
    return false;
  }

  // Output the current element
  out_c = _ring_buffer[_end];

  _end = (_end + 1) % BufferSize;

  if (_end == _start) {
    // If the end is now equal to the start, then the buffer has been drained
    // and is now empty
    _empty = true;
  }

  return true;
}

uint8_t UART::ReceiveBuffer::elements_in_buffer() const {
  if (_empty) {
    return 0;
  } else if (_start <= _end) {
    return BufferSize + _start - _end;
  } else {
    return _start - _end;
  }
}

void UART::ReceiveBuffer::print_buffer() const {
  if (_empty) {
    return;
  }

  UART::pl011_send_str("Ring buffer content: ");

  uint8_t current = _start;
  do {
    UART::pl011_send_char(_ring_buffer[current]);
    current = (current + 1) % BufferSize;
  } while (current != _end);

  UART::pl011_send_char('\n');
}
