#ifndef INCLUDE_LIBCSTUBS
#define INCLUDE_LIBCSTUBS

#include <cstddef>

extern "C" void memset(void* ptr, int c, size_t n);

extern "C" void* memcpy(void* dest, const void* src, size_t n);

extern "C" int strcmp(const char *s1, const char *s2);
extern "C" int strncmp(const char *s1, const char *s2, size_t n);

extern "C" size_t strlen(const char *s);
extern "C" size_t strnlen(const char *s, size_t maxlen);

#endif // INCLUDE_LIBCSTUBS
