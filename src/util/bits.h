#ifndef INCLUDE_UTIL_BITS
#define INCLUDE_UTIL_BITS

template <typename T>
inline T bits_clz(T value) {
  T lz, temp;

  // 1. Invert all bits (turning 0s to 1s)
  // 2. Reverse all bits
  // 3. Count leading zeros (using clz, starting from most significant bit)

  asm(
   "mvn  %1, %2 \n\t\
    rbit %1, %1 \n\t\
    clz  %0, %1 \n\t\
    "
    : "=r"(lz), "=&r"(temp) : "r"(value));

  return lz;
}

#endif //INCLUDE_UTIL_BITS
