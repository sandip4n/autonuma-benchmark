/*
 *  Copyright (C) 2012  Red Hat, Inc.
 *
 *  This work is licensed under the terms of the GNU GPL, version 2. See
 *  the COPYING file in the top-level directory.
 */

#define _GNU_SOURCE
#include <pthread.h>
#include <strings.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <numa.h>
#include <sched.h>
#include <time.h>
#include <sys/wait.h>
#include <sys/file.h>
#include <sys/sysinfo.h>

#define SIZE (3UL*1024*1024*1024) 
//#define THREAD_ALLOC
//#define HARD_BIND
//#define INVERSE_BIND
//#define NO_BIND_FORCE_SAME_NODE

static struct bitmask *memmask_global;
static struct bitmask *cpumask[2];
static struct bitmask *memmask[2];
static unsigned long threadsz;
static char *p_global;
static int nthreads;

static void init()
{
	int maxnode, nnodes, ncpus, idx, i, j, k;
	struct bitmask *nodes, *cpus;
	long freemem;

	if (numa_available() < 0)
		fprintf(stderr, "numa01: insufficient resources\n"), exit(1);

	ncpus = get_nprocs();
	nthreads = ncpus / 2;
#ifdef THREAD_ALLOC
	threadsz =  SIZE / nthreads;
#else
	threadsz = SIZE;
#endif

	for (i = 0; i < 2; i++) {
		cpumask[i] = numa_allocate_cpumask();
		memmask[i] = numa_allocate_nodemask();
		cpumask[i] = numa_bitmask_clearall(cpumask[i]);
		memmask[i] = numa_bitmask_clearall(memmask[i]);
	}

	/* Find all nodes with memory */
	nodes = numa_get_mems_allowed();
	cpus = numa_allocate_cpumask();
	maxnode = numa_max_node();

	/* Remove all cpu-less nodes and nodes with less free memory */
	for (i = 0; i <= maxnode; i++) {
		if (!numa_bitmask_isbitset(nodes, i))
			continue;
		if (numa_node_to_cpus(i, cpus))
			numa_error("numa_node_to_cpus"), exit(1);
		if (!numa_bitmask_weight(cpus) ||
		    numa_node_size(i, &freemem) < SIZE || freemem < SIZE)
			nodes = numa_bitmask_clearbit(nodes, i);
	}

	/* Build the nodemasks for the two halves */
	nnodes = numa_bitmask_weight(nodes);
	for (i = 0, j = 0; i <= maxnode; i++) {
		if (!numa_bitmask_isbitset(nodes, i))
			continue;
		if (numa_node_to_cpus(i, cpus))
			numa_error("numa_node_to_cpus"), exit(1);
		idx = j < (nnodes / 2);
		for (k = 0; k < numa_bitmask_nbytes(cpus) * 8; k++) {
			if (numa_bitmask_isbitset(cpus, k))
				cpumask[idx] = numa_bitmask_setbit(cpumask[idx], k);
		}
		memmask[idx] = numa_bitmask_setbit(memmask[idx], i);
		j++;
	}

	/* At least 2 nodes with cpus and enough memory are required */
	if (j < 2)
		fprintf(stderr, "numa01: insufficient resources\n"), exit(1);

	numa_bitmask_free(nodes);
	numa_bitmask_free(cpus);
}

static void *thread(void * arg)
{
	char *p = arg;
	int i;
#ifndef THREAD_ALLOC
	int nr = 50;
#else
	int nr = 1000;
#endif
#ifdef NO_BIND_FORCE_SAME_NODE
	numa_set_membind(memmask_global);
#endif
	bzero(p_global, SIZE);
#ifdef NO_BIND_FORCE_SAME_NODE
	numa_set_localalloc();
#endif
	for (i = 0; i < nr; i++) {
		bzero(p, threadsz);
		asm volatile("" : : : "memory");
	}
	return NULL;
}

int main()
{
	int i;
	pthread_t *pthread;
	char *p;
	pid_t pid;
	int cpuidx, memidx, f;
	unsigned long nodemask __attribute__((__unused__));

	init();
	memmask_global = (time(NULL) & 1) ? memmask[0] : memmask[1];

	f = creat("lock", 0400);
	if (f < 0)
		perror("creat"), exit(1);
	if (unlink("lock") < 0)
		perror("unlink"), exit(1);

	if ((pid = fork()) < 0)
		perror("fork"), exit(1);

	p_global = p = malloc(SIZE);
	if (!p)
		perror("malloc"), exit(1);

	cpuidx = memidx = !!pid;
#ifdef INVERSE_BIND
	cpuidx = !memidx;
#endif
#ifdef HARD_BIND
	if (numa_sched_setaffinity(0, cpumask[cpuidx]) < 0)
		numa_error("numa_sched_setaffinity"), exit(1);
	numa_set_membind(memmask[memidx]);
#endif

	pthread = (pthread_t *) malloc(nthreads * sizeof(pthread_t));
	if (!pthread)
		perror("malloc"), exit(1);

	for (i = 0; i < nthreads; i++) {
		char *_p = p;
#ifdef THREAD_ALLOC
		_p += threadsz * i;
#endif
		if (pthread_create(&pthread[i], NULL, thread, _p) != 0)
			perror("pthread_create"), exit(1);
	}
	for (i = 0; i < nthreads; i++)
		if (pthread_join(pthread[i], NULL) != 0)
			perror("pthread_join"), exit(1);
	if (pid)
		if (wait(NULL) < 0)
			perror("wait"), exit(1);

	free(pthread);
	for (i = 0; i < 2; i++) {
		numa_bitmask_free(cpumask[i]);
		numa_bitmask_free(memmask[i]);
	}

	return 0;
}
