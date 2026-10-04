#include "kstdio.h"

#include <cstdarg>

#include "uart.h"

template <void(*SendChar)(char), void(*SendStr)(const char*)>
static void kprintf_print_number(uint64_t number, int base, bool signed_int) {
  char number_buf[20];

  // Exit early if number is zero
  if (number == 0) {
    SendChar('0');
    return;
  }

  // Handle negative numbers (only meaningful for signed %d)
  if (signed_int && (int64_t)number < 0) {
    SendChar('-');
    number = (uint64_t)(-(int64_t)number);
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
    SendChar(number_buf[i]);
  }
}

template <void(*SendChar)(char), void(*SendStr)(const char*)>
static void vkprintf_impl(const char* format, va_list args) {
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
          kprintf_print_number<SendChar, SendStr>((uint64_t)number, 10, true);
        } else if (specifier == 'z') {
          const uint64_t number = va_arg(args, uint64_t);
          kprintf_print_number<SendChar, SendStr>(number, 10, false);
        } else if (specifier == 's') {
          const char* str = va_arg(args, const char*);
          if (str != nullptr) {
            SendStr(str);
          } else {
            SendStr("(null)");
          }
        } else if (specifier == 'p') {
          const void* number = va_arg(args, void*);
          SendStr("0x");
          kprintf_print_number<SendChar, SendStr>((uint64_t)number, 16, false);
        } else if (specifier == 'c') {
          const int char_as_num = va_arg(args, int);
          SendChar((char)char_as_num);
        }

        current++;
      }
    } else {
      // Normal case, just send the character
      SendChar(*current);
    }

    current++;
  }
}

void kprintf(const char* format, ...) {
  va_list args;
  va_start(args, format);
  vkprintf(format, args);
  va_end(args);
}

void vkprintf(const char *format, va_list args) {
  vkprintf_impl<UART::pl011_send_char_async, UART::pl011_send_str_async>(format, args);
}

void kprintf_sync(const char* format, ...) {
  va_list args;
  va_start(args, format);
  vkprintf_sync(format, args);
  va_end(args);
}

void vkprintf_sync(const char* format, va_list args) {
  vkprintf_impl<UART::pl011_send_char_sync, UART::pl011_send_str_sync>(format, args);
}