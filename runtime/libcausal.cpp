#include "causal.h"
#include "real.h"

#include "../include/causal.h"

__attribute__((constructor)) void ctor() {
	Causal::getInstance().initialize();
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

struct ThreadInit {
private:
  void* (*_fn)(void*);
  void* _arg;
public:
  ThreadInit(void* (*fn)(void*), void* arg) : _fn(fn), _arg(arg) {}
  void* run() { return _fn(_arg); }
};

void* thread_wrapper(void* p) {
  ThreadInit* init = (ThreadInit*)p;
  ThreadInit local_init = *init;
  delete init;
  Causal::getInstance().initializeThread();
  return local_init.run();
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
	
	int pthread_create(pthread_t* thread, const pthread_attr_t* attr, void* (*fn)(void*), void* arg) {
    return Real::pthread_create()(thread, attr, thread_wrapper, new ThreadInit(fn, arg));
	}

	int fork() {
    int result = Real::fork()();
  	if(result == 0) Causal::getInstance().reinitialize();
    return result;
	}
}
