#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <assert.h>

int lock;
int val;
int noassert;
int badlock;

pthread_mutex_t mylock = PTHREAD_MUTEX_INITIALIZER;

void *lockthread(void *arg)
{
#ifdef FORCE_FAILURE
	if (__sync_and_and_fetch(&lock, 0))
		noassert = 1;
#elif defined(PTHREAD_MUTEX)
	if (pthread_mutex_lock(&mylock))
		abort();
#else
	if (__sync_fetch_and_add(&lock, 1))
		noassert = 1;
#endif
	if (val)
		badlock++;
	val = 1;
	val = 0;
#ifdef FORCE_FAILURE
	(void)__sync_and_and_fetch(&lock, 0);
#elif defined(PTHREAD_MUTEX)
	if (pthread_mutex_unlock(&mylock))
		abort();
#else
	(void)__sync_fetch_and_sub(&lock, 1);
#endif
}

int main(int argc, char *argv[])
{
	pthread_t t1;
	pthread_t t2;

	if (pthread_create(&t1, NULL, lockthread, NULL))
		abort();
	if (pthread_create(&t2, NULL, lockthread, NULL))
		abort();
	if (pthread_join(t1, NULL))
		abort();
	if (pthread_join(t2, NULL))
		abort();
	assert(noassert || !badlock);
}
