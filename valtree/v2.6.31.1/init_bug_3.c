/*
 * This test is supposed to expose a bug in Linux kernel v.2.6.31.
 *
 * The bug was present in RCU versions prior to commit 83f5b01ffbba, 
 * which fixes a long-grace-period race between grace period forcing 
 * and grace period initialization.
 * Our test, however, shows that no such bug or race exists, and that
 * the scenario outlined in the commit log is impossible to happen.
 * More specifically, it is impossible to enter the RCU_SAVE_DYNTICK
 * leg of the switch statement in force_quiescent_state() before a 
 * grace period ends and record the dynticks_completed value after a
 * new grace period has started. There are two "if-statements" that
 * prevent this scenario from happening. In order to prove this, proper
 * assertions at force_quiescent_state() have been inserted.
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

int x;
int *ptr_x;

void null_function(struct rcu_head *unused)
{
	return;
}

void change_result(struct rcu_head *unused)
{
	x = 42;
	return;
}

void *thread_cpu0(void *unused)
{
	set_cpu(&cpu0);
	pthread_mutex_lock(&cpu_lock[get_cpu()]);
	
	force_quiescent_state(&rcu_state, 0);

	cond_resched();
	do_IRQ();

	fake_release_cpu(get_cpu());
	return NULL;
}

void *thread_cpu1(void *unused)
{
	set_cpu(&cpu1);
	pthread_mutex_lock(&cpu_lock[get_cpu()]);

	__VERIFIER_assume(rcu_state.gpnum == -297 && rcu_state.node[0].qsmask == rcu_state.node[0].qsmaskinit);

	do_IRQ();
	cond_resched();
	do_IRQ();

	__VERIFIER_assume(rcu_state.gpnum == -297 && rcu_state.node[1].qsmask == 0);

	rcu_read_lock();
	ptr_x = &x;
	rcu_read_unlock(); 

	fake_release_cpu(get_cpu());
	return NULL;
}

void *thread_cpu2(void *unused)
{
	struct rcu_head head;
  
	set_cpu(&cpu2);
	fake_acquire_cpu(get_cpu());

	__VERIFIER_assume(rcu_state.gpnum == -297 && rcu_state.node[1].qsmask == 0); 
	call_rcu(&head, change_result);
	do_IRQ(); 

	do_IRQ();
	cond_resched();
	do_IRQ();

	do_IRQ();

	fake_release_cpu(get_cpu());
	return NULL;
}

int main()
{
	pthread_t t0, t1;
	struct rcu_head head1, head2, head3;

	rcu_init();
	for (int i = 0; i < nr_cpu_ids; i++) {
		set_cpu(&i);
		rcu_enter_nohz();
	}

	set_cpu(&cpu0);
	fake_acquire_cpu(get_cpu());
	call_rcu(&head1, null_function);
	cond_resched();
	do_IRQ();
	
	set_cpu(&cpu1);
	fake_acquire_cpu(get_cpu()); 
	do_IRQ();
	cond_resched();
	do_IRQ();
	
	set_cpu(&cpu0);
	force_quiescent_state(&rcu_state, 0);
	do_IRQ();

	/* One grace period should have ended by now */
	assert(rcu_state.completed == -299 && rcu_state.gpnum == -299); 

	set_cpu(&cpu0);
	call_rcu(&head2, null_function);
	cond_resched();
	do_IRQ();
	call_rcu(&head3, null_function); /* This requires an additional GP */
	pthread_mutex_unlock(&cpu_lock[get_cpu()]); //fake_release_cpu(get_cpu());

	set_cpu(&cpu1);
	do_IRQ();
	cond_resched();
	do_IRQ();
	pthread_mutex_unlock(&cpu_lock[get_cpu()]); //fake_release_cpu(get_cpu());
    
	if (pthread_create(&t0, NULL, thread_cpu0, NULL))
		abort();
	if (pthread_create(&t1, NULL, thread_cpu1, NULL))
		abort();
	(void)thread_cpu2(NULL);

	if (pthread_join(t0, NULL))
		abort();
	if (pthread_join(t1, NULL))
		abort();

	BUG_ON(*ptr_x == 42 && rcu_state.completed == -297);

	return 0;
}
