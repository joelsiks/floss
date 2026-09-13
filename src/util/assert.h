#ifndef INCLUDE_UTIL_ASSERT
#define INCLUDE_UTIL_ASSERT

#include <cstdarg>

// This file details Errors that can occur in the floss kernel.
// An Error is either an Assert or a Panic. An assert takes in a predicate

void report_error(const char* file, int line, const char* error_msg, const char* detail_fmt, ...);

// TODO: Make this function [[noreturn]] and gracefully exit the OS
//void report_error_and_die(const char* file, int line, const char* error_msg, const char* detail_fmt, ...);
#define kprecond(p) kassert(p, "precond\n")
#define kpostcond(p) kassert(p, "postcond\n")

#define kassert(p, ...) \
  do { \
    if (!(p)) report_error(__FILE__, __LINE__, "kassert(" #p ") failed: ", __VA_ARGS__); \
  } while (0);

#define kpanic(...) \
  report_error(__FILE__, __LINE__, "Kernel panic: ", __VA_ARGS__)

#define ShouldNotReachHere() kpanic("ShouldNotReachHere")

#endif //INCLUDE_UTIL_ASSERT
