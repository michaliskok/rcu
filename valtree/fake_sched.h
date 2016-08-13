/*
 * "Fake" definitions to scaffold a Linux-kernel SMP environment.
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

#ifndef __FAKE_SCHED_H
#define __FAKE_SCHED_H

#ifndef __FAKE_DEFS_H
#include "fake_defs.h"
#endif

/*
 * The gp_kthread which runs in an early initcall, runs on an 
 * available CPU. The GP_KTHREAD_CPU variable defines the CPU on
 * which the gp_kthread will run. Running the thread on a different
 * CPU will result to unwanted behaviour.
 */
int GP_KTHREAD_CPU = 0;

/* 
 * Each thread has to run on a specific CPU. In order to achieve this,
 * we have a thread-local variable __running_cpu, which must be set
 * accordingly, when it enters the function it's designated to execute.
 */
int __thread __running_cpu;

/* 
 * get_cpu - returns the CPU on which a thread runs. 
 *
 * Note that this function could not be a macro, because it needs to 
 * be visible to files included before this one.
 */
int get_cpu(void)
{
	return __running_cpu;
}

/* 
 * set_cpu - sets the CPU on which a thread will run.
 *
 * It is preferrable that __running_cpu variable is not modified
 * directly, but only via get_cpu and set_cpu functions. Since this
 * function has to be called when a thread function starts, it takes
 * as argument the thread's argument.
 */
void set_cpu(void *cpu)
{
	__running_cpu = *((int *) cpu);
}

/*
 * Definitions to emulate CPU, interrupts, and scheduling.
 *
 * There is a cpu_lock for each cpu, when held, the corresponding thread
 *      is running. Array length is equal to nr_cpu_ids.
 * An irq_lock indicates that the corresponding thread has interrupts
 *	masked, perhaps due to being in an interrupt handler.  Acquire
 *	cpu_lock first, then irq_lock. You cannot disable interrupts
 *	unless you are running, after all!
 * An nmi_lock indicates that the corresponding thread is in an NMI
 *	handler.  You cannot acquire either cpu_lock or irq_lock while
 *	holding nmi_lock.
 */

pthread_mutex_t cpu_lock[nr_cpu_ids] = { [0 ... nr_cpu_ids-1] = PTHREAD_MUTEX_INITIALIZER };
pthread_mutex_t irq_lock[nr_cpu_ids] = { [0 ... nr_cpu_ids-1] = PTHREAD_MUTEX_INITIALIZER };
pthread_mutex_t nmi_lock[nr_cpu_ids] = { [0 ... nr_cpu_ids-1] = PTHREAD_MUTEX_INITIALIZER };

/*
 * Acquire the lock of the specified CPU. It is assumed that the CPU
 * of which the lock we are trying to acquire is idle, therefore
 * rcu_idle_exit is called.
 */
void fake_acquire_cpu(int cpu)
{
	if (pthread_mutex_lock(&cpu_lock[cpu]))
		exit(-1);
	rcu_idle_exit();
}

/*
 * Release the lock of the specified CPU. It is assumed that this CPU
 * will now be idle. If another process wants the lock of the CPU it
 * must obtain it using fake_acquire_cpu.
 */
void fake_release_cpu(int cpu)
{
	rcu_idle_enter();
	if (pthread_mutex_unlock(&cpu_lock[cpu]))
		exit(-1);
}

/*
 * Fake cond_resched by having the thread running on the CPU drop
 * the CPU lock and then try to acquire the lock of the same CPU. 
 * Before dropping the lock rcu_note_context_switch is called, since,
 * in theory, the __schedule function would have called it.
 */
int cond_resched(void)
{
	int ncpu;
  
	rcu_note_context_switch();
	fake_release_cpu(get_cpu());	
	fake_acquire_cpu(get_cpu());

	return 0;
}

void resched_cpu(int cpu)
{
	/* Uniplemented */
}

/* 
 * Functions that emulate interrupt enabling/disabling.
 *
 * These functions acquire the irq_lock if it is not a nested interrupt,
 * release it if we are returning to kernel space, and count the current 
 * interrupt depth.
 */
static int local_irq_depth[nr_cpu_ids];

void local_irq_save(unsigned long flags)
{
	if (!local_irq_depth[get_cpu()]++) {
		if (pthread_mutex_lock(&irq_lock[get_cpu()]))
			exit(-1);
	}	
}

void local_irq_restore(unsigned long flags)
{
	if (!--local_irq_depth[get_cpu()]) {
		if (pthread_mutex_unlock(&irq_lock[get_cpu()]))
			exit(-1);
	}	
}

void local_irq_disable(void)
{
	if (!local_irq_depth[get_cpu()]) {
		if (pthread_mutex_lock(&irq_lock[get_cpu()]))
			exit(-1);
	}
	local_irq_depth[get_cpu()] = 1;
}

void local_irq_enable(void)
{
	local_irq_depth[get_cpu()] = 0;
	if (pthread_mutex_unlock(&irq_lock[get_cpu()]))
		exit(-1);
}

int irqs_disabled_flags(unsigned long flags)
{
	return !!local_irq_depth[get_cpu()];
}

/*
 * Inform RCU that we are entering an interrupt handler.
 */
void irq_enter(void)
{
	rcu_irq_enter();
}

/*
 * If this CPU has pending callbacks, execute __do_softirq(). Since we only
 * care about RCU callbacks, just rcu_process_callbacks() is called.
 * Inform RCU that we are exiting an interrupt handler.
 */
void irq_exit(void)
{
	if (need_softirq[get_cpu()]) {
		rcu_process_callbacks(NULL);
		need_softirq[get_cpu()] = 0;
	}
	rcu_irq_exit();
}

/*
 * Main interrupt function. This function is designed to emulate timer
 * interrupts, however, I/O interrupts closely resemble timer interrupts.
 * This function assumes that the interrupt occured while in kernel space.
 * The irq_lock is held during the execution of the interrupt handler,
 * but it is released when softirqs are serviced.
 */
void do_IRQ(void)
{
	local_irq_disable();
	irq_enter();
	
	rcu_check_callbacks(0);

	local_irq_enable();
	irq_exit();
}

#endif /* __FAKE_SCHED_H */
