#if !defined(CAUSAL_H)
#define CAUSAL_H

#ifndef __USE_GNU
#  define __USE_GNU
#endif

#include <dlfcn.h>

#if defined(__cplusplus)
extern "C" {
#endif

static void __init_counter(int kind, size_t* ctr, const char* filename, int line) {
  void (*reg)(int, size_t*, const char*, int) = (void (*)(int, size_t*, const char*, int))dlsym(RTLD_DEFAULT, "__causal_register_counter");
  if(reg != NULL) reg(kind, ctr, filename, line);
}

#define CAUSAL_INCREMENT_COUNTER(kind, file, line) \
  if(1) { \
    static unsigned char __causal_counter_initialized = 0; \
    static size_t __causal_counter = 0; \
    if(__atomic_exchange_n(&__causal_counter_initialized, 1, __ATOMIC_SEQ_CST) == 0) { \
      __init_counter(kind, &__causal_counter, file, line); \
    } \
    __atomic_fetch_add(&__causal_counter, 1, __ATOMIC_SEQ_CST); \
  }

#define PROGRESS_COUNTER 1
#define BEGIN_COUNTER 2
#define END_COUNTER 3

#define CAUSAL_PROGRESS CAUSAL_INCREMENT_COUNTER(PROGRESS_COUNTER, __FILE__, __LINE__)
#define CAUSAL_BEGIN CAUSAL_INCREMENT_COUNTER(BEGIN_COUNTER, __FILE__, __LINE__)
#define CAUSAL_END CAUSAL_INCREMENT_COUNTER(END_COUNTER, __FILE__, __LINE__)

#if defined(__cplusplus)
}
#endif

#endif
