
#include "memory/FrameAllocator.h"

#include "memory/map.h"
#include "util/align.h"
#include "util/assert.h"
#include "util/bits.h"

static const uint64_t BitsPerWord = 64;

static const uint64_t MaxRamSize = 8ULL * 1024 * 1024 * 1024; // 8GB
static const uint64_t FrameSize = 4 * 1024; // 4KB
static const uint64_t NumFrames = MaxRamSize / FrameSize;
static const uint64_t BitmapSize = NumFrames / BitsPerWord;

// 0 means free, 1 means allocated
static uint64_t _frame_bitmap[BitmapSize];

struct BitRef {
  uint64_t* _word;
  uint64_t  _bit;
};

struct RamRange {
  uint64_t  _base;
  uint64_t  _num_frames;
  BitRef    _bitmap_start;
};

static const uint64_t MaxRamRanges = 8;
static RamRange _ram_ranges[MaxRamRanges];
static uint64_t _num_ram_ranges = 0;

// A RAM range does not necessarily contain a whole number of bitmap words, so
// the next range may start in the middle of a word.
static BitRef bit_ref_at(BitRef ref, uint64_t offset) {
  const uint64_t bit = ref._bit + offset;
  ref._word += bit / BitsPerWord;
  ref._bit = bit % BitsPerWord;
  return ref;
}

static BitRef paddr_to_frame(uint64_t paddr) {
  for (uint64_t i = 0; i < _num_ram_ranges; i++) {
    RamRange* range = &_ram_ranges[i];

    if (paddr >= range->_base &&
        paddr < (range->_base + range->_num_frames * FrameSize)) {
      const uint64_t frame_offset = (paddr - range->_base) / FrameSize;
      return bit_ref_at(range->_bitmap_start, frame_offset);
    }
  }

  kassert(false, "Address %x does not fall within any RAM range\n", paddr);
  kShouldNotReachHere();
}

static void mark_allocated(BitRef bit_ref, uint64_t num_frames) {
  uint64_t frames_left = num_frames;
  while (frames_left != 0) {
    const uint64_t frames_in_word = BitsPerWord - bit_ref._bit;
    const uint64_t frames_to_mark = frames_left < frames_in_word
        ? frames_left
        : frames_in_word;

    // Avoid shifting by 64 when the remainder fills the whole word
    const uint64_t mask = frames_to_mark == BitsPerWord
        ? ~0ULL
        : ((1ULL << frames_to_mark) - 1) << bit_ref._bit;

    *bit_ref._word |= mask;

    bit_ref = bit_ref_at(bit_ref, frames_to_mark);
    frames_left -= frames_to_mark;
  }
}

void Memory::FrameAllocator::initialize() {
  // To support sparse RAM representation, we represent each RAM region as a
  // range. That range has a base address, a number of frames that can be
  // allocated from it, and a bit reference into the bitmap where it starts.
  //
  // With this information, we can start searching in the bitmap and then
  // translate back to a physical address that we can hand back to the caller
  // when allocating.
  //
  // For example, the QEMU's Raspberry Pi 4 has the following RAM regions (2GB),
  // leaving a 64MB hole between the first and second gigabyte, for VideoCore
  // - [0x40000000, 0x80000000)
  // - [0x0, 0x3c000000)


  Memory::RegionList* ram_regions = Memory::ram_regions();
  kprecond(ram_regions->_num_regions <= MaxRamRanges);

  // Start by creating a "mirror" of the RAM regions, which we refer to as
  // "RAM ranges". These ranges contain where the region starts, how many
  // frames it holds, and where its bits start in the bitmap.
  BitRef current_bitmap = {
    ._word = &_frame_bitmap[0],
    ._bit = 0
  };

  uint64_t bitmap_frames = 0;

  for (uint64_t i = 0; i < ram_regions->_num_regions; i++) {
    Memory::Region* region = &ram_regions->_regions[i];

    // Only complete frames can be returned by the allocator. In particular,
    // do not hand out a frame which starts before or ends after a RAM region.
    const uint64_t base = align_up(region->_start, FrameSize);
    const uint64_t end = align_down(region->_start + region->_size, FrameSize);
    const uint64_t num_frames = end > base ? (end - base) / FrameSize : 0;

    if (num_frames == 0) {
      continue;
    }

    kprecond(num_frames <= NumFrames - bitmap_frames);
    _ram_ranges[_num_ram_ranges] = {
      ._base = base,
      ._num_frames = num_frames,
      ._bitmap_start = current_bitmap
    };

    _num_ram_ranges++;
    bitmap_frames += num_frames;
    current_bitmap = bit_ref_at(current_bitmap, num_frames);
  }

  // Mark all reserved regions as already allocated, so that frames from those
  // regions are not handed out in new allocations. A reserved region can
  // extend outside RAM, so mark only its intersection with each RAM range.
  Memory::RegionList* reserved_regions = Memory::reserved_regions();
  for (uint64_t i = 0; i < reserved_regions->_num_regions; i++) {
    Memory::Region* region = &reserved_regions->_regions[i];

    const uint64_t reserved_start = align_down(region->_start, FrameSize);
    const uint64_t reserved_end = align_up(region->_start + region->_size, FrameSize);

    for (uint64_t j = 0; j < _num_ram_ranges; j++) {
      RamRange* range = &_ram_ranges[j];

      const uint64_t range_end = range->_base + range->_num_frames * FrameSize;
      const uint64_t start = reserved_start > range->_base
          ? reserved_start
          : range->_base;

      const uint64_t end = reserved_end < range_end ? reserved_end : range_end;

      if (start < end) {
        const uint64_t num_frames = (end - start) / FrameSize;
        mark_allocated(paddr_to_frame(start), num_frames);
      }
    }
  }

  // All RAM ranges are now set up and the bitmap is populated with the reserved
  // regions as initially allocated
}

bool Memory::FrameAllocator::allocate_frame(Memory::Frame* frame) {
  // Initial implementation uses a first-fit algorithm, searching linearly from
  // low addresses until an available frame is found

  for (uint64_t i = 0; i < _num_ram_ranges; i++) {
    RamRange* const range = &_ram_ranges[i];
    BitRef bit_ref = range->_bitmap_start;
    uint64_t frames_left = range->_num_frames;
    uint64_t frame_offset = 0;

    while (frames_left != 0) {
      const uint64_t frames_in_word = BitsPerWord - bit_ref._bit;
      const uint64_t frames_to_check =
        frames_left < frames_in_word ? frames_left : frames_in_word;

      // bits_clz returns the position of the first zero bit, counting from
      // the least significant bit. Shift away bits belonging to earlier
      // ranges/frames; the zero fill above the shifted word naturally keeps
      // bits outside this range from being selected.
      const uint64_t bitpos = bits_clz(*bit_ref._word >> bit_ref._bit);
      if (bitpos < frames_to_check) {
        const uint64_t mask = 1ULL << (bit_ref._bit + bitpos);

        // Match found. Update the bitmap to mark the frame as allocated.
        *bit_ref._word |= mask;
        *frame = { range->_base + (frame_offset + bitpos) * FrameSize };
        return true;
      }

      bit_ref = bit_ref_at(bit_ref, frames_to_check);
      frame_offset += frames_to_check;
      frames_left -= frames_to_check;
    }
  }

  return false;
}

bool Memory::FrameAllocator::allocate_frames_contiguous(uint64_t count, Memory::Frame* frame) {
  (void)count;
  (void)frame;
  kassert(false, "Not allocated yet");
  return false;
}

void Memory::FrameAllocator::free_frame(Memory::Frame* frame) {
  kprecond(frame != nullptr);
  kassert(frame->_address % FrameSize == 0, "Frame ptr %p is not aligned to FrameSize %d\n", frame->_address, FrameSize);

  BitRef bit_ref = paddr_to_frame(frame->_address);
  const uint64_t mask = 1ULL << bit_ref._bit;

  // A frame must be allocated before it can be returned to the allocator
  kprecond((*bit_ref._word & mask) != 0);

  *bit_ref._word &= ~mask;
}

