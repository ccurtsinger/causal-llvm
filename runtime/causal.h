#if !defined(CAUSAL_RUNTIME_CAUSAL_H)
#define CAUSAL_RUNTIME_CAUSAL_H

#include <cstdint>
#include <cstdio>
#include <map>
#include <new>
#include <thread>

#include "basicblock.h"
#include "elf.h"
#include "function.h"
#include "log.h"
#include "papi.h"
#include "real.h"
#include "sampler.h"
#include "util.h"

enum {
  CycleSamplePeriod = 10000000,
  InstructionSamplePeriod = 5000000
};

class Causal {
private:
  bool _initialized;
  pthread_t _profiler_thread;
  map<interval, Function> _functions;
  
	Causal() : _initialized(false) {
    initialize();
	}
  
  Function& getFunction(uintptr_t p) {
    map<interval, Function>::iterator iter = _functions.find(p);
    if(iter == _functions.end())
      return Function::getNullFunction();
    else
      return iter->second;
  }
  
  void profiler() {
    while(true) {
      SampleBlock* block = sampler::getNextBlock();
      
      for(Sample& s : block->getSamples()) {
        Function& f = getFunction(s.address);
        BasicBlock& b = f.getBlock(s.address);
        
        if(s.type == SampleType::Cycle)
          b.cycleSample();
        else
          b.instructionSample();
      }
      
      delete block;
    }
  }
  
  static void* startProfiler(void* arg) {
    getInstance().profiler();
    return NULL;
  }
  
  void findFunctions() {
    for(const auto& file : papi::getFiles()) {
      const string& filename = file.first;
      const interval& file_range = file.second;
      
      // Skip libpapi and libcausal
      if(filename.find("libcausal") != string::npos ||
         filename.find("libpapi") != string::npos) {
        continue;
      }
      
      ELFFile* elf = ELFFile::open(filename);
      
      if(elf == NULL) {
        WARNING("Skipping file %s", filename.c_str());
      } else {
        for(const auto& fn : elf->getFunctions()) {
          const string& fn_name = fn.first;
          interval fn_range = fn.second;
          
          // Dynamic libraries need to be shifted to their load address
          if(elf->isDynamic())
            fn_range += file_range.getBase();
          
          _functions.emplace(fn_range, Function(fn_name, fn_range));
        }
        
        delete elf;
      }
    }
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
      
      // Set up PAPI
      papi::initialize();
      
      // Build a map of functions
      findFunctions();
    
      // Create the profiler thread
      REQUIRE(Real::pthread_create()(&_profiler_thread, NULL, startProfiler, NULL) == 0,
        "Failed to create profiler thread");
        
      // Initialize the main thread
      initializeThread();
    }
  }
  
  void reinitialize() {
    INFO("Reinitializing");
    __atomic_store_n(&_initialized, false, __ATOMIC_SEQ_CST);
  }
  
  void shutdown() {
    if(__atomic_exchange_n(&_initialized, false, __ATOMIC_SEQ_CST) == true) {
      INFO("Shutting down");
    }
  }
  
  void initializeThread() {
    sampler::initializeThread(CycleSamplePeriod, InstructionSamplePeriod);
  }
  
  void shutdownThread() {
    sampler::shutdownThread();
  }
};

#endif
