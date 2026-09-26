#ifndef INCLUDE_UTIL_ALIGN
#define INCLUDE_UTIL_ALIGN

template <typename T>
inline T align_up(T value, T alignment) {
  return (value + alignment - 1) & ~(alignment - 1);
}

template <typename T>
inline T align_down(T value, T alignment) {
  return value & ~(alignment - 1);
}

#endif //INCLUDE_UTIL_ALIGN
