/*
 * "Fake" definitions to scaffold a Linux-kernel UP environment.
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
 * Copyright IBM Corporation, 2015
 *
 * Author: Paul E. McKenney <paulmck@linux.vnet.ibm.com>
 * Modified: Michalis Kokologiannakis <mixaskok@gmail.com>
 */

#include "fake.h"
#include <linux/rcupdate.h>
#include "tiny.c"

/* Just say "no" to memory allocation. */
void kfree(const void *p)
{
}

/* Just say "no" to rcu_barrier() and friends. */
void wait_rcu_gp(call_rcu_func_t crf)
{
}

/*
 * Definitions to emulate CPU, interrupts, and scheduling.
 *
 * There is a cpu_lock, when held, the corresponding thread is running.
 * An irq_lock indicates that the corresponding thread has interrupts
 *	masked, perhaps due to being in an interrupt handler.  Acquire
 *	cpu_lock first, then irq_lock.  You cannot disable interrupts
 *	unless you are running, after all!
 * An nmi_lock indicates that the corresponding thread is in an NMI
 *	handler.  You cannot acquire either cpu_lock or irq_lock while
 *	holding nmi_lock.
 */

pthread_mutex_t cpu_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t irq_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t nmi_lock = PTHREAD_MUTEX_INITIALIZER;

void fake_acquire_cpu(void)
{
	if (pthread_mutex_lock(&cpu_lock))
		exit(-1);
	rcu_idle_exit();
}

void fake_release_cpu(void)
{
	rcu_idle_enter();
	if (pthread_mutex_unlock(&cpu_lock))
		exit(-1);
	if (need_softirq) {
		need_softirq = 0;
	}
}

void cond_resched(void)
{
	fake_release_cpu();
	fake_acquire_cpu();
}

static int __thread local_irq_depth;

void local_irq_save(unsigned long flags)
{
	if (!local_irq_depth++) {
		if (pthread_mutex_lock(&irq_lock))
			exit(-1);
	}
}

void local_irq_restore(unsigned long flags)
{
	if (!--local_irq_depth) {
		if (pthread_mutex_unlock(&irq_lock))
			exit(-1);
	}
}

/*
 * Code under test.
 */

int x;
int y;

int r_x;
int r_y;

void *thread_update(void *arg)
{
	fake_acquire_cpu();

	x = 1;
	synchronize_rcu();
	y = 1;

	fake_release_cpu();
}

void *thread_reader(void *arg)
{
	fake_acquire_cpu();

	rcu_read_lock();
        r_x = x;
#ifdef FORCE_FAILURE
	rcu_read_unlock();
	cond_resched();
	rcu_read_lock();
#endif
	r_y = y;
	rcu_read_unlock();

	fake_release_cpu();
}


/* Actually run the test. */
int main(int argc, char *argv[])
{
	pthread_t tu;
	pthread_t tr;

	rcu_idle_enter();
	if (pthread_create(&tu, NULL, thread_update, NULL))
		abort();
	if (pthread_create(&tr, NULL, thread_reader, NULL))
		abort();
	if (pthread_join(tu, NULL))
		abort();
	if (pthread_join(tr, NULL))
		abort();
	
	BUG_ON(r_x == 0 && r_y == 1);

	return 0;
}
