/*
 * "Fake" declarations to scaffold a Linux-kernel UP environment.
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

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <assert.h>


/* Definitions taken from the Linux kernel. */

/*
 * Getting something that works in C and CPP for an arg that may or may
 * not be defined is tricky.  Here, if we have "#define CONFIG_BOOGER 1"
 * we match on the placeholder define, insert the "0," for arg1 and generate
 * the triplet (0, 1, 0).  Then the last step cherry picks the 2nd arg (a one).
 * When CONFIG_BOOGER is not defined, we generate a (... 1, 0) pair, and when
 * the last step cherry picks the 2nd arg, we get a zero.
 */
#define __ARG_PLACEHOLDER_1 0,
#define config_enabled(cfg) _config_enabled(cfg)
#define _config_enabled(value) __config_enabled(__ARG_PLACEHOLDER_##value)
#define __config_enabled(arg1_or_junk) ___config_enabled(arg1_or_junk 1, 0)
#define ___config_enabled(__ignored, val, ...) val

/*
 * IS_ENABLED(CONFIG_FOO) evaluates to 1 if CONFIG_FOO is set to 'y' or 'm',
 * 0 otherwise.
 *
 */
#define IS_ENABLED(option) \
	(config_enabled(option) || config_enabled(option##_MODULE))

#ifndef __maybe_unused
# define __maybe_unused         /* unimplemented */
#endif

/* Optimization barrier */
/* The "volatile" is due to gcc bugs */
#define barrier() __asm__ __volatile__("": : :"memory")

/**
 * struct callback_head - callback structure for use with RCU and task_work
 * @next: next update requests in a list
 * @func: actual update function to call after the grace period.
 */
struct callback_head {
	struct callback_head *next;
	void (*func)(struct callback_head *head);
};
#define rcu_head callback_head

typedef _Bool bool;

enum {
	false	= 0,
	true	= 1
};

#define USHRT_MAX	((u16)(~0U))
#define SHRT_MAX	((s16)(USHRT_MAX>>1))
#define SHRT_MIN	((s16)(-SHRT_MAX - 1))
#define INT_MAX		((int)(~0U>>1))
#define INT_MIN		(-INT_MAX - 1)
#define UINT_MAX	(~0U)
#define LONG_MAX	((long)(~0UL>>1))
#define LONG_MIN	(-LONG_MAX - 1)
#define ULONG_MAX	(~0UL)
#define LLONG_MAX	((long long)(~0ULL>>1))
#define LLONG_MIN	(-LLONG_MAX - 1)
#define ULLONG_MAX	(~0ULL)
#define SIZE_MAX	(~(size_t)0)

#define U8_MAX		((u8)~0U)
#define S8_MAX		((s8)(U8_MAX>>1))
#define S8_MIN		((s8)(-S8_MAX - 1))
#define U16_MAX		((u16)~0U)
#define S16_MAX		((s16)(U16_MAX>>1))
#define S16_MIN		((s16)(-S16_MAX - 1))
#define U32_MAX		((u32)~0U)
#define S32_MAX		((s32)(U32_MAX>>1))
#define S32_MIN		((s32)(-S32_MAX - 1))
#define U64_MAX		((u64)~0ULL)
#define S64_MAX		((s64)(U64_MAX>>1))
#define S64_MIN		((s64)(-S64_MAX - 1))


/* "Cheater" definitions based on restricted Kconfig choices. */

#define CONFIG_TINY_RCU
#undef __CHECKER__
#undef CONFIG_DEBUG_LOCK_ALLOC
#undef CONFIG_DEBUG_OBJECTS_RCU_HEAD
#undef CONFIG_HOTPLUG_CPU
#undef CONFIG_MODULES
#undef CONFIG_NO_HZ_FULL_SYSIDLE
#undef CONFIG_PREEMPT_COUNT
#undef CONFIG_PREEMPT_RCU
#undef CONFIG_PROVE_RCU
#undef CONFIG_RCU_NOCB_CPU
#undef CONFIG_RCU_NOCB_CPU_ALL
#undef CONFIG_RCU_STALL_COMMON
#undef CONFIG_RCU_TRACE
#undef CONFIG_RCU_USER_QS
#undef CONFIG_SMP
#undef CONFIG_TASKS_RCU
#undef CONFIG_TREE_RCU

#define TASKS_RCU(x) do { } while (0)

#define is_idle_task(x) 0
#define idle_task(x) NULL

#define smp_mb() __sync_synchronize()
int noassert;
#ifdef ORDERING_BUG
#define SET_NOASSERT() do { noassert = 1; } while (0)
#define CK_NOASSERT() noassert
#define WARN_ON(condition) assert(noassert || !(condition))
#define WARN_ONCE(condition, format...) assert(noassert || !(condition))
#define WARN_ON_ONCE(condition)	assert(noassert || !(condition))
#else
#define SET_NOASSERT() do { noassert = 1; smp_mb(); } while (0)
#define CK_NOASSERT() ({ smp_mb(); noassert; })
#define WARN_ON(condition) assert(!(condition) || CK_NOASSERT())
#define WARN_ONCE(condition, format...) assert(!(condition) || CK_NOASSERT())
#define WARN_ON_ONCE(condition)	assert(!(condition) || CK_NOASSERT())
#endif

#define smp_processor_id()	0

/* Interrupts don't interrupt process-level code, so can cheat. */
#define open_softirq(x, y) do { } while (0)
int __thread need_softirq;
#define raise_softirq(x) do { need_softirq = 1; } while (0)

static inline void rcu_early_boot_tests(void)
{
}

#define EXPORT_SYMBOL_GPL(x)
#define EXPORT_SYMBOL(x)

struct task_struct {
	int pid;
	char comm[20];
};
struct task_struct *current;

struct softirq_action {
};

#define ftrace_dump(a) do { } while (0)

#define __acquire(RCU)
#define __release(RCU)

#define notrace
#define __init

#define in_softirq() 0

#define prefetch(next) do { } while (0)

#define preempt_disable() do { } while (0)
#define preempt_enable() do { } while (0)
#define preempt_disable_notrace() do { } while (0)
#define preempt_enable_notrace() do { } while (0)
#define local_bh_disable() do { } while (0)
#define local_bh_enable() do { } while (0)

/* Declarations to emulate CPU, interrupts, and scheduling.  */

void fake_acquire_cpu(void);
void fake_release_cpu(void);
void cond_resched(void);
#define might_sleep() do { } while (0)
void local_irq_save(unsigned long flags);
void local_irq_restore(unsigned long flags);
