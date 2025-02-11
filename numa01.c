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
#include <numaif.h>
#include <sched.h>
#include <time.h>
#include <sys/wait.h>
#include <sys/file.h>
#include <sys/sysinfo.h>

THREADS_VAL
NDMASK1
NDMASK2
#define SIZE (3UL*1024*1024*1024) 
//#define THREAD_ALLOC
#ifdef THREAD_ALLOC
#define THREAD_SIZE (SIZE/THREADS)
#else
#define THREAD_SIZE SIZE
#endif
//#define HARD_BIND
//#define INVERSE_BIND
//#define NO_BIND_FORCE_SAME_NODE

static char *p_global;
static unsigned long nodemask_global;

void *thread(void * arg)
{
	char *p = arg;
	int i;
#ifndef THREAD_ALLOC
	int nr = 50;
#else
	int nr = 1000;
#endif
#ifdef NO_BIND_FORCE_SAME_NODE
	if (set_mempolicy(MPOL_BIND, &nodemask_global, 64) < 0)
		perror("set_mempolicy"), printf("%lu\n", nodemask_global),
			exit(1);
#endif
	bzero(p_global, SIZE);
#ifdef NO_BIND_FORCE_SAME_NODE
	if (set_mempolicy(MPOL_DEFAULT, NULL, 64) < 0)
		perror("set_mempolicy"), exit(1);
#endif
	for (i = 0; i < nr; i++) {
		bzero(p, THREAD_SIZE);
		asm volatile("" : : : "memory");
	}
	return NULL;
}

int main()
{
	int i;
	pthread_t pthread[THREADS];
	char *p;
	pid_t pid;
	cpu_set_t *cpumask;
	size_t cpumasksz;
	int ncpus;
	int f;
	unsigned long nodemask __attribute__((__unused__));

	nodemask_global = 1UL << __builtin_ctz((time(NULL) & 1) ?
						NODEMASK1 : NODEMASK2);
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

	ncpus = get_nprocs();
	cpumasksz = CPU_ALLOC_SIZE(ncpus);
	cpumask = CPU_ALLOC(ncpus);
	CPU_ZERO_S(cpumasksz, cpumask);

	if (!pid) {
		FIRST_HALF
	} else {
		SECOND_HALF
	}
#ifdef INVERSE_BIND
	if (nodemask == NODEMASK1)
		nodemask = NODEMASK2;
	else if (nodemask == NODEMASK1)
		nodemask = NODEMASK2;
#endif
#ifdef HARD_BIND
	if (sched_setaffinity(0, cpumasksz, cpumask) < 0)
		perror("sched_setaffinity"), exit(1);
#endif
#ifdef HARD_BIND
	if (set_mempolicy(MPOL_BIND, &nodemask, 64) < 0)
		perror("set_mempolicy"), printf("%lu\n", nodemask), exit(1);
#endif
	for (i = 0; i < THREADS; i++) {
		char *_p = p;
#ifdef THREAD_ALLOC
		_p += THREAD_SIZE * i;
#endif
		if (pthread_create(&pthread[i], NULL, thread, _p) != 0)
			perror("pthread_create"), exit(1);
	}
	for (i = 0; i < THREADS; i++)
		if (pthread_join(pthread[i], NULL) != 0)
			perror("pthread_join"), exit(1);
	if (pid)
		if (wait(NULL) < 0)
			perror("wait"), exit(1);

	CPU_FREE(cpumask);
	return 0;
}
