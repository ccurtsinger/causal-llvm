#if !defined(CAUSAL_RUNTIME_CAUSAL_H)
#define CAUSAL_RUNTIME_CAUSAL_H

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <sstream>
#include <vector>

#include "counter.h"
#include "log.h"
#include "papi.h"
#include "real.h"
#include "sampler.h"
#include "util.h"

enum {
  CycleSamplePeriod = 10000000,
  InstructionSamplePeriod = 500011
};

class Causal {
private:
  std::atomic<bool> _initialized = ATOMIC_VAR_INIT(false);
  size_t _start_time;
  std::vector<Counter*> _progress_counters;
  
	Causal() {
    initialize();
	}
  
  void printCounters() {
    size_t i = 0;
    for(Counter* c : _progress_counters) {
      fprintf(stderr, "%lu:%lu\n", i, c->getValue());
      i++;
    }
  }
  
public:
	static Causal& getInstance() {
		static char buf[sizeof(Causal)];
		static Causal* instance = new(buf) Causal();
		return *instance;
	}
  
  void initialize() {
    if(_initialized.exchange(true) == false) {
      INFO("Initializing");
      
      char* mode_c = getenv("CAUSAL_MODE");
      REQUIRE(mode_c != NULL, "CAUSAL_MODE is not set");
      std::istringstream s(mode_c);
      
      char speedup_c;
      s.get(speedup_c);
      
      bool speedup;
      if(speedup_c == '+') {
        speedup = true;
      } else if(speedup_c == '-') {
        speedup = false;
      } else {
        FATAL("Mode must specify speedup (+) or slowdown (-)");
      }
      
      // skip up one char
      REQUIRE(s.get() == ' ', "Expected ' '");
      
      uintptr_t base, limit;
      REQUIRE(s.get() == '0', "Expected '0'");
      REQUIRE(s.get() == 'x', "Expected 'x'");
      s >> std::hex >> base;
      REQUIRE(s.get() == '-', "Expected '-'");
      
      REQUIRE(s.get() == '0', "Expected '0'");
      REQUIRE(s.get() == 'x', "Expected 'x'");
      s >> limit;
      
      REQUIRE(s.get() == ' ', "Expected ' '");
      
      size_t delay;
      s >> std::dec >> delay;
      
      INFO("Running in %s mode for %p-%p with a delay of %lums",
        speedup ? "speedup" : "slowdown",
        (void*)base,
        (void*)limit,
        delay);
      
      // Set up PAPI
      papi::initialize();
      
      if(speedup)
        sampler::speedup(interval(base, limit), delay);
      else
        sampler::slowdown(interval(base, limit), delay);
      
      _start_time = getTime();
        
      // Initialize the main thread
      initializeThread();
    }
  }
  
  void addProgressCounter(Counter* c) {
    _progress_counters.push_back(c);
  }
  
  void addBeginCounter(Counter* c) {
    WARNING("Transaction counters are not supported yet");
  }
  
  void addEndCounter(Counter* c) {
    WARNING("Transaction counters are not supported yet");
  }
  
  void initializeThread() {
    sampler::initializeThread(CycleSamplePeriod, InstructionSamplePeriod);
  }
  
  void shutdownThread() {
    sampler::shutdownThread();
  }
  
  void reinitialize() {
    INFO("Reinitializing");
    _initialized.store(false);
    initialize();
  }
  
  void shutdown() {
    if(_initialized.exchange(false) == true) {
      INFO("Shutting down");
      
      size_t elapsed = getTime() - _start_time;
      fprintf(stderr, "%fms elapsed\n", (float)elapsed / Time_ms);
    }
  }
};

#endif
