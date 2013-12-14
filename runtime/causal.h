#if !defined(CAUSAL_RUNTIME_CAUSAL_H)
#define CAUSAL_RUNTIME_CAUSAL_H

#include <dlfcn.h>
#include <papi.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>

#include <new>

#include "log.h"

using namespace std;

struct Causal {
private:
	bool _initialized;
  
	Causal() : _initialized(false) {
	  initialize();
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
      
      if(PAPI_library_init(PAPI_VER_CURRENT) != PAPI_VER_CURRENT) {
        FATAL("Failed to initialize PAPI");
      }
      
      initializeThread();
		}
	}
  
  void reinitialize() {
    __atomic_store_n(&_initialized, false, __ATOMIC_SEQ_CST);
    initialize();
  }
	
	void shutdown() {
		if(__atomic_exchange_n(&_initialized, false, __ATOMIC_SEQ_CST) == true) {
			INFO("Shutting down");
		}
	}
  
  void initializeThread() {
    
  }
};

#endif
