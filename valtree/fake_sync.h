/*
 * "Fake" definitions to emulate some kernel synchronization mechanisms.
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

#ifndef __FAKE_SYNC_H
#define __FAKE_SYNC_H

#include <pthread.h>

/* 
 * Fake datatypes for various synchronization mechanisms 
 *
 * raw_spinlock_t, spinlock_t, mutex_t are emulated using pthread_mutex_lock.
 * In kernel, these are architecture-dependent types, with similar but not
 * identical behaviour which can be modeled with pthread_mutex_lock.
 */
typedef pthread_mutex_t raw_spinlock_t;
#define __RAW_SPIN_LOCK_UNLOCKED(lockname) PTHREAD_MUTEX_INITIALIZER

typedef pthread_mutex_t spinlock_t;
#define SPINLOCK_INITIALIZER PTHREAD_MUTEX_INITIALIZER	

struct mutex {
	pthread_mutex_t lock;
};
#define __MUTEX_INITIALIZER(lockname) { .lock = PTHREAD_MUTEX_INITIALIZER }

/*
 * wait_queue_head_t is just an empty struct. 
 * Although wait queues can be also modeled with condition variables, 
 * for our purposes, and due to the fact that Nidhugg uses the spin-assume 
 * transformation, busy-waiting is sufficient.
 */
typedef struct __wait_queue_head {
} wait_queue_head_t;

/*
 * Since threads waiting on a waitqueue are just spinning, threads waiting
 * for a completion will be spinning as well.
 * done is declared volatile in order to prevent the compiler from optimizing
 * the busy-loop.
 */
struct completion {
	volatile unsigned int done;
	wait_queue_head_t     wait;
};


/* 
 * Raw-spinlock functions
 */
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


/* 
 * Spinlock functions
 */
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


/* 
 * Mutex functions
 */
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


/* 
 * Waitqueue functions
 */
#define init_waitqueue_head(wait_queue_head) do { } while (0)

#define wake_up(wait_queue_head) do { } while (0)
#define wake_up_locked(wait_queue_head) do { } while (0)

#define wait_event_interruptible(w, condition)	\
({					        \
	do_IRQ();				\
	fake_release_cpu(get_cpu());		\
	while (!(condition))			\
		;				\
	fake_acquire_cpu(get_cpu());		\
}) 

#define wait_event_interruptible_timeout(w, condition, timeout)		\
({								        \
	do_IRQ();							\
	cond_resched();							\
	do_IRQ();							\
	fake_release_cpu(get_cpu());					\
	while (!(condition))						\
		;							\
	fake_acquire_cpu(get_cpu());					\
	true;								\
})


/* 
 * Completion functions
 */
void init_completion(struct completion *x)
{
        x->done = 0;
        init_waitqueue_head(&x->wait);
}

void wait_for_completion(struct completion *x)
{
	might_sleep();

	do_IRQ();
        fake_release_cpu(get_cpu());
	while (!x->done)
		;
	fake_acquire_cpu(get_cpu());
}
	
void complete(struct completion *x)
{
	x->done++;
}

#endif /* __FAKE_SYNC_H */
