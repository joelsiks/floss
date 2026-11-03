#ifndef INCLUDE_MEMORY_FRAMEALLOCATOR
#define INCLUDE_MEMORY_FRAMEALLOCATOR

#include <cstdint>

namespace Memory {
  struct Frame {
    uint64_t _address;
  };

  namespace FrameAllocator {
    void initialize();

    bool allocate_frame(Frame* frame);
    bool allocate_frames_contiguous(uint64_t count, Frame* frame);

    void free_frame(Frame* frame);
  };

};

#endif // INCLUDE_MEMORY_FRAMEALLOCATOR
