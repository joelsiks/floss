
#include "kmalloc.h"

#include "FrameAllocator.h"
#include "locking/SpinLock.h"
#include "memory/VirtualAllocator.h"
#include "util/align.h"
#include "util/assert.h"
#include "mmu.h"

// The kernel malloc is a simple free-list allocator that uses a first-fit
// approach to find suitable blocks. When the allocator is out of backing
// storage, it allocates a new physical frame and a virtual page and maps them,
// and then enters it into the free-list, and retries the allocation.
//
// The free-list is made up of "blocks". Each block has a BlockHeader that
// immediately precedes the memory it is representing. The BlockHeader is
// necessary so that when memory is freed, we know how large the block is.

struct BlockHeader {
  BlockHeader* _next;
  uint64_t _size;

  void* to_ptr() {
    return reinterpret_cast<void*>(reinterpret_cast<char*>(this) + sizeof(BlockHeader));
  }

  static BlockHeader* from_ptr(void* pointer) {
    return reinterpret_cast<BlockHeader*>(reinterpret_cast<char*>(pointer) - sizeof(BlockHeader));
  }
};

static constexpr uint64_t PAGE_SIZE = 4 * 1024;
static constexpr uint64_t MinBlockSize = 16;

// The free-list is a single size class and updates to it must be synchronized
// via kmalloc_lock. The block header is stored intrusively in the node it
// represents, so no additional memory is needed for bookkeeping.
static SpinLock _kmalloc_lock;
static BlockHeader* _free_list;

static bool grow_by_one_page() {
  // Allocate one physical frame
  Memory::Frame physical_frame;
  if (!Memory::FrameAllocator::allocate_frame(&physical_frame)) {
    return false;
  }

  // Allocate one virtual page
  void* const virtual_page = Memory::kernel_virtual_alloc(1);
  if (virtual_page == nullptr) {
    // Free the physical frame if we fail to get a virtual page
    Memory::FrameAllocator::free_frame(&physical_frame);
    return false;
  }

  // Map the virtual page to the physical frame
  if (!MMU::map_page(physical_frame, virtual_page)) {
    // Free the physical frame and virtual page if we fail to map
    Memory::FrameAllocator::free_frame(&physical_frame);
    Memory::kernel_virtual_free(virtual_page, 1);

    return false;
  }

  // Mapping succeeded. Insert the new block in the free-list.
  BlockHeader* const new_block = reinterpret_cast<BlockHeader*>(virtual_page);
  new_block->_next = _free_list;
  new_block->_size = PAGE_SIZE - sizeof(BlockHeader);
  _free_list = new_block;

  return true;
}

void* kmalloc(uint64_t size) {
  if (size == 0) {
    return nullptr;
  }

  const uint64_t requested_size = align_up(size, MinBlockSize);
  if (requested_size > PAGE_SIZE - sizeof(BlockHeader)) {
    kassert(false, "Large kmalloc allocations are not supported yet...\n");
    return nullptr;
  }

  SpinLockGuard guard(&_kmalloc_lock);

  for (;;) {
    BlockHeader* prev_block = nullptr;
    BlockHeader* current_block = _free_list;
    while (current_block != nullptr) {
      // The size of the block is just what we need
      if (current_block->_size == requested_size) {
        if (prev_block == nullptr) {
          // This is the first block, update the _free_list head
          _free_list = current_block->_next;
        } else {
          // Unlink block
          prev_block->_next = current_block->_next;
        }

        current_block->_next = nullptr;
        return current_block->to_ptr();
      }

      // Block is large enough to split off a new block if it is large enough
      // for: 1) The requested size + 2) The size of a new Block header +
      // 3) Enough left for the current block
      if (current_block->_size >= requested_size + sizeof(BlockHeader) + MinBlockSize) {
        // Split off a piece from the end so we don't have to update the next
        // pointers

        const uint64_t new_current_size = current_block->_size - sizeof(BlockHeader) - requested_size;
        current_block->_size = new_current_size;

        BlockHeader* new_block = reinterpret_cast<BlockHeader*>(reinterpret_cast<char*>(current_block) +
                                                                sizeof(BlockHeader) +
                                                                current_block->_size);

        new_block->_size = requested_size;
        new_block->_next = nullptr;

        return new_block->to_ptr();
      }

      // Move on to next block
      prev_block = current_block;
      current_block = current_block->_next;
    }

    // Could not find a suitable block. Try growing the heap and retry the
    // allocation.
    if (!grow_by_one_page()) {
      return nullptr;
    }
  }
}

void kfree(void* pointer) {
  if (pointer == nullptr) {
    return;
  }

  SpinLockGuard guard(&_kmalloc_lock);

  // Link the freed block back into the free-list. No coaslescing for now to
  // keep it simple.
  BlockHeader* const block = BlockHeader::from_ptr(pointer);
  block->_next = _free_list;
  _free_list = block;
}
