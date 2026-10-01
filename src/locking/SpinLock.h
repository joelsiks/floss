#ifndef INCLUDE_LOCKING_SPINLOCK
#define INCLUDE_LOCKING_SPINLOCK

#include <cstdint>

class SpinLock {
private:
  uint32_t _lock{0};

public:
  SpinLock() noexcept = default;

  SpinLock(const SpinLock&) = delete;
  SpinLock& operator=(const SpinLock&) = delete;
  SpinLock(SpinLock&&) = delete;
  SpinLock& operator=(SpinLock&&) = delete;

  void lock();
  void unlock();
};

class SpinLockGuard {
private:
  SpinLock& _lock;

public:
  SpinLockGuard(SpinLock& lock);
  ~SpinLockGuard();
};

#endif // INCLUDE_LOCKING_SPINLOCK
