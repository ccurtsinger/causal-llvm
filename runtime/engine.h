#if !defined(CAUSAL_RUNTIME_ENGINE_H)
#define CAUSAL_RUNTIME_ENGINE_H

#include <stdint.h>
#include <stdlib.h>

#include "function.h"

namespace engine {
  void initialize();
  void addThread();
  void removeThread();
  Function& getFunction(uintptr_t);
};

#endif
