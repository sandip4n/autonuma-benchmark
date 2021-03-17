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
#include <numaif.h>
#include <sched.h>
#include <sys/wait.h>
#include <sys/file.h>
#include <sys/sysinfo.h>

CPUNUM
NODENUM
#ifndef SMT
#define THREADS NCPUS
#else
#define THREADS (NCPUS/2)
#endif
#define SIZE (1UL*1024*1024*1024)
#define TOTALSIZE (4UL*1024*1024*1024*200)
#define THREAD_SIZE (SIZE/THREADS)
//#define HARD_BIND
//#define INVERSE_BIND

static void *thread(void * arg)
{
	char *p = arg;
	int i;
	for (i = 0; i < TOTALSIZE/SIZE; i++) {
		bzero(p, THREAD_SIZE);
		asm volatile("" : : : "memory");
	}
	return NULL;
}

#ifdef HARD_BIND
static void bind(int node)
{
	int i;
	unsigned long nodemask;
	cpu_set_t *cpumask;
	size_t cpumasksz;
	int ncpus;

	ncpus = get_nprocs();
	cpumasksz = CPU_ALLOC_SIZE(ncpus);
	cpumask = CPU_ALLOC(ncpus);
	CPU_ZERO_S(cpumasksz, cpumask);

	nodemask = 0x1 << node;
	switch (node) {
		NODEMAP
		default:
			break;
	}
	if (sched_setaffinity(0, cpumasksz, cpumask) < 0)
		perror("sched_setaffinity"), exit(1);
	if (set_mempolicy(MPOL_BIND, &nodemask, 9) < 0)
		perror("set_mempolicy"), printf("%lu\n", nodemask), exit(1);

	CPU_FREE(cpumask);
}
#else
static void bind(int node) {}
#endif

int main()
{
	int i;
	pthread_t pthread[THREADS];
	char *p;
	pid_t pid;
	int f;

	p = malloc(SIZE);
	if (!p)
		perror("malloc"), exit(1);
	BINDMAP
	for (i = 0; i < THREADS; i++) {
		char *_p = p + THREAD_SIZE * i;
#ifdef INVERSE_BIND
		bind(numa_node_of_cpu(THREADS - 1 - i));
#else
		bind(numa_node_of_cpu(i));
#endif
		if (pthread_create(&pthread[i], NULL, thread, _p) != 0)
			perror("pthread_create"), exit(1);
	}
	for (i = 0; i < THREADS; i++)
		if (pthread_join(pthread[i], NULL) != 0)
			perror("pthread_join"), exit(1);
	return 0;
}
