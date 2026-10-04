#ifndef INCLUDE_KSTDIO
#define INCLUDE_KSTDIO

#include <cstdarg>

void kprintf(const char* format, ...);
void vkprintf(const char* format, va_list args);

// Synchronous variants that poll the UART directly and bypass the TX ring
// buffer / TX interrupts. Use these from contexts where IRQs may be masked
// (e.g. the synchronous exception handler) so output is not lost when the
// ring buffer overflows while no TX interrupt can fire to drain it.
void kprintf_sync(const char* format, ...);
void vkprintf_sync(const char* format, va_list args);

#endif // INCLUDE_KSTDIO