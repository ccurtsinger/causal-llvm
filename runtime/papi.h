#if !defined(CAUSAL_RUNTIME_PAPI_H)
#define CAUSAL_RUNTIME_PAPI_H

#include <papi.h>

#include "log.h"

namespace papi {
  typedef void (*overflow_handler_t)(int, void*, long long, void*);
  __thread int _event_set;
  
  void initialize() {
    int rc;
    
    // Initialize the PAPI library
    rc = PAPI_library_init(PAPI_VER_CURRENT);
    REQUIRE(rc == PAPI_VER_CURRENT, "Failed to initialize PAPI: %s", PAPI_strerror(rc));
    
    // Tell PAPI to use pthread_self to identify threads
    rc = PAPI_thread_init(pthread_self);
    REQUIRE(rc == PAPI_OK, "Failed initialize PAPI thread support: %s", PAPI_strerror(rc));
    
    // Enable counters at kernel or hypervisor domain:
    // rc = PAPI_set_domain(PAPI_DOM_ALL);
    // REQUIRE(rc == PAPI_OK, "Failed to set PAPI domain: %s", PAPI_strerror(rc));
  
    // Verify that the instruction and cycle events are available
    rc = PAPI_query_event(PAPI_TOT_CYC);
    REQUIRE(rc == PAPI_OK, "Hardware cycle counter is not available: %s", PAPI_strerror(rc));
    rc = PAPI_query_event(PAPI_TOT_INS);
    REQUIRE(rc == PAPI_OK, "Hardware instruction counter is not available: %s", PAPI_strerror(rc));
      
    INFO("PAPI Initialized");
  }
  
  void startThread(size_t cycle_period, size_t inst_period, overflow_handler_t handler) {
    static atomic<size_t> total_startup = ATOMIC_VAR_INIT(0);
    size_t start = getTime();
    int rc;
    // Set up the PAPI event set
    _event_set = PAPI_NULL;
    rc = PAPI_create_eventset(&_event_set);
    REQUIRE(rc == PAPI_OK, "Failed to create PAPI event set: %s", PAPI_strerror(rc));
  
    // Add cycle and instruction counting events
    rc = PAPI_add_event(_event_set, PAPI_TOT_CYC);
    REQUIRE(rc == PAPI_OK, "Failed to add cycle counter event: %s", PAPI_strerror(rc));
    rc = PAPI_add_event(_event_set, PAPI_TOT_INS);
    REQUIRE(rc == PAPI_OK, "Failed to add instruction counter event: %s", PAPI_strerror(rc));
  
    // Set up sampling (overflow signals) for the cycle counter
    rc = PAPI_overflow(_event_set, PAPI_TOT_CYC, cycle_period, 0, handler);
    REQUIRE(rc == PAPI_OK, "Failed to set up cycle counter sampling: %s", PAPI_strerror(rc));
    
    // Set up sampling (overflow signals) for the instruction counter
    rc = PAPI_overflow(_event_set, PAPI_TOT_INS, inst_period, 0, handler);
    REQUIRE(rc == PAPI_OK, "Failed to set up instruction counter sampling: %s", PAPI_strerror(rc));
    
    PAPI_start(_event_set);
    total_startup += getTime() - start;
    INFO("Total thread startup time: %fs", (float)total_startup.load() / Time_s);
  }
  
  void stopThread() {
    static atomic<size_t> total_shutdown = ATOMIC_VAR_INIT(0);
    size_t start = getTime();
    long long result[2];
    REQUIRE(PAPI_stop(_event_set, result) == PAPI_OK, "Failed to stop PAPI");
    REQUIRE(PAPI_cleanup_eventset(_event_set) == PAPI_OK, "Failed to clean up event set");
    REQUIRE(PAPI_destroy_eventset(&_event_set) == PAPI_OK, "Failed to destroy event set");
    REQUIRE(PAPI_unregister_thread() == PAPI_OK, "Failed to unregister thread");
    total_shutdown += getTime() - start;
    INFO("Total thread shutdown time: %fs", (float)total_shutdown.load() / Time_s);
  }
};

#endif
