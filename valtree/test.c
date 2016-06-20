#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <sched.h>
#include <assert.h>

//#define get_cpu() 
//#include "fake.h"

typedef struct {
	int x;
	int y;
} my_struct;

;

int __thread test_var;

//atomic_t *cnt;

pthread_mutex_t m;
//struct mutex test_m = { .lock = PTHREAD_MUTEX_INITIALIZER };

int completed;



void *thread_update(void *arg)
{
	for (int i = 0; i < 42; i++)
		;
	__asm__ volatile("mfence": : :"memory");
	completed = 1;
}

void *thread_process_reader(void *arg)
{
	while (!completed)
		;
}


/* Actually run the test. */
int main(int argc, char *argv[])
{
	pthread_t tu;
	pthread_t tpr;

	pthread_mutex_init(&m, NULL);
	
	if (pthread_create(&tu, NULL, thread_update, NULL))
		abort();
	if (pthread_create(&tpr, NULL, thread_process_reader, NULL))
		abort();
	if (pthread_join(tu, NULL))
		abort();
/*	if (pthread_join(tpr, NULL))
	abort();*/


	__asm__ volatile("mfence": : :"memory");
	assert(completed == 1);

	return 0;
}
