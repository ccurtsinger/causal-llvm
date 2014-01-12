#if !defined(CAUSAL_RUNTIME_SAMPLES_H)
#define CAUSAL_RUNTIME_SAMPLES_H

#include "heap.h"
#include "util.h"

enum {
  ChunkSize = 1024
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

struct SampleChunk : public PrivateAllocated {
private:
  size_t _start_time;
  size_t _end_time;
  size_t _count = 0;
  Sample _samples[ChunkSize];
  
public:
  SampleChunk() : _start_time(getTime()) {}
  
  bool isFull() {
    return _count >= ChunkSize;
  }
  
  void add(SampleType type, uintptr_t address) {
    _samples[_count] = Sample(type, address);
    _count++;
  }
  
  void finalize() {
    _end_time = getTime();
  }
  
  wrapped_array<Sample> getSamples() {
    return wrap(_samples, _count);
  }
  
  /// Get the current thread-local sample chunk
  static SampleChunk* getLocal();
  /// Flush the current thread-local sample chunk to the global list
  static void flushLocal();
  /// Take the oldest global sample chunk
  static SampleChunk* take();
};

#endif
