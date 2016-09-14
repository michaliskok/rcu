/*
 * This test exposes a bug in Linux kernel in v2.6.31.1 and v2.6.32.1.
 * It is designed to run on both the emulated environments of 
 * kernel v2.6.31.1 and v2.6.32.1.
 * The bug is present in both multi-level hierarchies (-DMULTI_LEVEL) 
 * and single-node hierarchies.
 *
 * The bug was present in RCU versions prior to commit d09b62dfa336
 * (v.2.6.32), and was caused by unsynchronized accesses to the
 * ->completed field of the rcu_state struct.
 *
 * If -DKERNEL_VERSION_3 is defined, this test can also be run
 * in the emulated environment of Linux kernel v3.0.
 *
 * This test demonstrates how this bug may cause callbacks whose grace
 * period has not ended to be called prematurely. That can cause
 * trouble if there were pre-existing readers when a callback
 * was registered, and the callback is invoked before these readers
 * complete their RCU read-side critical sections.
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

#include "fake_defs.h"
#include "fake_sync.h"
#include <linux/rcupdate.h>
#include <rcupdate.c>
#include "rcutree.c"
#include "fake_sched.h"

/* Memory de-allocation boils down to a call to free */
void kfree(const void *p)
{
	free((void *) p);
}

int cpu0 = 0;
int cpu1 = 1;

/* 
 * In the emulated environment of v2.6.31 NR_CPUS == 3.
 * This admittedly peculiar architecture is chosen in order for an
 * alleged bug in this version to be exposed (see init_bug.c).
 */
#if NR_CPUS == 3
int cpu2 = 2;
#endif

/* Code under test */

int r_x;
int r_y;

int x;
int y;

/* Empty callback function */
void dummy(struct rcu_head *unused)
{
	return;
}
	
/*
 * thread_update() -- This thread represents code running in CPU0.
 *
 * CPU0 sets x = 1 and then calls synchronize_rcu() to wait for all
 * pre-existing readers to complete. However, due to the fact that
 * a grace-period ending can be seen after a grace-period beginning
 * in v2.6.3[12].1, synchronize_rcu() may end prematurely.
 * This way, it is possible for a pre-existing reader to see the
 * new value of y = 1.
 */
void *thread_update(void *arg)
{
	set_cpu(cpu0);
	fake_acquire_cpu(get_cpu());

	x = 1;
	synchronize_rcu();
	y = 1;
	
	fake_release_cpu(get_cpu());
	return NULL;
}

/*
 * thread_helper() -- This thread represents code running in CPU0.
 *
 * Since CPU0 will block after executing synchronize_rcu(), this
 * thread represents an arbitrary kernel thread that will occupy
 * CPU0 after CPU0 releases it.
 * If this thread services two interrupts, and under the right
 * circumstances, the premature invocation of CPU0's callbacks
 * is rendered possible.
 */
void *thread_helper(void *arg)
{
	set_cpu(cpu0);
	fake_acquire_cpu(get_cpu());

	do_IRQ();

	fake_release_cpu(get_cpu());
	return NULL;
}

/*
 * thread_reader() -- This thread represents code running in CPU1.
 *
 * CPU1 just passes through a quiescent state, starts an RCU read-side
 * critical section and then reports the quiescent state to the RCU
 * core mechanism. 
 * If CPU1 starts its RCU read-side critical section before CPU0
 * executes synchronize_rcu(), it should be impossible for CPU1 to
 * see changes after synchronize_rcu() (however, it is not).
 */
void *thread_reader(void *arg)
{
	set_cpu(cpu1);
	fake_acquire_cpu(get_cpu());

	do_IRQ();
	cond_resched();

	rcu_read_lock();
	r_x = x;
	do_IRQ();
	r_y = y;
	rcu_read_unlock();

	fake_release_cpu(get_cpu());
	return NULL;
}

int main()
{
	pthread_t tu, th;
	struct rcu_head cb1;

	/* Initialize RCU related data structures */
	rcu_init();
	/* For v3.0, suppose that the scheduler initialization has completed */
#ifdef KERNEL_VERSION_3
	rcu_scheduler_fully_active = 1;
#endif
	for (int i = 0; i < NR_CPUS; i++) {
		set_cpu(i);
		rcu_enter_nohz(); 	/* All CPUs start out idle */
	}


	/* 
	 * Setting up the environment. Every CPU except for CPU1 
	 * passes through a quiescent state and reports it to RCU.
	 */
	set_cpu(cpu0); 
	fake_acquire_cpu(get_cpu());
	call_rcu(&cb1, dummy);
#ifdef KERNEL_VERSION_3	
	do_IRQ(); /* In v3.0 call_rcu() does not always start a grace period */
#endif 
	cond_resched();
	do_IRQ();	
	fake_release_cpu(get_cpu());

#if NR_CPUS == 3
	set_cpu(cpu2);
	fake_acquire_cpu(get_cpu());
	do_IRQ();
	cond_resched();
	do_IRQ();
	fake_release_cpu(get_cpu());
#endif
    
	if (pthread_create(&tu, NULL, thread_update, NULL))
		abort();
	if (pthread_create(&th, NULL, thread_helper, NULL))
		abort();
	(void)thread_reader(NULL);

	if (pthread_join(tu, NULL))
		abort();
	if (pthread_join(th, NULL))
		abort();

	BUG_ON(r_x == 0 && r_y == 1);

	return 0;
}
