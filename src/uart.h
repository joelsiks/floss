
#include <stdint.h>

namespace UART {
  void pl011_send_char(char c);
  void pl011_send_str(const char* str);

  char pl011_recv();
};
