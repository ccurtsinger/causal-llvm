#if !defined(CAUSAL_RUNTIME_ENGINE_H)
#define CAUSAL_RUNTIME_ENGINE_H

#include <stdint.h>
#include <stdlib.h>

namespace engine {
  void initialize();
  void addThread();
  void removeThread();
  bool inRange(void* p);
  void collectSamples(void** cycle_samples, size_t* cycle_max, void** inst_samples, size_t* inst_max);
};

#endif
