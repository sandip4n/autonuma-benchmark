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

all: numa01 numa02

numa01: numa01.c
	$(CC) $< $(CFLAGS) -DNO_BIND_FORCE_SAME_NODE -o $@
	$(CC) $< $(CFLAGS) -DNO_BIND_FORCE_SAME_NODE -DTHREAD_ALLOC -o numa01_THREAD_ALLOC
	$(CC) $< $(CFLAGS) -DHARD_BIND -o numa01_HARD_BIND
	$(CC) $< $(CFLAGS) -DHARD_BIND -DINVERSE_BIND -o numa01_INVERSE_BIND
	$(CC) $< $(CFLAGS) -DTHREAD_ALLOC -DHARD_BIND -o numa01_THREAD_ALLOC_HARD_BIND
	$(CC) $< $(CFLAGS) -DTHREAD_ALLOC -DHARD_BIND -DINVERSE_BIND -o numa01_THREAD_ALLOC_INVERSE_BIND
numa02: numa02.prep.c
	$(CC) $< $(CFLAGS) -o $@
	$(CC) $< $(CFLAGS) -DHARD_BIND -o numa02_HARD_BIND
	$(CC) $< $(CFLAGS) -DHARD_BIND -DINVERSE_BIND -o numa02_INVERSE_BIND
	$(CC) $< $(CFLAGS) -DSMT -o numa02_SMT
	$(CC) $< $(CFLAGS) -DSMT -DHARD_BIND -o numa02_SMT_HARD_BIND
	$(CC) $< $(CFLAGS) -DSMT -DHARD_BIND -DINVERSE_BIND -o numa02_SMT_INVERSE_BIND
clean: 
	rm -f numa01 numa02 numa01_* numa02_* numa02.prep.c *.txt *.pdf
