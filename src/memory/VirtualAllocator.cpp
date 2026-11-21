
#include "VirtualAllocator.h"
#include "util/align.h"
#include "util/assert.h"

static constexpr uint64_t PAGE_SIZE = 4 * 1024;
static constexpr uint64_t VIRTUAL_ADDRESS_SPACE_SIZE = 128ULL * 1024 * 1024 * 1024 * 1024; // 128  TB

Memory::VirtualAllocator::VirtualAllocator(void* start, uint64_t size)
  : _start(start),
    _size(size),
    _lock() {
  kassert(is_aligned(reinterpret_cast<uint64_t>(start), PAGE_SIZE), "start is not aligned to the page size: %p\n", start);
  kassert(is_aligned(size, PAGE_SIZE), "size is not aligned to the page size: %p\n", size);
}

Memory::VirtualAllocator::VirtualAllocator(uint64_t size)
  : Memory::VirtualAllocator(nullptr, size) {}

void* Memory::VirtualAllocator::allocate(uint64_t pages) {
  SpinLockGuard guard(&_lock);

  void* const pages_start = _start;
  const uint64_t pages_bytes = pages * PAGE_SIZE;

  if (reinterpret_cast<uint64_t>(_start) + pages_bytes > _size) {
    return nullptr;
  }

  _start = reinterpret_cast<char*>(_start) + pages_bytes;

  return pages_start;
}

void Memory::VirtualAllocator::free(void* start, uint64_t pages) {
  SpinLockGuard guard(&_lock);
  (void)start;
  (void)pages;
  // TODO: Add a free-list for freeing, until this allocator has been redesigned
}

static Memory::VirtualAllocator _virtual_global(VIRTUAL_ADDRESS_SPACE_SIZE);

void* Memory::allocate_virtual_global(uint64_t pages) {
  return _virtual_global.allocate(pages);
}

void Memory::free_virtual_global(void* start, uint64_t pages) {
  _virtual_global.free(start, pages);
}
