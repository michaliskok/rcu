/*
 * "Fake" definitions to emulate some kernel locking mechanisms.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you can access it online at
 * http://www.gnu.org/licenses/gpl-2.0.html.
 *
 * Author: Michalis Kokologiannakis <mixaskok@gmail.com>
 */

#ifndef __FAKE_LOCKS_H
#define __FAKE_LOCKS_H

#include <pthread.h>

/* Fake datatypes for various locking mechanisms */

typedef pthread_mutex_t raw_spinlock_t;
#define __RAW_SPIN_LOCK_UNLOCKED(lockname) PTHREAD_MUTEX_INITIALIZER

typedef pthread_mutex_t spinlock_t;
#define SPINLOCK_INITIALIZER PTHREAD_MUTEX_INITIALIZER	

struct mutex {
	pthread_mutex_t lock;
};
#define __MUTEX_INITIALIZER(lockname) { .lock = PTHREAD_MUTEX_INITIALIZER }

typedef struct __wait_queue_head {
	spinlock_t     lock;
	pthread_cond_t cond;
} wait_queue_head_t;

struct completion {
	unsigned int      done;
	wait_queue_head_t wait;
};


/* Raw spinlocks functions */

void raw_spin_lock_init(raw_spinlock_t *l)
{
	if (pthread_mutex_init(l, NULL))
		exit(-1);
}

void raw_spin_lock_irqsave(raw_spinlock_t *l, unsigned long flags)
{
	local_irq_save(flags);
	preempt_disable();
	if (pthread_mutex_lock(l))
		exit(-1);
}

void raw_spin_unlock_irqrestore(raw_spinlock_t *l, unsigned long flags)
{
	if (pthread_mutex_unlock(l))
		exit(-1);
	local_irq_restore(flags);
	preempt_enable();
}

void raw_spin_lock_irq(raw_spinlock_t *l)
{
	local_irq_disable();
	preempt_disable();
	if (pthread_mutex_lock(l))
		exit(-1);
}

void raw_spin_unlock_irq(raw_spinlock_t *l)
{
	if (pthread_mutex_unlock(l))
		exit(-1);
	local_irq_enable();
	preempt_enable();
}

void raw_spin_lock(raw_spinlock_t *l)
{
	preempt_disable();
	if (pthread_mutex_lock(l))
		exit(-1);
}

void raw_spin_unlock(raw_spinlock_t *l)
{
	if (pthread_mutex_unlock(l))
		exit(-1);
	preempt_enable();
}

int raw_spin_trylock(raw_spinlock_t *l)
{
	preempt_disable();
	if (pthread_mutex_trylock(l)) {
		preempt_enable();
		return 0;
	}
	return 1;
}


/* Spinlock functions */

void spin_lock_irq(spinlock_t *lock)
{
	raw_spin_lock_irq(lock);
}

void spin_unlock_irq(spinlock_t *lock)
{
	raw_spin_unlock_irq(lock);
}

void spin_lock_irqsave(spinlock_t *lock, unsigned long flags)
{
	raw_spin_lock_irqsave(lock, flags);
}

void spin_unlock_irqrestore(spinlock_t *lock, unsigned long flags)
{
	raw_spin_unlock_irqrestore(lock, flags);
}


/* Mutex functions */

void mutex_lock(struct mutex *l)
{
	if (pthread_mutex_lock(&l->lock))
		exit(-1);
}

void mutex_unlock(struct mutex *l)
{
	if (pthread_mutex_unlock(&l->lock))
		exit(-1);
}


/* Waitqueues functions */

void init_waitqueue_head(wait_queue_head_t *w)
{
	if (pthread_mutex_init(&w->lock, NULL))
		exit(-1);
	if (pthread_cond_init(&w->cond, NULL))
		exit(-1);
}

void wake_up(wait_queue_head_t *w)
{
	if (pthread_mutex_lock(&w->lock))
		exit(-1);
	pthread_cond_signal(&w->cond);
	if (pthread_mutex_unlock(&w->lock))
		exit(-1);
}

void wake_up_locked(wait_queue_head_t *w)
{
	pthread_cond_signal(&w->cond);
}

#define wait_event_interruptible(w, condition)				\
	({								\
		if (pthread_mutex_lock(&(&w)->lock))			\
			exit(-1);					\
		fake_release_cpu(get_cpu());				\
		do_IRQ();						\
		while (!(condition)) {					\
			pthread_cond_wait(&(&w)->cond, &(&w)->lock);}	\
		if (pthread_mutex_unlock(&(&w)->lock))			\
			exit(-1);					\
		if (test_done)						\
			pthread_exit(NULL);				\
		fake_acquire_cpu(get_cpu());				\
	})

#define wait_event_interruptible_timeout(w, condition, timeout)		\
	({								\
		wait_event_interruptible(w, condition);			\
		true;							\
	})


/* Completion functions */

void init_completion(struct completion *x)
{
        x->done = 0;
        init_waitqueue_head(&x->wait);
}

void wait_for_completion(struct completion *x)
{
	might_sleep();

	raw_spin_lock(&x->wait.lock); //spin_lock_irq(&x->wait.lock);
        fake_release_cpu(get_cpu());
	//do_IRQ();
	while(!x->done) { 
		pthread_cond_wait(&x->wait.cond, &x->wait.lock);}
	raw_spin_unlock(&x->wait.lock); //spin_unlock_irq(&x->wait.lock);
	fake_acquire_cpu(get_cpu());
}

void complete(struct completion *x)
{
	unsigned long flags;

	spin_lock_irqsave(&x->wait.lock, flags);
	x->done++;
	wake_up_locked(&x->wait);
	spin_unlock_irqrestore(&x->wait.lock, flags);
}

#endif /* __FAKE_LOCKS_H */
