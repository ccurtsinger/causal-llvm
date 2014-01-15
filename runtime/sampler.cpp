#include "sampler.h"

#include <pthread.h>

#include <atomic>
#include <list>
#include <new>

#include "papi.h"

using std::list;

typedef list<SampleBlock*, STLAllocator<SampleBlock*, CausalHeap>> GlobalBlockList;

/// Mutex to protect the global block list
pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
/// Condition variable used to block on the global block list
pthread_cond_t cv = PTHREAD_COND_INITIALIZER;

/// Get a mutable reference to the global block list. Required to ensure proper initialization order.
GlobalBlockList& getGlobalBlocks() {
  static char buf[sizeof(GlobalBlockList)];
  static GlobalBlockList* global_blocks = new(buf) GlobalBlockList();
  return *global_blocks;
}

/// The current sampler mode
atomic<SamplerMode> mode = ATOMIC_VAR_INIT(SamplerMode::Normal);
/// The range of addresses used for speedup/slowdown
interval perturbed_range;
/// The size of the delay to insert in slowdown/speedup mode
size_t delay_size;

/// The round number for delays (speedup/slowdown)
atomic<size_t> delay_round = ATOMIC_VAR_INIT(0);
/// The global count of delays to insert
atomic<size_t> delay_count = ATOMIC_VAR_INIT(0);
/// The global count of delays actually executed
atomic<size_t> executed_delay_count = ATOMIC_VAR_INIT(0);

/// The thread local delay round
__thread size_t local_delay_round = 0;
/// The thread local count of delays inserted
__thread size_t local_delay_count;

/// Magic variable used to ensure the local block pointer is initialized
__thread int local_magic;
/// The thread-local sample block pointer
__thread SampleBlock* local_block;
/// Set to false when sampling should finish up
atomic<bool> active = ATOMIC_VAR_INIT(true);

/// Push the current thread's sample block to the global list
void submitLocalBlock() {
  // Finish the current block (sets the end time)
  local_block->done();
  // Lock the global blocks list
  pthread_mutex_lock(&mtx);
  // Add the local block to the global list
  getGlobalBlocks().push_back(local_block);
  // If the list was previously empty, notify anyone waiting on the list
  if(getGlobalBlocks().size() == 1) pthread_cond_signal(&cv);
  // Unlock the global blocks list
  pthread_mutex_unlock(&mtx);
}

/// Get a usable sample block for the current thread
SampleBlock* getLocalBlock() {
  if(local_magic != 0xD00FCA75 || local_block == NULL) {
    local_magic = 0xD00FCA75;
    local_block = new SampleBlock(mode);
  } else if(local_block->isFull()) {
    submitLocalBlock();
    local_block = new SampleBlock(mode);
  } else if(local_block->getMode() != mode) {
    submitLocalBlock();
    local_block = new SampleBlock(mode);
  }
  
  return local_block;
}

/// Push the current block to the global list if it exists
void flushLocalBlock() {
  if(local_magic == 0xD00FCA75 && local_block != NULL) {
    submitLocalBlock();
    local_block = new SampleBlock(mode);
  }
}

enum {
  CycleSampleMask = 0x1,
  InstructionSampleMask = 0x2
};

/// Signal handler for PAPI's instruction and cycle sampling
static void overflowHandler(int event_set, void* address, long long vec, void* context) {
  if(!active) {
    flushLocalBlock();
    return;
  }
  
  if(vec & CycleSampleMask) {
    getLocalBlock()->add(SampleType::Cycle, (uintptr_t)address);
  }

  if(vec & InstructionSampleMask) {
    getLocalBlock()->add(SampleType::Instruction, (uintptr_t)address);
    
    if(mode.load() == SamplerMode::Slowdown && perturbed_range.contains((uintptr_t)address)) {
      // Reset the local delay count if this is a new round
      if(local_delay_round != delay_round) {
        local_delay_round = delay_round;
        local_delay_count = 0;
      }
      delay_count++;
      local_delay_count++;
      wait(delay_size);
      
    } else if(mode.load() == SamplerMode::Speedup) {
      // Reset the local delay count if this is a new round
      if(local_delay_round != delay_round) {
        local_delay_round = delay_round;
        local_delay_count = 0;
      }
      
      // Wait to catch up to the global delay count
      while(local_delay_count < delay_count.load()) {
        size_t old_local_count = local_delay_count;
        local_delay_count++;
        wait(delay_size);
        // Update the executed delay count if this thread was the straggler
        executed_delay_count.compare_exchange_strong(old_local_count, local_delay_count);
      }
      
      // When we get a sample in the perturbed range, make other threads delay
      if(perturbed_range.contains((uintptr_t)address)) {
        local_delay_count++;
        delay_count++;
      }
    }
  }
}

// The public API
namespace sampler {
  void startSlowdown(interval r, size_t d) {
    perturbed_range = r;
    delay_size = d;
    
    // Advance to the next delay round (causes threads to reset their local counts to zero)
    delay_round++;
    // Reset the global delay count
    delay_count.store(0);
    executed_delay_count.store(0);
    
    mode.store(SamplerMode::Slowdown);
  }
  
  void startSpeedup(interval r, size_t d) {
    perturbed_range = r;
    delay_size = d;
    
    // Advance to the next delay round (causes threads to reset their local counts to zero)
    delay_round++;
    // Reset the global delay count
    delay_count.store(0);
    executed_delay_count.store(0);
    
    mode.store(SamplerMode::Speedup);
  }
  
  size_t reset() {
    mode.store(SamplerMode::Normal);
    return executed_delay_count.load();
  }
  
  SampleBlock* getNextBlock() {
    // Lock the global blocks list
    pthread_mutex_lock(&mtx);
    // Wait while the list is empty
    while(getGlobalBlocks().size() == 0) {
      // When sampling is inactive and there are no global blocks, just return NULL
      if(!active.load()) {
        return NULL;
      }
      pthread_cond_wait(&cv, &mtx);
    }
    // Take the first block off the list
    SampleBlock* result = getGlobalBlocks().front();
    getGlobalBlocks().pop_front();
    pthread_mutex_unlock(&mtx);
    return result;
  }

  void initializeThread(size_t cycle_period, size_t inst_period) {
    // Set the thread-local delay round and counts to match the global executed count
    // This thread is just being created, so it should inherit from the source thread
    local_delay_round = delay_round.load();
    local_delay_count = executed_delay_count.load();
    
    papi::startThread(cycle_period, inst_period, overflowHandler);
  }

  void shutdownThread() {
    papi::stopThread();
    flushLocalBlock();
  }
  
  void finish() {
    active.store(false);
    pthread_cond_signal(&cv);
  }
}
