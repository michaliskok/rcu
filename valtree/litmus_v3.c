/*
 * Litmus test for correct RCU operation in kernel v3.0.
 * In this version there is not an rcu_gp_kthread(), as in v3.19.
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

/* Code under test */

int r_x;
int r_y;

int x;
int y;

void *thread_reader(void *arg)
{
	set_cpu(cpu1);
	fake_acquire_cpu(get_cpu());

	rcu_read_lock();	
        r_x = x;
	do_IRQ();
#ifdef FORCE_FAILURE_4
	rcu_enter_nohz();
	rcu_exit_nohz();
#endif
#ifdef FORCE_FAILURE_1
	cond_resched();
	do_IRQ();
#endif
	r_y = y; 
	rcu_read_unlock();
#if !defined(FORCE_FAILURE_1) && !defined(FORCE_FAILURE_4)
	cond_resched();
	do_IRQ();
#endif

	fake_release_cpu(get_cpu());	
	return NULL;
}

void *thread_update(void *arg)
{
	set_cpu(cpu0);
	fake_acquire_cpu(get_cpu());

	x = 1;
	synchronize_rcu();
#ifdef ASSERT_0
	assert(0);
#endif
	y = 1;

	fake_release_cpu(get_cpu());
	return NULL;
}

void *thread_helper(void *arg)
{
	set_cpu(cpu0);
	fake_acquire_cpu(get_cpu());

	do_IRQ();
	cond_resched();
	do_IRQ();
#ifdef FORCE_FAILURE_4
	force_quiescent_state(&rcu_sched_state, 0);
	force_quiescent_state(&rcu_sched_state, 0);
#endif
	do_IRQ();
	
	fake_release_cpu(get_cpu());
	return NULL;
}

int main()
{
	pthread_t tu, th;

	rcu_scheduler_fully_active=1;
	rcu_init();
	for (int i = 0; i < NR_CPUS; i++) {
		set_cpu(i);
		rcu_enter_nohz();
	}
	
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
