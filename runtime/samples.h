#if !defined(CAUSAL_RUNTIME_SAMPLES_H)
#define CAUSAL_RUNTIME_SAMPLES_H

#include "heap.h"
#include "util.h"

enum {
  BlockSize = 1024
};

enum class SampleType {
  Cycle,
  Instruction
};

struct Sample {
public:
  SampleType type;
  uintptr_t address;
  inline Sample() {}
  inline Sample(SampleType type, uintptr_t address) : type(type), address(address) {}
};

struct SampleBlock : public PrivateAllocated {
private:
  size_t _start_time;
  size_t _end_time;
  size_t _count = 0;
  Sample _samples[BlockSize];
  
public:
  SampleBlock() : _start_time(getTime()) {}
  
  inline bool isFull() const { return _count >= BlockSize; }
  
  void add(SampleType type, uintptr_t address) {
    _samples[_count] = Sample(type, address);
    _count++;
  }
  
  void done() {
    _end_time = getTime();
  }
  
  wrapped_array<Sample> getSamples() {
    return wrap(_samples, _count);
  }
};

namespace sampler {
  /// Take the oldest global sample chunk
  SampleBlock* getNextBlock();
  /// Start sampling in the current thread
  void initializeThread(size_t cycle_period, size_t inst_period);
  /// Finish sampling in the current thread
  void shutdownThread();
}

#endif
