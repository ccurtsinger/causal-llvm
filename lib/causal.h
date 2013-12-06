#if !defined(CAUSAL_LIB_RUNTIME_CAUSAL_H)
#define CAUSAL_LIB_RUNTIME_CAUSAL_H

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>

#include <new>

#include "debug.h"
#include "real.h"

using namespace std;

struct Causal {
private:
	//atomic<bool> _initialized = ATOMIC_VAR_INIT(false);
	bool _initialized;
  
	class ThreadInit {
	private:
		void* (*_fn)(void*);
		void* _arg;
	public:
		ThreadInit(void* (*fn)(void*), void* arg) : _fn(fn), _arg(arg) {}

		void* run() {
			void* result = _fn(_arg);
			return result;
		}

		static void* entry(void* arg) {
			ThreadInit* init = (ThreadInit*)arg;
			return init->run();
		}
	};
  
	Causal() : _initialized(false) {}
  
public:
	static Causal& getInstance() {
		static char buf[sizeof(Causal)];
		static Causal* instance = new(buf) Causal();
		return *instance;
	}
	
	void initialize() {
		if(__atomic_exchange_n(&_initialized, true, __ATOMIC_SEQ_CST) == false) {
			DEBUG("Initializing");
		}
	}
	
	void shutdown() {
		if(__atomic_exchange_n(&_initialized, false, __ATOMIC_SEQ_CST) == true) {
			DEBUG("Shutting down");
		}
	}
  
	int fork() {
		int result = Real::fork()();
		if(result == 0) {
			// TODO: Clear profiling data (it will stay with the parent process)
			__atomic_store_n(&_initialized, false, __ATOMIC_SEQ_CST);
			initialize();
		}
		return result;
	}
  
	int pthread_create(pthread_t* thread, const pthread_attr_t* attr, void* (*fn)(void*), void* arg) {
		ThreadInit* init = new ThreadInit(fn, arg);
		pthread_t thread_id;
		int result = Real::pthread_create()(&thread_id, attr, ThreadInit::entry, init); 
		return result;
	}

	void __attribute__((noreturn)) exit(int status) {
    shutdown();
    Real::exit()(status);
  }
  
  void __attribute__((noreturn)) _exit(int status) {
    shutdown();
    Real::_exit()(status);
  }
  
  void __attribute__((noreturn)) _Exit(int status) {
    shutdown();
    Real::_Exit()(status);
  }
};

#endif
