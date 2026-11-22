
#include "VirtualAllocator.h"

#include "util/align.h"
#include "util/assert.h"

static constexpr uint64_t PAGE_SIZE = 4 * 1024;
static constexpr uint64_t VIRTUAL_ADDRESS_SPACE_SIZE = 128ULL * 1024 * 1024 * 1024 * 1024; // 128  TB

Memory::VirtualAllocator::VirtualAllocator(uint64_t start, uint64_t size)
  : _start(start),
    _current(start),
    _size(size),
    _lock() {
  kassert(is_aligned(reinterpret_cast<uint64_t>(start), PAGE_SIZE), "start is not aligned to the page size: %p\n", start);
  kassert(is_aligned(size, PAGE_SIZE), "size is not aligned to the page size: %p\n", size);
}

Memory::VirtualAllocator::VirtualAllocator(uint64_t size)
  : Memory::VirtualAllocator(0, size) {}

void* Memory::VirtualAllocator::allocate(uint64_t pages) {
  if (pages == 0) {
    return nullptr;
  }

  SpinLockGuard guard(&_lock);

  uint64_t const pages_start = _current;
  const uint64_t pages_bytes = pages * PAGE_SIZE;

  if (_current + pages_bytes > _start + _size) {
    return nullptr;
  }

  _current = _current + pages_bytes;

  return reinterpret_cast<void*>(pages_start);
}

void Memory::VirtualAllocator::free(void* start, uint64_t pages) {
  if (start == nullptr || pages == 0) {
    return;
  }

  SpinLockGuard guard(&_lock);
  (void)start;
  (void)pages;
  // TODO: Add a free-list for freeing, until this allocator has been redesigned
}

static Memory::VirtualAllocator _virtual_global(0xffff000000000000UL, VIRTUAL_ADDRESS_SPACE_SIZE);

void* Memory::kernel_virtual_alloc(uint64_t pages) {
  return _virtual_global.allocate(pages);
}

void Memory::kernel_virtual_free(void* start, uint64_t pages) {
  _virtual_global.free(start, pages);
}
