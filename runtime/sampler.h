#if !defined(CAUSAL_RUNTIME_SAMPLES_H)
#define CAUSAL_RUNTIME_SAMPLES_H

#include "interval.h"

namespace sampler {
  void slowdown(interval r, size_t delay);
  void speedup(interval r, size_t delay);
  size_t getRequestedDelays();
  size_t getExecutedDelays();
  void initializeThread(size_t cycle_period, size_t inst_period);
  void shutdownThread();
}

#endif
