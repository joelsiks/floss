#ifndef INCLUDE_LIBCSTUBS
#define INCLUDE_LIBCSTUBS

#include <cstdint>

extern "C" void memset(void* ptr, int c, uint64_t n);

extern "C" int strcmp(const char *s1, const char *s2);
extern "C" int strncmp(const char *s1, const char *s2, uint64_t n);

#endif // INCLUDE_LIBCSTUBS
