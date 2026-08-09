
#include "libcstubs.h"

#include "util/assert.h"

void memset(void* ptr, int c, size_t n) {
  char* const memory = reinterpret_cast<char*>(ptr);
  for (size_t i = 0; i < n; i++) {
    memory[i] = (char)c;
  }
}

int strcmp(const char *s1, const char *s2) {
  while (*s1 && (*s1 == *s2)) {
    s1++;
    s2++;
  }

  return (unsigned char)*s1 - (unsigned char)*s2;
}

int strncmp(const char *s1, const char *s2, size_t n) {
  size_t characters_left = n;
  while (*s1 && (*s1 == *s2) && characters_left > 1) {
    s1++;
    s2++;
    characters_left--;
  }

  return (unsigned char)*s1 - (unsigned char)*s2;
}

size_t strlen(const char* s) {
  const char* begin = s;
  for(; *s; s++);
  return s - begin;
}

size_t strnlen(const char* s, size_t maxlen) {
  const char* begin = s;

  for(; *s; s++) {
    if ((size_t)(s - begin) == maxlen) {
      return maxlen;
    }
  }

  return s - begin;
}
