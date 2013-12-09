#include "causal.h"
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

// Wrapped POSIX functions
extern "C" {
	void exit(int status) {
		Causal::getInstance().exit(status);
	}
	
	void _exit(int status) {
		Causal::getInstance()._exit(status);
	}
	
	void _Exit(int status) {
		Causal::getInstance()._Exit(status);
	}
	
	int pthread_create(pthread_t* thread, const pthread_attr_t* attr, void* (*fn)(void*), void* arg) {
		return Causal::getInstance().pthread_create(thread, attr, fn, arg);
	}

	int fork() {
		return Causal::getInstance().fork();
	}
}
