#if !defined(CAUSAL_RUNTIME_CAUSAL_H)
#define CAUSAL_RUNTIME_CAUSAL_H

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

#include <new>

#include "blockmap.h"
#include "engine.h"
#include "log.h"
#include "real.h"

using namespace std;

enum Time {
  Time_ns = 1,
  Time_us = 1000 * Time_ns,
  Time_ms = 1000 * Time_us,
  Time_s = 1000 * Time_ms
};

struct Causal {
private:
  bool _initialized;
  pthread_t _profiler_thread;
  blockmap _map;
  
	Causal() : _initialized(false) {
    initialize();
	}
  
  static size_t getTime() {
    struct timespec ts;
    if(clock_gettime(CLOCK_REALTIME, &ts)) {
      perror("Host::getTime():");
      abort();
    }
    return ts.tv_nsec + ts.tv_sec * Time_s;
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
    
    while(true) {
      cycle_max = 1024;
      inst_max = 1024;
      engine::collectSamples(cycle_samples, &cycle_max, inst_samples, &inst_max);
      fprintf(stderr, "Sampling complete!\n");
    
      /*fprintf(stderr, "  Cycle samples:\n");
      for(size_t i=0; i<cycle_max; i++) {
        if(_map.inBounds(cycle_samples[i])) {
          fprintf(stderr, "    %p (%s : %p)\n", 
            cycle_samples[i],
            _map.getFile(cycle_samples[i])->getName(),
            _map.getFunction(cycle_samples[i]));
        }
      }
    
      fprintf(stderr, "\n  Instruction samples:\n");
      for(size_t i=0; i<inst_max; i++) {
        if(_map.inBounds(inst_samples[i])) {
          fprintf(stderr, "    %p (%s : %p)\n", 
            inst_samples[i], 
            _map.getFile(inst_samples[i])->getName(),
            _map.getFunction(inst_samples[i]));
        }
      }*/
      
      wait(500 * Time_ms);
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
    
      _map = engine::buildMap();
    
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
