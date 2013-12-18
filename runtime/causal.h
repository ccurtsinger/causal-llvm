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

enum {
  CycleSamplePeriod = 200000,
  CycleSampleMask = 0x1,
  InstructionSamplePeriod = 200000,
  InstructionSampleMask = 0x2
};

struct Causal {
private:
	bool _initialized;
  int _event_set;
  
	Causal() : _initialized(false) {
	  initialize();
	}
  
  void printExecInfo() {
  	const PAPI_exe_info_t* exec_info = PAPI_get_executable_info();
  	if(exec_info) {
      INFO("Profiling %s (%s)\n  text: %p - %p\n  data: %p - %p\n  bss: %p - %p", 
        exec_info->address_info.name, exec_info->fullname, 
        exec_info->address_info.text_start, exec_info->address_info.text_end,
        exec_info->address_info.data_start, exec_info->address_info.data_end,
        exec_info->address_info.bss_start, exec_info->address_info.bss_end);
  	}
    
    const PAPI_shlib_info_t* shlib_info = PAPI_get_shared_lib_info();
    if(shlib_info) {
      INFO(" %d shared libraries", shlib_info->count);
      for(int i=0; i<shlib_info->count; i++) {
        INFO("  %d: %s\n    text: %p - %p\n    data: %p - %p\n    bss: %p - %p\n",
          i, shlib_info->map[i].name,
          shlib_info->map[i].text_start, shlib_info->map[i].text_end,
          shlib_info->map[i].data_start, shlib_info->map[i].data_end,
          shlib_info->map[i].bss_start, shlib_info->map[i].bss_end);
      }
    }
  }
  
  static void sample(int event_set, void* address, long long vec, void* context) {
    if(vec & CycleSampleMask) {
      fprintf(stderr, "Cycle sample at %p\n", address);
    }
    
    if(vec & InstructionSampleMask) {
      fprintf(stderr, "Instruction sample at %p\n", address); 
    }
    
    if((vec & CycleSampleMask) && (vec & InstructionSampleMask)) {
      fprintf(stderr, "BOTH!\n");
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
      
      // Initialize the PAPI library
      REQUIRE(PAPI_library_init(PAPI_VER_CURRENT) == PAPI_VER_CURRENT, "Failed to initialize PAPI");
      REQUIRE(PAPI_thread_init(pthread_self) == PAPI_OK, "Failed initialize PAPI thread support");
      REQUIRE(PAPI_set_domain(PAPI_DOM_ALL) == PAPI_OK, "Failed to set PAPI domain");
      
      // Verify that the instruction and cycle events are available
      REQUIRE(PAPI_query_event(PAPI_TOT_CYC) == PAPI_OK, "Hardware cycle counter is not available");
      REQUIRE(PAPI_query_event(PAPI_TOT_INS) == PAPI_OK, "Hardware instruction counter is not available");
      
      // Print some executable info
    	printExecInfo();
      
      // Set up the PAPI event set
      _event_set = PAPI_NULL;
      REQUIRE(PAPI_create_eventset(&_event_set) == PAPI_OK, "Failed to create PAPI event set");
      
      // Add cycle and instruction counting events
      REQUIRE(PAPI_add_event(_event_set, PAPI_TOT_CYC) == PAPI_OK, "Failed to add cycle counter event");
      REQUIRE(PAPI_add_event(_event_set, PAPI_TOT_INS) == PAPI_OK, "Failed to add instruction counter event");
      
      // Set up sampling (overflow signals) for the cycle counter
      REQUIRE(PAPI_overflow(_event_set, PAPI_TOT_CYC, CycleSamplePeriod, 0, sample) == PAPI_OK,
        "Failed to set up cycle counter sampling:");
    
      // Set up sampling (overflow signals) for the instruction counter
      REQUIRE(PAPI_overflow(_event_set, PAPI_TOT_INS, InstructionSamplePeriod, 0, sample) == PAPI_OK,
        "Failed to set up instruction counter sampling");
      
      // Initialize performance counters on the main thread
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
    // Start sampling
    PAPI_start(_event_set);
  }
};

#endif
