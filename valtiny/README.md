Validation of Tiny RCU
======================


This repository aims at validating Tiny RCU's Linux kernel code (v.3.19) using 
[Nidhugg](https://github.com/nidhugg/nidhugg).

* [Licence](#licence)
* [RCU Requirements](#RCU requirements)
* [Tiny RCU](#Tiny RCU)
* [Usage](#Usage)

Licence
-------

The code at this repository is distributed under the GPL, version 2 or (at your option) later.
Please see the COPYING file for details.

RCU Requirements
----------------

Every RCU implementation has to adhere to the following rule:

> If any statement in a given RCU read-side critical section precedes a grace period, then
> all statements in that RCU read-side critical section must complete before that grace
> period ends.

This fact allows RCU validation to be extremely focused: we can simply demonstrate that any
RCU read-side critical section in progress at the beginning of a grace period has to end
completely before that grace period ends (along with sufficient memory barriers to ensure that
the work done from RCU won't be undone from the compiler or the CPU).

Tiny RCU
--------

The key feature that distincts Tiny RCU from all other RCU implementations is that it is designed
to run on a single CPU. *This means that any given time the sole CPU passes through a quiescent state,
a grace period has elapsed*.

### Proof Idea

In order to prove the grace-period requirement of this implementation, we just need two threads:

	void rcu_reader(void)
	{
	  rcu_read_lock();
	  r1 = x; 
	  r2 = y; 
	  rcu_read_unlock();
	}
	
	void rcu_updater(void)
	{
	  x = 1; 
	  synchronize_rcu();
	  y = 1; 
	}

and then assert that:

	assert(!(r1 == 0 && r2 == 1));

### Technical Issues

In order for the above-mentioned verification to success we need to solve some issues.

1. Stub the unneeded #includes, and keep the needed primitives and definitions in "fake.h"
and "fake.c", e.g. IS_ENABLED, barrier(), and bool.
2. Tiny RCU runs on a single CPU, hence we used a mutex to simulate this environment. Since
we are in a non-preemptible kernel (Tiny RCU is only available for non-preemptible kernels),
preemption cannot happen everywhere (i.e. threads voluntarily leave the CPU when their work
is finished), and we have to keep that in mind
using a tool like Nidhugg, which performs the equivalent of a full state-space search. So,
the thread that is currently running on the CPU, holds that lock as well.
3. synchronize_rcu() can block. This is modeled by having it drop the lock and then (possibly)
re-acquire it.
4. The sole CPU starts out idle.

Usage
-----

To run under TSO:

	nidhuggc -I . -- --tso --disable-mutex-init-requirement fake.c

To force failure by dividing the grace period in half, and allowing a context switch
between the two grace periods:

	nidhuggc -I . -DFORCE_FAILURE -- --tso --disable-mutex-init-requirement fake.c
