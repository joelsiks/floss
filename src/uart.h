#ifndef INCLUDE_UART
#define INCLUDE_UART

#include <cstdint>

#include "DeviceTree.h"

namespace UART {
  void dt_parse(const DeviceTree::NodeFrame* node_frame);

  uint32_t intid();

  void initialize();

  void pl011_toggle_tx_interrupts(bool on);
  void pl011_toggle_rx_interrupts(bool on);

  char pl011_recv_sync();

  void pl011_send_char_sync(char c);
  void pl011_send_str_sync(const char* str);

  void pl011_send_char_async(char c);
  void pl011_send_str_async(const char* str);

  void pl011_handle_irq();
  void pl011_receive_async();
  void pl011_transmit_async();

  class CharBuffer {
  private:
    static const uint8_t BufferSize = 64;
    uint8_t _start;
    uint8_t _end;
    bool    _empty;
    char    _ring_buffer[BufferSize];

  public:
    CharBuffer();

    void buffer_char(char c);
    bool read_char(char& out_c);

    uint8_t elements_in_buffer() const;
    void print_buffer() const;
  };
};

#endif // INCLUDE_UART
