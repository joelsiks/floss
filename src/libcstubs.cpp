
#include "libcstubs.h"

void memset(void* ptr, int c, uint64_t n) {
  // TODO: Add an assert that 0 <= c <= 255
  uint8_t* const memory = reinterpret_cast<uint8_t*>(ptr);
  for (uint64_t i = 0; i < n; i++) {
    memory[i] = (uint8_t)c;
  }
}

int strcmp(const char *s1, const char *s2) {
  while (*s1 && (*s1 == *s2)) {
    s1++;
    s2++;
  }

  return (unsigned char)*s1 - (unsigned char)*s2;
}

int strncmp(const char *s1, const char *s2, uint64_t n) {
  uint64_t characters_left = n;
  while (*s1 && (*s1 == *s2) && characters_left > 1) {
    s1++;
    s2++;
    characters_left--;
  }

  return (unsigned char)*s1 - (unsigned char)*s2;
}
