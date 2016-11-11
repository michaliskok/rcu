Validation of Tree RCU
======================


This repository aims at validating the non-preemptible version of
Linux kernel's Tree RCU using [Nidhugg](https://github.com/nidhugg/nidhugg).

* [Licence](#licence)
* [RCU Requirements](#requirements)
* [Tree RCU](#tree)
* [Usage](#usage)

<a name="licence">Licence</a>
-----------------------------

The code at this repository is distributed under the GPL, version 2 or (at your option) later.
Please see the COPYING file for details.

<a name="requirements">RCU Requirements</a>
-------------------------------------------

Every RCU implementation has to adhere to the following rule (Grace-Period guarantee):


> If any statement in a given RCU read-side critical section precedes a grace period,
> then all statements -including memory operations- in that RCU read-side critical
> section must complete before that grace period ends.

This fact allows RCU validation to be extremely focused: we can simply demonstrate that any
RCU read-side critical section in progress at the beginning of a grace period has to end
completely before that grace period ends (along with sufficient memory barriers to ensure that
the work done from RCU won't be undone from the compiler or the CPU).

The fundamental RCU requirements can be found
[here](https://www.kernel.org/doc/Documentation/RCU/Design/Requirements/).

<a name="tree">Tree RCU</a>
---------------------------

The problem with Classic RCU was that it suffered from increasing lock
contention due to the presence of one global lock that had to be acquired from
every CPU that wished to report a quiescent state to RCU.
In addition, Classic RCU's dyntick-idle interface had room for improvements,
since Classic RCU had to wake up every CPU (even the idle ones) at least once
per grace period, thus increasing power consumption.

Tree RCU offers a solution to both these problems since it reduces lock
contention and avoids awakening dyntick-idle CPUs. Tree RCU scales to
thousands of CPUs easily, while Classic RCU could scale only to several hundred.

### Proof Idea

In order to validate the grace-period guarantee for Tree RCU, the structure for
a canonical litmus test is provided below:

	void rcu_reader(void)
	{
	  rcu_read_lock();
	  r_x = x; 
	  r_y = y; 
	  rcu_read_unlock();
	}
	
	void rcu_updater(void)
	{
	  x = 1; 
	  synchronize_rcu();
	  y = 1; 
	}

	BUG_ON(r_x == 0 && r_y == 1);

### Technical Issues

1. Empty files have been provided in place of `#include` directives.
2. Various kernel primives have been copied directly from Linux kernel, while
fake definitions based on some specific `Kconfig` choices have been provided for
others. These are located in the files: `fake_defs.h`, `fake_sched.h` and `fake_sync.h`.
2. CPUs have been modeled with mutexes. The number of CPUs can be specified by setting
the `-DCONFIG_NR_CPUS=x` preprocessor option.
4. All CPUs start out idle.

<a name="usage">Usage</a>
-------------------------

To run all the default tests, simply run the file `driver.sh`.

### Tests explanation

Below an explanation for each test is presented, along with some of the Linux-kernel
versions with which each test works.

For more information, please look at the test files themselves.

| Filename       | Purpose                          |   Kernel Version   |        Notes        |
| -------------  |  ---------------------------     | :----------------: | :-----------------: |
| `litmus.c`     | Grace-Period guarantee test      |     3.19+          |          -          |
| `litmus_v3.c`  | Grace-Period guarantee test      |     3.0            |          -          | 
| `gp_end_bug.c` | Exposes known bug                |  2.6.3[12].1, 3.0  | Commit d09b62dfa336 |
| `init_bug.c`   | Deals with alleged bug           |  2.6.3[12].1       | Commit 83f5b01ffbba |
| `publish.c`    | Publish-Subscribe guarantee test |     3.19+          | Publisher's side    |

