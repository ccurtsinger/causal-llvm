#include "causal.h"

__attribute__((constructor)) void ctor() {
	Causal::getInstance().initialize();
}

__attribute__((destructor)) void dtor() {
	Causal::getInstance().shutdown();
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
