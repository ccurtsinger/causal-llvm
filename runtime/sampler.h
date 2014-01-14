#if !defined(CAUSAL_RUNTIME_SAMPLES_H)
#define CAUSAL_RUNTIME_SAMPLES_H

#include "heap.h"
#include "interval.h"
#include "util.h"

enum {
  BlockSize = 1024
};

enum class SampleType {
  Cycle,
  Instruction
};

enum class SamplerMode {
  Normal,
  Slowdown,
  Speedup
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
  SamplerMode _mode;
  size_t _start_time;
  size_t _end_time;
  size_t _count = 0;
  Sample _samples[BlockSize];
  
public:
  SampleBlock(SamplerMode mode) : _mode(mode), _start_time(getTime()) {}
  
  inline SamplerMode getMode() const { return _mode; }
  inline bool isFull() const { return _count >= BlockSize; }
  inline size_t getCount() const { return _count; }
  
  void add(SampleType type, uintptr_t address) {
    _samples[_count] = Sample(type, address);
    _count++;
  }
  
  Sample& get(size_t index) {
    return _samples[index];
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
  /// Start slowdown mode in an address range
  void startSlowdown(interval range, size_t delay_size);
  /// Start speedup mode in an address range
  void startSpeedup(interval range, size_t delay_size);
  /// Return to normal sampling mode. Returns the total number of delays inserted.
  size_t reset();
}

#endif
