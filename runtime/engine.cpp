#include "engine.h"

#include <errno.h>
#include <papi.h>
#include <pthread.h>
#include <stdio.h>

#include <set>
#include <string>

#include "interval.h"
#include "log.h"

using std::set;
using std::string;

namespace engine {
  enum {
    CycleSamplePeriod = 10000000,
    CycleSampleMask = 0x1,
    InstructionSamplePeriod = 10000000,
    InstructionSampleMask = 0x2
  };
  
  enum Mode {
    Idle,
    Sample,
    Slowdown,
    Speedup
  };
  
  pthread_mutex_t papi_mutex = PTHREAD_MUTEX_INITIALIZER;
  __thread int _event_set;
  Mode _mode = Idle;
  
  // Globals for straight-up sampling
  void** _cycle_samples;
  void** _inst_samples;
  size_t* _cycle_index;
  size_t* _inst_index;
  size_t _cycle_max;
  size_t _inst_max;
  
  void collectSamples(void** cycle_samples, size_t* cycle_max, void** inst_samples, size_t* inst_max) {
    _cycle_samples = cycle_samples;
    _inst_samples = inst_samples;
    
    _cycle_max = *cycle_max;
    _cycle_index = cycle_max;
    *_cycle_index = 0;
    
    _inst_max = *inst_max;
    _inst_index = inst_max;
    *_inst_index = 0;
    
    __atomic_store_n(&_mode, Sample, __ATOMIC_SEQ_CST);
    
    while(__atomic_load_n(&_mode, __ATOMIC_SEQ_CST) == Sample) {
      __asm__("pause");
    }
  }
  
  void overflowHandler(int event_set, void* address, long long vec, void* context) {
    Mode m = __atomic_load_n(&_mode, __ATOMIC_SEQ_CST);
    
    if(m == Idle) {
      // Do nothing
      
    } else if(m == Sample) {
      if(vec & CycleSampleMask) {
        size_t index = __atomic_fetch_add(_cycle_index, 1, __ATOMIC_SEQ_CST);
        if(index < _cycle_max) {
          _cycle_samples[index] = address;
        } else if(index == _cycle_max) {
          __atomic_store_n(&_mode, Idle, __ATOMIC_SEQ_CST);
        }
      }
  
      if(vec & InstructionSampleMask) {
        size_t index = __atomic_fetch_add(_inst_index, 1, __ATOMIC_SEQ_CST);
        if(index < _inst_max) {
          _inst_samples[index] = address;
        } else if(index == _cycle_max) {
          __atomic_store_n(&_mode, Idle, __ATOMIC_SEQ_CST);
        }
      }
    }
  }

  void initialize() {
    // Initialize the PAPI library
    REQUIRE(PAPI_library_init(PAPI_VER_CURRENT) == PAPI_VER_CURRENT, "Failed to initialize PAPI");
    REQUIRE(PAPI_thread_init(pthread_self) == PAPI_OK, "Failed initialize PAPI thread support");
    //REQUIRE(PAPI_set_domain(PAPI_DOM_ALL) == PAPI_OK, "Failed to set PAPI domain");
  
    // Verify that the instruction and cycle events are available
    REQUIRE(PAPI_query_event(PAPI_TOT_CYC) == PAPI_OK, "Hardware cycle counter is not available");
    REQUIRE(PAPI_query_event(PAPI_TOT_INS) == PAPI_OK, "Hardware instruction counter is not available");
      
    INFO("PAPI Initialized");
  }
  
  void addThread() {
    // Set up the PAPI event set
    _event_set = PAPI_NULL;
    REQUIRE(PAPI_create_eventset(&_event_set) == PAPI_OK, "Failed to create PAPI event set");
  
    // Add cycle and instruction counting events
    REQUIRE(PAPI_add_event(_event_set, PAPI_TOT_CYC) == PAPI_OK, "Failed to add cycle counter event");
    REQUIRE(PAPI_add_event(_event_set, PAPI_TOT_INS) == PAPI_OK, "Failed to add instruction counter event");
  
    // Set up sampling (overflow signals) for the cycle counter
    REQUIRE(PAPI_overflow(_event_set, PAPI_TOT_CYC, CycleSamplePeriod, 0, overflowHandler) == PAPI_OK, 
      "Failed to set up cycle counter sampling");
    
    // Set up sampling (overflow signals) for the instruction counter
    REQUIRE(PAPI_overflow(_event_set, PAPI_TOT_INS, InstructionSamplePeriod, 0, overflowHandler) == PAPI_OK,
      "Failed to set up instruction counter sampling");
    
    PAPI_start(_event_set);
  }
  
  void removeThread() {
    long long result[2];
    REQUIRE(PAPI_stop(_event_set, result) == PAPI_OK, "Failed to stop PAPI");
    REQUIRE(PAPI_cleanup_eventset(_event_set) == PAPI_OK, "Failed to clean up event set");
    REQUIRE(PAPI_destroy_eventset(&_event_set) == PAPI_OK, "Failed to destroy event set");
    REQUIRE(PAPI_unregister_thread() == PAPI_OK, "Failed to unregister thread");
  }
  
  set<interval> _buildCodeMap() {
    set<interval> code;
    
  	const PAPI_exe_info_t* info = PAPI_get_executable_info();
  	if(info) {
      code.insert(interval(info->address_info.text_start, info->address_info.text_end));
  	}
    
    /*const PAPI_shlib_info_t* lib_info = PAPI_get_shared_lib_info();
    if(lib_info) {
      for(int i=0; i<lib_info->count; i++) {
        // Don't include PAPI or causal
        if(string(lib_info->map[i].name).find("libpapi") == string::npos &&
           string(lib_info->map[i].name).find("libcausal") == string::npos) {
          code.insert(interval(lib_info->map[i].text_start, lib_info->map[i].text_end));
        }
      }
    }*/
    
    return code;
  }
  
  bool inRange(void* p) {
    static set<interval> code = _buildCodeMap();
    return code.find(p) != code.end();
  }
}
