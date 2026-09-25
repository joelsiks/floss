
#include <cstdarg>

#include "kstdio.h"
#include "uart.h"

template <typename T>
static void kprintf_print_number(T number, int base) {
  char number_buf[20];

  // Exit early if number is zero
  if (number == 0) {
    UART::pl011_send_char_sync('0');
    return;
  }

  // Handle negative numbers
  if (number < 0) {
    UART::pl011_send_char_sync('-');
    number *= -1;
  }

  int max_idx = 0;

  while (number != 0) {
    const int remainder = number % base;

    if (remainder < 10) {
      number_buf[max_idx] = '0' + remainder;
    } else {
      number_buf[max_idx] = 'a' + remainder - 10;
    }

    number /= base;
    max_idx++;
  }

  for (int i = max_idx - 1; i >= 0; i--) {
    UART::pl011_send_char_sync(number_buf[i]);
  }
}

void kprintf(const char* format, ...) {
  va_list args;
  va_start(args, format);
  vkprintf(format, args);
  va_end(args);
}

void vkprintf(const char *format, va_list args) {
  const char* current = format;
  while (*current != '\0') {
    // If we encounter a '%' and the next character is not the null-terminator,
    // then the next character is the format. We are not supporting escaped
    // percentage signs here.
    if (*current == '%') {
      const char specifier = *(current + 1);

      if (specifier != '\0') {
        if (specifier == 'd') {
          const int number = va_arg(args, int);
          kprintf_print_number(number, 10);
        } else if (specifier == 'z') {
          const uint64_t number = va_arg(args, uint64_t);
          kprintf_print_number(number, 10);
        } else if (specifier == 's') {
          const char* str = va_arg(args, const char*);
          if (str != nullptr) {
            UART::pl011_send_str_sync(str);
          } else {
            UART::pl011_send_str_sync("(null)");
          }
        } else if (specifier == 'p') {
          const void* number = va_arg(args, void*);
          UART::pl011_send_str_sync("0x");
          kprintf_print_number((uint64_t)number, 16);
        } else if (specifier == 'c') {
          const int char_as_num = va_arg(args, int);
          UART::pl011_send_char_sync((char)char_as_num);
        }

        current++;
      }
    } else {
      // Normal case, just send the character
      UART::pl011_send_char_sync(*current);
    }

    current++;
  }
}
