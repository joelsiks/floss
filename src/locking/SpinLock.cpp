
#include "SpinLock.h"

// The default SpinLock uses the exclusive operations ldaxr and stlxr to
// load/store a flag to.
//
// We have a memory location that's private to the class instance, "_lock",
// which is 0 when unlocked, and 1 when locked.
//
// When locking, we load the value using a ldxar, then see if it is unlocked,
// if unlocked set the value to 1 and try to update it using stlxr, otherwise
// branch back to loading and spin.
//
// When unlocking, we assume that we are definitely holding the lock and will
// just set the _lock value to 0 and store it using a store-release, stlr, which
// is not exclusive.

void SpinLock::lock() {
  uint32_t lock_read;
  uint32_t store_result;

  asm volatile(
    "1:                    \t\n\
     ldaxr  %w0, [%2]      \t\n\
     cbnz   %w0, 1b        \t\n\
     mov    %w0, #1        \t\n\
     stxr   %w1, %w0, [%2] \t\n\
     cbnz   %w1, 1b        \t\n\
     "
     : "=&r"(lock_read), "=&r"(store_result)
     : "r"(&_lock)
     : "memory"
  );
}

void SpinLock::unlock() {
  // TODO: In a debug build, assert/condition that the lock is actually held
  asm volatile(
    "stlr   wzr, [%0]"
     :
     : "r"(&_lock)
     : "memory"
  );
}

SpinLockGuard::SpinLockGuard(SpinLock* lock)
  : _lock(lock) {
  _lock->lock();
}

SpinLockGuard::~SpinLockGuard() {
  _lock->unlock();
}
