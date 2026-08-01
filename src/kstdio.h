#ifndef INCLUDE_KSTDIO
#define INCLUDE_KSTDIO

#include <cstdarg>

void kprintf(const char* format, ...);
void vkprintf(const char* format, va_list args);

#endif // INCLUDE_KSTDIO
