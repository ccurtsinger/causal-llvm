#include "sampler.h"

#include <pthread.h>
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

/// Magic variable used to ensure the local block pointer is initialized
__thread int local_magic;
/// The thread-local sample block pointer
__thread SampleBlock* local_block;

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
    local_block = new SampleBlock();
  } else if(local_block->isFull()) {
    submitLocalBlock();
    local_block = new SampleBlock();
  }
  return local_block;
}

/// Push the current block to the global list if it exists
void flushLocalBlock() {
  if(local_magic == 0xD00FCA75 && local_block != NULL) {
    submitLocalBlock();
    local_block = new SampleBlock();
  }
}

enum {
  CycleSampleMask = 0x1,
  InstructionSampleMask = 0x2
};

/// Signal handler for PAPI's instruction and cycle sampling
static void overflowHandler(int event_set, void* address, long long vec, void* context) {
  if(vec & CycleSampleMask) {
    getLocalBlock()->add(SampleType::Cycle, (uintptr_t)address);
  }

  if(vec & InstructionSampleMask) {
    getLocalBlock()->add(SampleType::Instruction, (uintptr_t)address);
  }
}

// The public API
namespace sampler {
  SampleBlock* getNextBlock() {
    // Lock the global blocks list
    pthread_mutex_lock(&mtx);
    // Wait while the list is empty
    while(getGlobalBlocks().size() == 0) {
      pthread_cond_wait(&cv, &mtx);
    }
    // Take the first block off the list
    SampleBlock* result = getGlobalBlocks().front();
    getGlobalBlocks().pop_front();
    pthread_mutex_unlock(&mtx);
    return result;
  }

  void initializeThread(size_t cycle_period, size_t inst_period) {
    papi::startThread(cycle_period, inst_period, overflowHandler);
  }

  void shutdownThread() {
    papi::stopThread();
    flushLocalBlock();
  }
}
