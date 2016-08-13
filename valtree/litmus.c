/*
 * Litmus test for correct RCU operation.
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
#include <update.c>
#include "tree.c"
#include "fake_sched.h"

/* Memory de-allocation boils down to a call to free */
void kfree(const void *p)
{
	free((void *) p);
}

/* Code under test */

int __unbuffered_tpr_x;
int __unbuffered_tpr_y;

int x;
int y;

void *thread_reader(void *cpu)
{
	set_cpu(cpu);
	fake_acquire_cpu(get_cpu());
	
	rcu_read_lock();	
	__unbuffered_tpr_x = x;
	do_IRQ();	
#ifdef FORCE_FAILURE
	cond_resched();
	do_IRQ();
#endif	
        __unbuffered_tpr_y = y;	
	rcu_read_unlock();
#ifndef FORCE_FAILURE
	cond_resched();
	do_IRQ();
#endif 

	fake_release_cpu(get_cpu());	
	return NULL;
}

void *thread_update(void *cpu)
{
	set_cpu(cpu);
	fake_acquire_cpu(get_cpu());

	x = 1;
	synchronize_rcu();
	y = 1;
	
	fake_release_cpu(get_cpu());
	return NULL;
}

void *run_gp_kthread(void *arg)
{
	struct rcu_state *rsp = arg;

	set_cpu(&GP_KTHREAD_CPU);
	current = rsp->gp_kthread; /* rcu_gp_kthread must not wake itself */
	
	fake_acquire_cpu(get_cpu());
	
	rcu_gp_kthread(rsp);
	
	fake_release_cpu(get_cpu());
	return NULL;
}

int main()
{
	pthread_t tu;
	int cpu_id[nr_cpu_ids];

	rcu_init();
	for (int i = 0; i < nr_cpu_ids; i++) {
		cpu_id[i] = i;
		set_cpu(&cpu_id[i]);
		rcu_idle_enter();
	}
	
        rcu_spawn_gp_kthread(); /* rcu_spawn_gp_kthread changed @ tree.c */
	if (pthread_create(&tu, NULL, thread_update, &cpu_id[0]))
		abort();
	(void)thread_reader(&cpu_id[1]);
	
	if (pthread_join(tu, NULL))
		abort();
	
	assert(__unbuffered_tpr_y == 0 || __unbuffered_tpr_x == 1 ||
	       CK_NOASSERT());
	
	return 0;
}
