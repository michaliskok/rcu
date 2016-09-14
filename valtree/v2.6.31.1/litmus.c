/*
 * Test that exposes a bug in Linux kernel v.2.6.31.
 *
 * This test is intended to expose a bug in Linux kernel v2.6.31 prior to 
 * commit 83f5b01ffbba, which fixes a long-grace-period race between grace
 * period forcing and grace period initialization.
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

/* Code under test */

int cpu0 = 0;
int cpu1 = 1;
int cpu2 = 2;

void null_function(struct rcu_head *unused)
{
	return;
}

void *thread_cpu0(void *cpu)
{
	set_cpu(cpu);
	fake_acquire_cpu(get_cpu());

	force_quiescent_state(&rcu_state, 0);

	do_IRQ();
	cond_resched();
	force_quiescent_state(&rcu_state, 0);

	fake_release_cpu(get_cpu());
	return NULL;
}

void *thread_cpu1(void *cpu)
{
	set_cpu(cpu);
	fake_acquire_cpu(get_cpu());
	
	do_IRQ();

	fake_release_cpu(get_cpu());
	fake_acquire_cpu(get_cpu());
	
	fake_release_cpu(get_cpu());
	return NULL;
}

void *thread_cpu2(void *cpu)
{
	struct rcu_head head;
  
	set_cpu(cpu);
	fake_acquire_cpu(get_cpu());
	
	call_rcu(&head, null_function);

	fake_release_cpu(get_cpu());
	fake_acquire_cpu(get_cpu());

	BUG_ON(rcu_state.completed == -298);

	fake_release_cpu(get_cpu());
	return NULL;
}

int main()
{
	pthread_t t0, t1;
	struct rcu_head head;

	rcu_init();
	for (int i = 0; i < nr_cpu_ids; i++) {
		set_cpu(&i);
		rcu_enter_nohz();
	}

	set_cpu(&cpu0);
	fake_acquire_cpu(get_cpu());
	call_rcu(&head, null_function);
	cond_resched();
	do_IRQ();
	fake_release_cpu(get_cpu());

	set_cpu(&cpu2);
	fake_acquire_cpu(get_cpu()); 
	do_IRQ();
        cond_resched();
	do_IRQ();
	fake_release_cpu(get_cpu());

	set_cpu(&cpu1);
	fake_acquire_cpu(cpu1);
	do_IRQ();
	cond_resched();
	fake_release_cpu(cpu1);
	
	if (pthread_create(&t0, NULL, thread_cpu0, &cpu0))
		abort();
	if (pthread_create(&t1, NULL, thread_cpu1, &cpu1))
		abort();
	(void)thread_cpu2(&cpu2);

	if (pthread_join(t0, NULL))
		abort();
	if (pthread_join(t1, NULL))
		abort();

	return 0;
}
