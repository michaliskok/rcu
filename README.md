Validation of RCU
=================


This repository aims at validating the Linux kernel's
Read-Copy-Update (RCU) mechanism.

Author: Michalis Kokologiannakis.

* [Licence](#licence)
* [Requirements](#requirements)
* [Directories](#directories)
* [Contact](#contact)

<a name="licence">Licence</a>
-----------------------------

The code at this repository is distributed under the GPL, version 2 or (at your option) later.
Please see the COPYING file for details.

<a name="requirements">Requirements</a>
--------------------------------------

In order to run the tests in this repository you need the stateless model checking
tool [Nidhugg](https://github.com/nidhugg/nidhugg).

More information about this tool can be found at
[this paper](https://arxiv.org/abs/1501.02069).

<a name="directories">Directories</a>
-------------------------------------

* The `valtiny` directory aims at validating the Tiny RCU flavour.
* The `valtree` directory aims at validating the Tree RCU flavour.

<a name="contact">Contact</a>
-----------------------------

Feel free to contact me at: `mixaskok at gmail dot com`.
