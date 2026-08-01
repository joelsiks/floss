
#include "util/assert.h"

#include <cstdlib>

#include "kstdio.h"

void report_error(const char* file, int line, const char* error_msg, const char* detail_fmt, ...) {
  va_list args;
  va_start(args, detail_fmt);

  kprintf("==========================================\n");
  kprintf("Error at %s:%d:\n", file, line);
  kprintf(error_msg);
  vkprintf(detail_fmt, args);

  va_end(args);
}
