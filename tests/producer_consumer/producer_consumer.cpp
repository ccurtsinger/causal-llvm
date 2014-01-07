#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <pthread.h>
#include <cassert>

#include <causal.h>

enum {
	Items = 1000000,
	QueueSize = 10,
	ProducerCount = 5,
	ConsumerCount = 3
};

int produced = 0;
int consumed = 0;
int queue_size;
int queue[QueueSize];

size_t signal_calls = 0;

pthread_mutex_t queue_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t producer_condvar = PTHREAD_COND_INITIALIZER;
pthread_cond_t consumer_condvar = PTHREAD_COND_INITIALIZER;
pthread_cond_t main_condvar = PTHREAD_COND_INITIALIZER;

void foo(pthread_cond_t* cv) {
  static int x, y, z;
  x++;
  y++;
  z++;
  __atomic_fetch_add(&signal_calls, 1, __ATOMIC_SEQ_CST);
  pthread_cond_signal(cv);
}

void* producer(void* arg) {
	for(size_t n = 0; n < Items / ProducerCount; n++) {
		pthread_mutex_lock(&queue_lock);
		while(queue_size == QueueSize) {
			pthread_cond_wait(&producer_condvar, &queue_lock);
		}
		queue[queue_size] = 123;
		queue_size++;
		produced++;
		pthread_mutex_unlock(&queue_lock);
		foo(&consumer_condvar);
	}
	pthread_mutex_lock(&queue_lock);
	foo(&main_condvar);
	pthread_mutex_unlock(&queue_lock);
	return NULL;
}

void* consumer(void* arg) {
	while(true) {
		pthread_mutex_lock(&queue_lock);
		while(queue_size == 0) {
			pthread_cond_wait(&consumer_condvar, &queue_lock);
		}
		queue_size--;
		assert(queue[queue_size] == 123);
		queue[queue_size] = 321;
		consumed++;
    if(consumed >= Items) {
      foo(&main_condvar);
    }
    
		pthread_mutex_unlock(&queue_lock);
		foo(&producer_condvar);
		CAUSAL_PROGRESS;
	}
}

int main(int argc, char** argv) {
	pthread_t producers[ProducerCount];
	pthread_t consumers[ConsumerCount];
	
	for(size_t i=0; i<ProducerCount; i++) {
		pthread_create(&producers[i], NULL, producer, NULL);
	}
	
	for(size_t i=0; i<ConsumerCount; i++) {
		pthread_create(&consumers[i], NULL, consumer, NULL);
	}
	
	pthread_mutex_lock(&queue_lock);
	while(consumed < Items) {
		pthread_cond_wait(&main_condvar, &queue_lock);
	}
	pthread_mutex_unlock(&queue_lock);
  
  fprintf(stderr, "%lu signal calls\n", signal_calls);
}
