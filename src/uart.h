#ifndef INCLUDE_UART
#define INCLUDE_UART

#include <stdint.h>

#include "DeviceTree.h"

namespace UART {
  void dt_parse(const DeviceTree::NodeFrame* node_frame);

  void pl011_toggle_rx_interrupts(bool on);
  void pl011_toggle_tx_interrupts(bool on);

  void pl011_send_char(char c);
  void pl011_send_str(const char* str);

  char pl011_recv_sync();
  char pl011_recv_async();

  class ReceiveBuffer {
  private:
    static const uint8_t BufferSize = 16;
    uint8_t _start;
    uint8_t _end;
    bool    _empty;
    char    _ring_buffer[BufferSize];

  public:
    ReceiveBuffer();

    void buffer_char(char c);
    bool read_char(char& out_c);

    uint8_t elements_in_buffer() const;
    void print_buffer() const;
  };
};

#endif // INCLUDE_UART
