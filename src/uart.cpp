
#include "uart.h"

#include <cstring>

#include "DeviceTree.h"
#include "util/assert.h"
#include "memory/map.h"

// Memory Mapped Device Register for the Universal Asynchronous Receiver-Transmitter (UART)
struct MMDR_UART {
  volatile uint32_t DR;    // 0x00, Data Register,
  char _unused0[20];
  volatile uint32_t FR;    // 0x18 Flag Register
  char _unused1[8];
  volatile uint32_t IBRD;  // 0x24 Integer Baud Rate Register
  volatile uint32_t FBRD;  // 0x28 Fractional Baud Rate Register
  volatile uint32_t LCR_H; // 0x2C Line Control Register
  volatile uint32_t CR;    // 0x30 Control Register
  volatile uint32_t IFLS;  // 0x34 Interrupt Fifo Level Select
  volatile uint32_t IMSC;  // 0x38 Interrupt Mask Set Clear
  volatile uint32_t RIS;   // 0x3C Raw Interrupt Status Register
  volatile uint32_t MIS;   // 0x40 Masked Interrupt Status Register
  volatile uint32_t ICR;   // 0x44 Interrupt Clear Register
  volatile uint32_t DMACR; // 0x48 DMA Control Register
};

static MMDR_UART* uart = nullptr;

static DeviceTree::Interrupts _uart_interrupts;

static const uint32_t BAUD_RATE = 115200;
static uint32_t _uart_clock_frequency = 0;

static UART::CharBuffer _rx_buffer;
static UART::CharBuffer _tx_buffer;

static bool _irqs_ready = false;

void UART::dt_parse(const DeviceTree::NodeFrame* node_frame) {
  for (uint32_t i = 0; i < node_frame->_nprops; i++) {
    const DeviceTree::PropFrame* prop = &node_frame->_props[i];
    if (strcmp(prop->_name, "status") == 0 &&
        strcmp((const char*)prop->_value, "okay") != 0) {
      // If the UART node we matched the compatible string on has a status 
      // property that does not say "okay", we skip it.
      return;
    }
  }

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
          const uint64_t phys = DeviceTree::translate_address(node_frame, rp._address);

          // Record as Device memory
          Memory::record_device_region(phys, rp._length);

          uart = reinterpret_cast<MMDR_UART*>(phys);
        }

        current_value = (const char*)current_value + rp_size_bytes;
      }
    } else if (strcmp(prop->_name, "interrupts") == 0) {
      _uart_interrupts.reset();

      const uint32_t interrupt_cells = node_frame->_own_cells.lookup_interrupt_cells();
      const uint32_t num_interrupts = prop->_len / (interrupt_cells * sizeof(uint32_t));
      kprecond(num_interrupts <= _uart_interrupts.Capacity);

      for (uint32_t j = 0; j < num_interrupts; j++) {
        _uart_interrupts.register_intid(DeviceTree::read_interrupt_id(prop, j, interrupt_cells));
      }
    } else if(strcmp(prop->_name, "clocks") == 0) {
      const uint32_t clock_phandle = DeviceTree::Parser::read_u32(prop->_value);
      _uart_clock_frequency = DeviceTree::lookup_clock_frequency(clock_phandle);
    }
  }
}

uint32_t UART::intid() {
  return _uart_interrupts.get(0);
}

static void calculate_divisors(uint32_t& integer, uint32_t& fractional) {
  kprecond(_uart_clock_frequency != 0);
  // Cred: https://krinkinmu.github.io/2020/11/29/PL011.html#calculating-baudrate-divisiors
  // Although the UART (PL011) Reference Manual has great instructions as well

  // 64 * F_UARTCLK / (16 * B) = 4 * F_UARTCLK / B
  const uint32_t div = 4 * _uart_clock_frequency / BAUD_RATE;

  fractional = div & 0x3f;
  integer = (div >> 6) & 0xffff;
}

static const uint32_t FR_TXFF = 1 << 5;
static const uint32_t FR_RXFE = 1 << 4;

static void pl011_wait_poll_tx_complete() {
  while ((uart->FR & FR_TXFF) != 0) { }
}

static void pl011_wait_poll_rx_complete() {
  while ((uart->FR & FR_RXFE) != 0) { }
}

static const uint32_t CR_RXE = 1 << 9; // Receive enable
static const uint32_t CR_TXE = 1 << 8; // Transmit enable
static const uint32_t CR_UARTEN = 1 << 0; // UART Enable

static const uint32_t LCR_H_FEN = 1 << 4; // Enable FIFO

static const uint32_t DMACR_TXDMAE = 1 << 1; // Transmit DMA enable
static const uint32_t DMACR_RXDMAE = 1 << 0; // Receive DMA enable

void UART::initialize() {
  // Disable UART via UARTEN, RXE, and TXE bits
  uart->CR = (uart->CR & ~(CR_RXE | CR_TXE | CR_UARTEN));

  // Wait for any ongoing transmissions to complete
  pl011_wait_poll_tx_complete();

  // Flush FIFOs
  uart->LCR_H = (uart->LCR_H & (~LCR_H_FEN));

  // Configure baud rate/frequency
  uint32_t ibrd, fbrd;
  calculate_divisors(ibrd, fbrd);
  uart->IBRD = ibrd;
  uart->FBRD = fbrd;

  // Mask all interrupts
  UART::pl011_toggle_rx_interrupts(true);

  // Disable DMA
  uart->DMACR = (uart->DMACR & ~DMACR_RXDMAE);
  uart->DMACR = (uart->DMACR & ~DMACR_TXDMAE);

  // Enable UART via UARTEN, RXE, and TXE bits
  uart->CR = (uart->CR | CR_RXE | CR_TXE | CR_UARTEN);
}

void UART::signal_irqs_ready() {
  _irqs_ready = true;
}

// Bits for the Interrupt Mask Set Clear register
static const uint32_t IMSC_TXIM_BIT = 1 << 5;
static const uint32_t IMSC_RXIM_BIT = 1 << 4;

static void toggle_imsc_mask(bool on, uint32_t bit) {
  kprecond(uart != nullptr);

  // Read-Modify-Write. Other bits might be set and we only want to update
  // the bit that we're concerned with here.

  const uint32_t old_mask = uart->IMSC;

  const uint32_t new_mask = on
      ? old_mask | bit
      : old_mask & ~bit;

  uart->IMSC = new_mask;
}

void UART::pl011_toggle_tx_interrupts(bool on) {
  toggle_imsc_mask(on, IMSC_TXIM_BIT);
}

void UART::pl011_toggle_rx_interrupts(bool on) {
  toggle_imsc_mask(on, IMSC_RXIM_BIT);
}

char UART::pl011_recv_sync() {
  pl011_toggle_rx_interrupts(false);
  pl011_wait_poll_rx_complete();
  char c = uart->DR;
  pl011_toggle_rx_interrupts(true);
  return c;
}


void UART::pl011_send_char_sync(char c) {
  pl011_wait_poll_tx_complete();

  if (c == '\n') {
    uart->DR = (uint32_t)'\r';
    pl011_wait_poll_tx_complete();
  }

  uart->DR = c;
  pl011_wait_poll_tx_complete();
}

void UART::pl011_send_str_sync(const char* str) {
  pl011_wait_poll_tx_complete();

  const char* current = str;
  while (*current != '\0') {
    const char c = *current;
    if (c == '\n') {
      uart->DR = (uint32_t)'\r';
      pl011_wait_poll_tx_complete();
    }

    uart->DR = c;
    pl011_wait_poll_tx_complete();

    current++;
  }
}

static void pl011_drain_tx() {
  char c;
  while ((uart->FR & FR_TXFF) == 0 && _tx_buffer.read_char(c)) {
    uart->DR = c;
  }
}

void UART::pl011_send_char_async(const char c) {
  // Push character(s) to ring buffer
  if (c == '\n') {
    _tx_buffer.buffer_char((uint32_t)'\r');
  }

  _tx_buffer.buffer_char(c);

  // Drain as much as possible before enabling interrupts (or not if tx buffer
  // is empty), or synchronously drain the whole tx buffer if IRQs are not
  // enabled yet.

  pl011_drain_tx();

  if (_tx_buffer.elements_in_buffer() == 0) {
    pl011_toggle_tx_interrupts(false);
  } else if (_irqs_ready) {
    pl011_toggle_tx_interrupts(true);
  } else {
     // Synchronously drain the buffer if IRQs are not ready yet
     while (_tx_buffer.elements_in_buffer() != 0) {
       pl011_wait_poll_tx_complete();
       pl011_drain_tx();
     }

  }
}

void UART::pl011_send_str_async(const char* str) {
  // Push each character in str to ring buffer
  const char* current = str;
  while (*current != '\0') {
    const char c = *current;
    if (c == '\n') {
      pl011_send_char_async('\r');
    }

    pl011_send_char_async(c);
    current++;
  }
}

static const uint32_t MIS_TX = 1 << 5; // Transmit masked interrupt status
static const uint32_t MIS_RX = 1 << 4; // Receive masked interrupt status

void UART::pl011_handle_irq() {
  const uint32_t mis = uart->MIS;

  const bool tx = (mis & MIS_TX) != 0;
  const bool rx = (mis & MIS_RX) != 0;

  if (tx) {
    // We got here since the UART hardware sent an interrupt signaling that
    // there was room available in the UART's FIFO queue. Drain as much as
    // possible before either deferring again (wait for another TX interrupt)
    // or disabling TX interrupts if the tx buffer is empty.
    pl011_drain_tx();

    // TX interrupts is already enabled since we got here. If the buffer is
    // empty, we disable TX interrupts.
    if (_tx_buffer.elements_in_buffer() == 0) {
      pl011_toggle_tx_interrupts(false);
    }
  }

  if (rx) {
    // We got here since the UART hardware sent an interrupt signaling that
    // there was data to read, so a plain read of DR here is OK
    const char c = uart->DR;
    _rx_buffer.buffer_char(c);
    _rx_buffer.print_buffer();
  }

  uart->ICR = mis;
}

UART::CharBuffer::CharBuffer()
  : _start(0),
    _end(0),
    _ring_buffer() {}

// The CharBuffer is a ring buffer meant for buffering characters that are being
// received or are about to be sent.
//
// Initially, both _start and _end point at 0.
//
// When buffering a character, it is placed at the end of the ring buffer, where
// _end is pointing to. When reading a character from the ring buffer, it is read
// from the start of the ring buffer, where _start is pointing to.
//
// If _end is incremented so that it becomes equal to _start, _start is bumped
// and we "leak" one character from the buffer. This creates the "ring" effect,
// effectively writing over itself if the data loops back.
//

void UART::CharBuffer::buffer_char(char c) {
  _ring_buffer[_end] = c;

  // Increment end
  _end = (_end + 1) % BufferSize;

  if (_end == _start) {
    // If we bumped _end so that it is now equal to _start, we "leak" an element
    // by incrementing _start by one
    _start = (_start + 1) % BufferSize;
  }
}

bool UART::CharBuffer::read_char(char& out_c) {
  // If start and end are the same the ring buffer is empty
  if (_start == _end) {
    return false;
  }

  // Output the current element
  out_c = _ring_buffer[_start];

  // Increment the start by one
  _start = (_start + 1) % BufferSize;

  return true;
}

uint8_t UART::CharBuffer::elements_in_buffer() const {
  if (_start == _end) {
    return 0;
  } else if (_start < _end) {
    return _start - _end;
  } else {
    return BufferSize - _start + _end + 1;
  }
}

void UART::CharBuffer::print_buffer() const {
  // If start and end are the same the ring buffer is empty
  if (_start == _end) {
    return;
  }

  UART::pl011_send_str_async("Ring buffer content: ");

  uint8_t current = _start;
  do {
    UART::pl011_send_char_async(_ring_buffer[current]);
    current = (current + 1) % BufferSize;
  } while (current != _end);

  UART::pl011_send_char_async('\n');
}
