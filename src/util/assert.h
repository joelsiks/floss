#ifndef INCLUDE_UTIL_ASSERT
#define INCLUDE_UTIL_ASSERT

#include <cstdarg>

// This file exposes an interface for asserting/documenting/upholding invariants,
// as well as things that definitely should not happen.

[[noreturn]] void report_error(const char* file, int line, const char* error_msg, const char* detail_fmt, ...);

#define kprecond(p) kassert(p, "precond\n")
#define kpostcond(p) kassert(p, "postcond\n")

#define kassert(p, ...) \
  do { \
    if (!(p)) report_error(__FILE__, __LINE__, "kassert(" #p ") failed: ", __VA_ARGS__); \
  } while (0);

#define kpanic(...) \
  report_error(__FILE__, __LINE__, "Kernel panic: ", __VA_ARGS__)

#define kShouldNotReachHere() kpanic("ShouldNotReachHere")

#endif //INCLUDE_UTIL_ASSERT
