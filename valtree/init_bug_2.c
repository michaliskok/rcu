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
 * assertions at force_quiescent_state() are inserted when FQS_NO_BUG
 * is defined.
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

/*
 * thread_reader() -- This thread represents CPU1.
 *
 * We want CPU1 to end the grace period the same time CPU0 tries to force it,
 * hence the __VERIFIER_assume() statement (although not required).
 * The commit log states that complex races between grace period forcing and
 * initialization may occur when CPU0 tries to force a grace period while CPU1
 * is ending it, which may cause the next grace period (started by CPU2) to be
 * too short.
 */
void *thread_cpu1(void *arg)
{
	set_cpu(cpu1);

	do_IRQ();

	return NULL;
}

void *thread_cpu2(void *arg)
{
	struct rcu_head head;
  
	set_cpu(cpu2);
	fake_acquire_cpu(get_cpu());
	
	call_rcu(&head, null_function);

	fake_release_cpu(get_cpu());
	return NULL;
}

/*
 * thread_forcer() -- Thread representing CPU0.
 *
 * According to the commit log, CPU0 can acquire the ->fqslock
 * in force_quiescent_state(), pick up the ->completed and ->signaled 
 * values _after_ CPU1 ends the grace period, enter the RCU_SAVE_DYNTICK
 * leg of the switch statement and then record the ->dynticks_completed
 * value as soon as CPU1 drops the root node's lock to initialize
 * the multi-level hierarchy (with ->signaled state being RCU_GP_INIT).
 * Proper assertions at force_quiescent_state() prove that this is not
 * possible.
 */
void *thread_forcer(void *arg)
{
	set_cpu(cpu0);
	fake_acquire_cpu(get_cpu());

	force_quiescent_state(&rcu_state, 0);

	fake_release_cpu(get_cpu());
	return NULL;
}

int main()
{
	pthread_t tu, tr;
	struct rcu_head head;

	rcu_init();
	for (int i = 0; i < NR_CPUS; i++) {
		set_cpu(i);
		rcu_enter_nohz();
	}

	/* 
	 * First, set up the environment. CPU0 registers a callback
	 * and starts a grace period. After that, each CPU passes through and 
	 * reports a quiescent state, with CPU1 the only CPU that has not 
	 * reported a quiescent state to the core RCU mechanism yet.
	 * Note that, initially, gpnum is set to -300UL.
	 */
	set_cpu(cpu0);
	fake_acquire_cpu(get_cpu());
	call_rcu(&head, null_function);
	cond_resched();
	do_IRQ();
	fake_release_cpu(get_cpu());

	/* CPU2 reports a quiescent state to the core RCU mechanism */
	set_cpu(cpu2);
	fake_acquire_cpu(get_cpu()); 
	do_IRQ();
        cond_resched();
	do_IRQ();
	fake_release_cpu(get_cpu());

	/* CPU1 passes through a quiescent state, but does not report it yet */
	set_cpu(cpu1);
	fake_acquire_cpu(cpu1);
	do_IRQ();
	cond_resched();
	
	if (pthread_create(&tu, NULL, thread_forcer, NULL))
		abort();
	if (pthread_create(&tr, NULL, thread_cpu1, NULL))
		abort();
	(void)thread_cpu2(NULL);

 	if (pthread_join(tu, NULL))
		abort();
	if (pthread_join(tr, NULL))
		abort();

	return 0;
}
