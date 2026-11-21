
#include "VirtualAllocator.h"
#include "util/align.h"
#include "util/assert.h"

static constexpr uint64_t PAGE_SIZE = 4 * 1024;

Memory::VirtualAllocator::VirtualAllocator(uint8_t* start, uint8_t* end)
  : _start(start),
    _end(end) {
  kassert(is_aligned(uint64_t(start), PAGE_SIZE), "start is not aligned to the page size: %p\n", start);
  kassert(is_aligned(uint64_t(end), PAGE_SIZE), "end is not aligned to the page size: %p\n", end);
}

Memory::VirtualAllocator::VirtualAllocator(uint8_t* end)
  : _start(0),
    _end(end) { }

  Memory::VirtualAllocator::~VirtualAllocator() {}

uint8_t* Memory::VirtualAllocator::allocate(uint64_t pages) {
  uint8_t* const pages_start = _start;
  const uint64_t pages_bytes = pages * PAGE_SIZE;

  if (_start + pages_bytes > _end) {
    return nullptr;
  }

  _start += pages_bytes * PAGE_SIZE;

  return pages_start;
}

void Memory::VirtualAllocator::free(uint8_t* start, uint64_t pages) {
  (void)start;
  (void)pages;
  // TODO: Add a free-list for freeing, until this allocator has been redesigned
}
