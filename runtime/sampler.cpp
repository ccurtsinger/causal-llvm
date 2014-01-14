#include "sampler.h"

#include <atomic>

#include "papi.h"
#include "util.h"

using std::atomic;

enum class SamplerMode {
  Normal,
  Slowdown,
  Speedup
};

/// The current sampler mode
atomic<SamplerMode> mode = ATOMIC_VAR_INIT(SamplerMode::Normal);

/// The range of addresses used for speedup/slowdown
interval perturbed_range;

/// The size of the delay to insert in slowdown/speedup mode
size_t delay_size;

/// The global count of delays to insert
atomic<size_t> delay_count = ATOMIC_VAR_INIT(0);

/// The global count of delays actually executed
atomic<size_t> executed_delay_count = ATOMIC_VAR_INIT(0);

/// The thread local count of delays inserted
__thread size_t local_delay_count = 0;

enum {
  CycleSampleMask = 0x1,
  InstructionSampleMask = 0x2
};

/// Signal handler for PAPI's instruction and cycle sampling
static void overflowHandler(int event_set, void* address, long long vec, void* context) {
  if(vec & CycleSampleMask) {
    // Do nothing for now
  }

  if(vec & InstructionSampleMask) {
    if(mode == SamplerMode::Slowdown) {
      if(perturbed_range.contains((uintptr_t)address)) {
        executed_delay_count++;
        wait(delay_size);
      }
      
    } else if(mode == SamplerMode::Speedup) {
      if(local_delay_count < delay_count) {
        size_t old_local_count = local_delay_count;
        local_delay_count++;
        wait(delay_size);
        executed_delay_count.compare_exchange_strong(old_local_count, local_delay_count); 
      }
      
      if(perturbed_range.contains((uintptr_t)address)) {
        local_delay_count++;
        delay_count++;
      }
    }
  }
}

// The public API
namespace sampler {
  void slowdown(interval r, size_t d) {
    perturbed_range = r;
    delay_size = d;

    // Reset the global delay count
    delay_count.store(0);
    executed_delay_count.store(0);
    
    mode.store(SamplerMode::Slowdown);
  }
  
  void speedup(interval r, size_t d) {
    perturbed_range = r;
    delay_size = d;
    
    // Reset the global delay count
    delay_count.store(0);
    executed_delay_count.store(0);
    
    mode.store(SamplerMode::Speedup);
  }
  
  size_t getRequestedDelays() {
    return delay_count;
  }
  
  size_t getExecutedDelays() {
    return executed_delay_count;
  }

  void initializeThread(size_t cycle_period, size_t inst_period) {
    // This thread is just being created, so it should inherit delays from the source thread
    local_delay_count = executed_delay_count.load();
    
    papi::startThread(cycle_period, inst_period, overflowHandler);
  }

  void shutdownThread() {
    papi::stopThread();
  }
}
