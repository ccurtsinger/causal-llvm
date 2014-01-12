#if !defined(CAUSAL_RUNTIME_CAUSAL_H)
#define CAUSAL_RUNTIME_CAUSAL_H

#include <cstdint>
#include <cstdio>
#include <new>
#include <thread>

#include "basicblock.h"
#include "engine.h"
#include "function.h"
#include "log.h"
#include "real.h"
#include "samples.h"
#include "util.h"

struct Causal {
private:
  bool _initialized;
  pthread_t _profiler_thread;
  
	Causal() : _initialized(false) {
    initialize();
	}
  
  void profiler() {
    while(true) {
      SampleChunk* chunk = SampleChunk::take();
      
      for(Sample& s : chunk->getSamples()) {
        Function& f = engine::getFunction(s.address);
        BasicBlock& b = f.getBlock(s.address);
        if(s.type == SampleType::Cycle)
          b.cycleSample();
        else
          b.instructionSample();
      }
    }
  }
  
  static void* startProfiler(void* arg) {
    getInstance().profiler();
    return NULL;
  }
  
public:
	static Causal& getInstance() {
		static char buf[sizeof(Causal)];
		static Causal* instance = new(buf) Causal();
		return *instance;
	}
  
  void initialize() {
    if(__atomic_exchange_n(&_initialized, true, __ATOMIC_SEQ_CST) == false) {
      INFO("Initializing");
      engine::initialize();
    
      // Create the profiler thread
      REQUIRE(Real::pthread_create()(&_profiler_thread, NULL, startProfiler, NULL) == 0,
        "Failed to create profiler thread");
    
      // Add the main thread to the sampling engine
      engine::addThread();
    }
  }
  
  void reinitialize() {
    INFO("Reinitializing");
    __atomic_store_n(&_initialized, false, __ATOMIC_SEQ_CST);
    engine::initialize();
  }
  
  void shutdown() {
    if(__atomic_exchange_n(&_initialized, false, __ATOMIC_SEQ_CST) == true) {
      INFO("Shutting down");
    }
  }
  
  void initializeThread() {
    engine::addThread();
  }
};

#endif
