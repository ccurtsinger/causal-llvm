#include "samples.h"

#include <condition_variable>
#include <list>
#include <mutex>

using std::condition_variable;
using std::list;
using std::mutex;
using std::unique_lock;

mutex mtx;
condition_variable cv;
list<SampleChunk*, STLAllocator<SampleChunk*, CausalHeap>> global_chunks;

__thread int local_magic;
__thread SampleChunk* local_chunk;

void submitLocalChunk() {
  // Finalize the current chunk (sets the end time)
  local_chunk->finalize();
  // Lock the global chunks list
  unique_lock<mutex> l(mtx);
  // Add the local chunk to the global list
  global_chunks.push_back(local_chunk);
  // If the list was previously empty, notify anyone waiting on the list
  if(global_chunks.size() == 1) cv.notify_all();
}

SampleChunk* SampleChunk::getLocal() {
  if(local_magic != 0xDEADBEEF || local_chunk == NULL) {
    local_magic = 0xDEADBEEF;
    local_chunk = new SampleChunk();
    
  } else if(local_chunk->isFull()) {
    submitLocalChunk();
    local_chunk = new SampleChunk();
  }
  
  return local_chunk;
}

void SampleChunk::flushLocal() {
  if(local_magic == 0xDEADBEEF && local_chunk != NULL) {
    submitLocalChunk();
    local_chunk = NULL;
  }
}

SampleChunk* SampleChunk::take() {
  // Lock the global chunks list
  unique_lock<mutex> l(mtx);
  // Wait while the list is empty
  while(global_chunks.size() == 0) { cv.wait(l); }
  // Take the first chunk off the list
  SampleChunk* result = global_chunks.front();
  global_chunks.pop_front();
  return result;
}
