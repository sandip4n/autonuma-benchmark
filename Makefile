#
#  Copyright (C) 2012  Red Hat, Inc.
#
#  This work is licensed under the terms of the GNU GPL, version 2. See
#  the COPYING file in the top-level directory.
# 
#  Tool for AutoNUMA benchmarking scripts
#  

CC=gcc
CFLAGS=-O2 -lnuma -pthread

all: numa01 numa02 nmstat

numa01: numa01.prep.c
	$(CC) $< $(CFLAGS) -o $@
	$(CC) $< $(CFLAGS) -DTHREAD_ALLOC -o numa01_THREAD_ALLOC
numa02: numa02.prep.c
	$(CC) $< $(CFLAGS) -o $@
	$(CC) $< $(CFLAGS) -DSMT -o numa02_SMT
nmstat: nmstat.c
	$(CC) $< -std=gnu99 -o $@
clean: 
	rm -f numa01 numa02 numa01_* numa02_* numa01.prep.c numa02.prep.c nmstat *.txt *.pdf 
