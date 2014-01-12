#if !defined(CAUSAL_RUNTIME_PAPI_H)
#define CAUSAL_RUNTIME_PAPI_H

#include <papi.h>

#include <map>
#include <string>

#include "interval.h"

namespace papi {
  typedef void (*overflow_handler_t)(int, void*, long long, void*);
  
  /// Initialize the PAPI library
  void initialize();
  
  /// Start PAPI sampling in the current thread
  void startThread(size_t cycle_period, size_t inst_period, overflow_handler_t handler);
  
  /// Stop PAPI sampling in the current thread
  void stopThread();
  
  /// Get loaded executable files identified by PAPI
  std::map<std::string, interval> getFiles();
}

#endif
