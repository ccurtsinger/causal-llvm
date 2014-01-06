#include <dlfcn.h>

#include "causal.h"
#include "real.h"

#include "../include/causal.h"

__attribute__((constructor)) void ctor() {
  Causal::getInstance();
}

__attribute__((destructor)) void dtor() {
	Causal::getInstance().shutdown();
}

extern "C" {
  void __causal_register_counter(int kind, size_t* ctr, const char* file, int line) {
    /*if(kind == PROGRESS_COUNTER) {
      fprintf(stderr, "Progress counter: %s:%d (%p)\n", file, line, ctr);
    } else if (kind == BEGIN_COUNTER) {
      fprintf(stderr, "Transaction begin counter: %s:%d (%p)\n", file, line, ctr);
    } else if (kind == END_COUNTER) {
      fprintf(stderr, "Transaction end counter: %s:%d (%p)\n", file, line, ctr);
    } else {
      fprintf(stderr, "Unknown counter type: %s:%d (%p)\n", file, line, ctr);
    }*/
  }
}

typedef void* (*thread_fn_t)(void*);

struct ThreadInit {
public:
  thread_fn_t _fn;
  void* _arg;
public:
  ThreadInit(thread_fn_t fn, void* arg) : _fn(fn), _arg(arg) {}
  void* run() { return _fn(_arg); }
};

extern "C" void* thread_wrapper(void* p) {
  ThreadInit* init = (ThreadInit*)p;
  ThreadInit local_init = *init;
  delete init;
  engine::addThread();
  void* result = local_init.run();
  engine::removeThread();
  return result;
}

// Wrapped POSIX functions
extern "C" {
	void exit(int status) {
		Causal::getInstance().shutdown();
    Real::exit()(status);
	}
	
	void _exit(int status) {
		Causal::getInstance().shutdown();
    Real::_exit()(status);
	}
	
	void _Exit(int status) {
		Causal::getInstance().shutdown();
    Real::_Exit()(status);
	}
	
	int pthread_create(pthread_t* thread, const pthread_attr_t* attr, thread_fn_t fn, void* arg) {
    void* arg_wrapper = (void*)new ThreadInit(fn, arg);
    return Real::pthread_create()(thread, attr, thread_wrapper, arg_wrapper);
	}
  
  void pthread_exit(void* arg) {
    engine::removeThread();
    Real::pthread_exit()(arg);
  }

	int fork() {
    int result = Real::fork()();
  	if(result == 0) Causal::getInstance().reinitialize();
    return result;
  }
}
