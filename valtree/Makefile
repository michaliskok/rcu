# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, you can access it online at
# http://www.gnu.org/licenses/gpl-2.0.html.
#
# Note that in the rule which produces litmus, all lines containing
# empty inline assembly are deleted. These are produced by barrier(),
# they serve as compiler fences and they are NOT supported by Nidhugg.
#
# Author: Michalis Kokologiannakis <mixaskok@gmail.com>

.PHONY: clean distclean

CC=clang
CFLAGS=-I . -S -emit-llvm

default: litmus.ll

litmus.ll: litmus.c fake_defs.h fake_sched.h fake_locks.h tree.c Makefile
	$(CC) $(CFLAGS) litmus.c
	@sed -i.bak '/call void asm sideeffect ""/d' ./litmus.ll
	@if [ -f ./litmus.ll ] ; then echo SUCCESSFUL ; else echo FAILED ; fi

clean:
	rm ./litmus.ll
	rm ./litmus.ll.bak

distclean: clean
