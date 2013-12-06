#if !defined(CAUSAL_RUNTIME_REAL_H)
#define CAUSAL_RUNTIME_REAL_H

#include <dlfcn.h>

#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>

#define MAKE_WRAPPER(name, handle) \
  static typeof(::name)* name() { \
    static typeof(::name)* _fn = (typeof(::name)*)dlsym(handle, #name); \
    return _fn; \
  }

class Real {
public:
  MAKE_WRAPPER(exit, RTLD_NEXT);
  MAKE_WRAPPER(_exit, RTLD_NEXT);
  MAKE_WRAPPER(_Exit, RTLD_NEXT);
  MAKE_WRAPPER(fork, RTLD_NEXT);
  MAKE_WRAPPER(pthread_create, RTLD_NEXT);
};

#endif
