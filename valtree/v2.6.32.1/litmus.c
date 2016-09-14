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
#include <rcupdate.c>
#include "rcutree.c"
#include "fake_sched.h"

/* Memory de-allocation boils down to a call to free */
void kfree(const void *p)
{
	free((void *) p);
}

/* Code under test */

int r_x;
int r_y;

int x;
int y;

void *thread_reader(void *cpu)
{
	set_cpu(cpu);
	fake_acquire_cpu(get_cpu());

#ifdef PREMATURE_GP_END
	do_IRQ();
	cond_resched();
#endif
	
	rcu_read_lock();	
        r_x = x; __VERIFIER_assume(rcu_sched_state.completed);
	do_IRQ();
#ifdef FORCE_FAILURE
	cond_resched();
	do_IRQ();
#endif
	r_y = y; 
	rcu_read_unlock();
/*#ifndef FORCE_FAILURE
	cond_resched();
	do_IRQ();
	#endif*/

	fake_release_cpu(get_cpu());	
	return NULL;
}

void *thread_update(void *cpu)
{
	set_cpu(cpu);
	fake_acquire_cpu(get_cpu());

#ifdef PREMATURE_GP_END
	synchronize_rcu();
#endif

	x = 1;
	synchronize_rcu();
#ifdef ASSERT_FAILURE
	assert(0);
#endif
	y = 1;

	fake_release_cpu(get_cpu());
	return NULL;
}

void *random_kthread(void *cpu)
{
	set_cpu(cpu);
	fake_acquire_cpu(get_cpu());

	cond_resched();
	do_IRQ();

	__VERIFIER_assume(rcu_sched_state.completed == 1);

	cond_resched();
	do_IRQ();

	fake_release_cpu(get_cpu());
	return NULL; 
}

int main()
{
	pthread_t tu, tr;
	int cpu_id[nr_cpu_ids];

	rcu_init();
	for (int i = 0; i < nr_cpu_ids; i++) {
		cpu_id[i] = i;
		set_cpu(&cpu_id[i]);
		rcu_enter_nohz();
	}

	if (pthread_create(&tr, NULL, random_kthread, &cpu_id[0]))
		abort();
        if (pthread_create(&tu, NULL, thread_update, &cpu_id[0]))
		abort();
	(void)thread_reader(&cpu_id[1]);

	if (pthread_join(tu, NULL))
		abort();
	if (pthread_join(tr, NULL))
		abort();
	
	BUG_ON(r_x == 0 && r_y == 1);
	
	return 0;
}
