#if !defined(CAUSAL_RUNTIME_CAUSAL_H)
#define CAUSAL_RUNTIME_CAUSAL_H

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>

#include <new>

#include "engine.h"
#include "function.h"
#include "log.h"
#include "real.h"
#include "util.h"

struct Causal {
private:
  bool _initialized;
  pthread_t _profiler_thread;
  
	Causal() : _initialized(false) {
    initialize();
	}
  
  static size_t wait(uint64_t nanos) {
    if(nanos == 0) return 0;
    size_t start_time = getTime();
    struct timespec ts;
    ts.tv_nsec = nanos % Time_s;
    ts.tv_sec = (nanos - ts.tv_nsec) / Time_s;
    while(clock_nanosleep(CLOCK_REALTIME, 0, &ts, &ts)) {}
    return getTime() - start_time;
  }
  
  void profiler() {
    void* cycle_samples[1024];
    size_t cycle_max;
    void* inst_samples[1024];
    size_t inst_max;
    
    size_t total_samples = 0;
    
    while(true) {
      /*cycle_max = 1024;
      inst_max = 1024;
      engine::collectSamples(cycle_samples, &cycle_max, inst_samples, &inst_max);
      
      fprintf(stderr, "  Instruction samples:\n");
      for(size_t i=0; i<inst_max; i++) {
        function* f = engine::getFunction(inst_samples[i]);
        if(f != NULL) {
          int block = f->getBlock(inst_samples[i]);
          fprintf(stderr, "  %s block %d (%p)\n", f->getName().c_str(), block, inst_samples[i]);
        }
      }
      
      total_samples += inst_max;
      
      fprintf(stderr, "%lu total samples\n", total_samples);*/
      
      wait(Time_s);
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
      //REQUIRE(Real::pthread_create()(&_profiler_thread, NULL, startProfiler, NULL) == 0,
      //  "Failed to create profiler thread");
    
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
