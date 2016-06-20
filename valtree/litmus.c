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
#include "fake_locks.h"
#include <linux/rcupdate.h>
#include <update.c>
#include <tree.c>
#include "fake_sched.h"

/* Memory allocation boils down to a call to free */
void kfree(const void *p)
{
	free((void *) p);
}

bool test_done;

/* Code under test */

int varx[nr_cpu_ids];
int vary[nr_cpu_ids];

int __thread __unbuffered_tpr_x;
int __thread __unbuffered_tpr_y;

int x;
int y;

void rcu_reader(void)
{
	rcu_read_lock();
	__unbuffered_tpr_x = x;
	__unbuffered_tpr_y = y;
	varx[get_cpu()] = x;
	vary[get_cpu()] = y;
	rcu_read_unlock();
}

void *thread_process_reader(void *cpu)
{
	set_cpu(cpu);
	fake_acquire_cpu(get_cpu());
	
	while (!test_done) {
		rcu_reader();
		cond_resched();
		do_IRQ();
	}

	fake_release_cpu(get_cpu());
	assert(!(__unbuffered_tpr_x == 0 && __unbuffered_tpr_y == 1));
}

void *thread_process_reader_undisrupted(void *cpu)
{
	set_cpu(cpu);
	fake_acquire_cpu(get_cpu());

	while (!test_done) {		
		rcu_reader();
		cond_resched();
		do_IRQ();
	}

	fake_release_cpu(get_cpu());
	assert(!(__unbuffered_tpr_x == 0 && __unbuffered_tpr_y == 1));
}

void *thread_update(void *cpu)
{
	set_cpu(cpu);
	fake_acquire_cpu(get_cpu());

	x = 1;
	synchronize_rcu();
	y = 1;

	fake_release_cpu(get_cpu());

	test_done = 1;
	assert(!(__unbuffered_tpr_x == 0 && __unbuffered_tpr_y == 1));
}

void *random_kthread(void *cpu)
{
	set_cpu(cpu);
	fake_acquire_cpu(get_cpu());
     	
	while (!test_done) {
		cond_resched();
		do_IRQ();
	}
	
	fake_release_cpu(get_cpu());
	assert(!(__unbuffered_tpr_x == 0 && __unbuffered_tpr_y == 1));
}

void *run_gp_kthread(void *arg)
{
	struct rcu_state *rsp = arg;

	set_cpu(&GP_KTHREAD_CPU);
	/* Set current to this gp_kthread so it won't wake itself up */
	current = rsp->gp_kthread;
	fake_acquire_cpu(get_cpu());

	rcu_gp_kthread(rsp);
	
	fake_release_cpu(get_cpu());
}

int main()
{
	struct rcu_state *rsp;
	int cpu_id[nr_cpu_ids];
	pthread_t tid[nr_cpu_ids];
	pthread_t tt;

	pthread_mutex_init(&rcu_sched_state.onoff_mutex.lock, NULL);
	pthread_mutex_init(&rcu_sched_state.barrier_mutex.lock, NULL);
	pthread_mutex_init(&rcu_sched_state.orphan_lock, NULL);
	pthread_mutex_init(&rcu_bh_state.onoff_mutex.lock, NULL);
	pthread_mutex_init(&rcu_bh_state.barrier_mutex.lock, NULL);
	pthread_mutex_init(&rcu_bh_state.orphan_lock, NULL);

	init_cpus();
	rcu_init();
	for (int i = 0; i < nr_cpu_ids; i++) {
		cpu_id[i] = i;
		set_cpu(&cpu_id[i]);
		rcu_idle_enter();
	}

        rcu_spawn_gp_kthread(); 
	if (pthread_create(&tid[1], NULL, thread_update, &cpu_id[1]))
		abort();
	if (pthread_create(&tid[2], NULL, thread_process_reader, &cpu_id[2]))
		abort();
	if (pthread_create(&tid[3], NULL, thread_process_reader_undisrupted, &cpu_id[3]))
		abort();
	if (pthread_create(&tt, NULL, random_kthread, &cpu_id[1]))
		abort();
	
	if (pthread_join(tid[1], NULL))
		abort();
	if (pthread_join(tid[2], NULL))
		abort(); 
	if (pthread_join(tid[3], NULL))
		abort();
	if (pthread_join(tt, NULL))
		abort();

	for (int i = 0; i < nr_cpu_ids; i++)
	  assert(!(varx[i] == 0 && vary[i] == 1));

	return 0;
}
