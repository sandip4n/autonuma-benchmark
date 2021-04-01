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
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <numa.h>
#include <sched.h>
#include <sys/wait.h>
#include <sys/file.h>
#include <sys/sysinfo.h>

#define SIZE (1UL*1024*1024*1024)
#define TOTALSIZE (4UL*1024*1024*1024*200)
//#define HARD_BIND
//#define INVERSE_BIND

static int readcmd(char *cmd, char *fmt, ...)
{
	char buf[256] = { 0 };
	va_list args;
	FILE *f;

	f = popen(cmd, "r");
	if (!f)
		perror("popen"), exit(1);

	va_start(args, fmt);
	if (!(fgets(buf, sizeof(buf), f)))
		perror("fgets"), exit(1);

	vsscanf(buf, fmt, args);
	va_end(args);
	pclose(f);

	return 0;
}

static int smt()
{
	/* Assume one thread per core by default, i.e. SMT off */
	int val = 1;
#ifdef __powerpc64__
	if (readcmd("ppc64_cpu --smt -n | cut -f2 -d '='", "%d", &val))
		exit(1);
#elif defined(__x86__)
	if (readcmd("cat /sys/devices/system/cpu/smt/active", "%d", &val))
		exit(1);
	if (val)
		val = 2;
#endif
	return val;
}

static int smtmax()
{
	/* Assume one thread per core by default, i.e. SMT off */
	int val = 1;
#ifdef __powerpc64__
	if (readcmd("ppc64_cpu --threads-per-core | cut -f2 -d ':'", "%d", &val))
		exit(1);
#elif defined(__x86__)
	val = 2;
#endif
	return val;
}

static int nsmt, nthreads, nnodes, maxsmt, maxnode;
static struct bitmask *nodemask;
static unsigned long threadsz;

static void init()
{
	struct bitmask *nodecpus;
	int ncpus, i, j;
	long freemem;

	if (numa_available() < 0)
		fprintf(stderr, "numa02: insufficient resources\n"), exit(1);

	nsmt = smt();
	maxsmt = smtmax();
	ncpus = get_nprocs();
#ifndef SMT
	nthreads = ncpus;
#else
	nthreads = ncpus / nsmt;
#endif
	threadsz =  SIZE / nthreads;

	/* Find all nodes with memory */
	nodemask = numa_get_mems_allowed();
	nodecpus = numa_allocate_cpumask();
	maxnode = numa_max_node();

	/* Remove all cpu-less nodes and nodes with less free memory */
	for (i = 0, j = 0; i <= maxnode; i++) {
		if (!numa_bitmask_isbitset(nodemask, i))
			continue;
		if (numa_node_to_cpus(i, nodecpus))
			numa_error("numa_node_to_cpus"), exit(1);
		if (!numa_bitmask_weight(nodecpus) ||
		    numa_node_size(i, &freemem) < SIZE || freemem < SIZE)
			nodemask = numa_bitmask_clearbit(nodemask, i);
		else
			j++;
	}

	/* At least 2 nodes with cpus and enough memory are required */
	if (j < 2)
		fprintf(stderr, "numa02: insufficient resources\n"), exit(1);

	nnodes = numa_bitmask_weight(nodemask);
	numa_bitmask_free(nodecpus);
}

static void *thread(void * arg)
{
	char *p = arg;
	int i;
	for (i = 0; i < TOTALSIZE/SIZE; i++) {
		bzero(p, threadsz);
		asm volatile("" : : : "memory");
	}
	return NULL;
}

#ifdef HARD_BIND
static void bind(int node)
{
	struct bitmask *cpus, *mems;

	cpus = numa_allocate_cpumask();
	mems = numa_allocate_nodemask();
	mems = numa_bitmask_clearall(mems);
	mems = numa_bitmask_setbit(mems, node);

	if (numa_node_to_cpus(node, cpus))
		numa_error("numa_node_to_cpus"), exit(1);
	if (numa_sched_setaffinity(0, cpus) < 0)
		numa_error("numa_sched_setaffinity"), exit(1);
	numa_set_membind(mems);
	numa_bitmask_free(cpus);
	numa_bitmask_free(mems);
}
#else
static void bind(int node) {}
#endif

int main()
{
	int ncpus, node, i, j;
	pthread_t *pthread;
	char *p;

	init();
	ncpus = get_nprocs_conf();
	p = malloc(SIZE);
	if (!p)
		perror("malloc"), exit(1);

	for (i = 0, j = 0; i <= maxnode; i++) {
		if (!numa_bitmask_isbitset(nodemask, i))
			continue;
		bind(i);
		bzero(p + j * (SIZE / nnodes), SIZE / nnodes);
		j++;
	}

	pthread = (pthread_t *) malloc(nthreads * sizeof(pthread_t));
	if (!pthread)
		perror("malloc"), exit(1);

	for (i = 0; i < nthreads; i++) {
		char *_p = p + threadsz * i;
#ifdef INVERSE_BIND
		node = numa_node_of_cpu(ncpus - (i / nsmt + 1) * maxsmt + (i % nsmt));
#else
		node = numa_node_of_cpu((i / nsmt) * maxsmt + (i % nsmt));
#endif
		bind(node);
		if (pthread_create(&pthread[i], NULL, thread, _p) != 0)
			perror("pthread_create"), exit(1);
	}
	for (i = 0; i < nthreads; i++)
		if (pthread_join(pthread[i], NULL) != 0)
			perror("pthread_join"), exit(1);

	free(pthread);
	numa_bitmask_free(nodemask);

	return 0;
}
