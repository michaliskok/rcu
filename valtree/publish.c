/*
 * Test for the Publish-Subscribe RCU guarantee.
 *
 * This test examines if the rcu_assign_pointer() primitive is necessary
 * for an updater using a simple publish-subscribe example, and the
 * effects that the underlying memory model has on the outcome of the
 * test.
 * Currently configured for kernel v3.19, x86 and powerpc (-DPOWERPC).
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

#include <stdio.h>
#include "fake_defs.h"
#include "fake_sync.h"
#include <linux/rcupdate.h>
#include <pthread.h>
#include <assert.h>

#define ENOMEM          12      /* Out of memory */

struct foo {
	int a;
	int b;
};
struct foo *gp = NULL;
spinlock_t gp_lock = SPINLOCK_INITIALIZER;

bool add_gp(int x, int y)
{
	struct foo *p;

	p = calloc(1, sizeof(*p));
	if (!p)
		return -ENOMEM;
	spin_lock(&gp_lock);
	if (rcu_access_pointer(gp)) {
		spin_unlock(&gp_lock);
		return false;
	}
	p->a = x;
	p->b = x;
#ifdef ORDERING_BUG
	gp = p;
#else
	rcu_assign_pointer(gp, p);
#endif
	spin_unlock(&gp_lock);
	return true;
}

bool use_gp(void)
{
	struct foo *p;
	
	rcu_read_lock();
	p = rcu_dereference(gp);
	if (p) {
		BUG_ON(p->a != 42 || p->b != 42);
		/* do something with p->a, p->b */
		rcu_read_unlock();
		return true;
	}
	rcu_read_unlock();
	return false;
}

void *thread_publisher(void *arg)
{
	add_gp(42, 42);
	return NULL;
}

void *thread_subscriber(void *arg)
{
	use_gp();
	return NULL;
}

int main()
{
	pthread_t tp;

	if (pthread_create(&tp, NULL, thread_publisher, NULL))
		abort();
	(void)thread_subscriber(NULL);

	if (pthread_join(tp, NULL))
		abort();
	
	return 0;
}

