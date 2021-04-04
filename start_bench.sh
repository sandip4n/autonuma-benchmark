#!/bin/bash

#
#  Copyright (C) 2012  Red Hat, Inc.
#
#  This work is licensed under the terms of the GNU GPL, version 2. See
#  the COPYING file in the top-level directory.
#
#  Tool for AutoNUMA benchmarking scripts
#
#  Requirements: numactl-devel, gnuplot
#

usage()
{
	echo -e "./start_bench.sh [-stnbiA] [-h]"
	echo -e "\t-s : run numa02_SMT test additionally"
	echo -e "\t-t : run numa01_THREAD_ALLOC test additionally"
	echo -e "\t-b : run *_HARD_BIND tests additionally"
	echo -e "\t-i : run *_INVERSE_BIND tests additionally"
	echo -e "\t-A : run all available tests"
	echo -e "\t-m : use a minimal number of threads per NUMA"
	echo -e "\t-h : this help"
}

test_numa()
{
	NUMNODES=`numactl --hardware | grep available`
	if [ $? -ne 0 ] ; then
		echo "Abort: NUMA hardware not detected."
		exit 1
	fi
	NUMNODES=`echo $NUMNODES | cut -f2 -d' '`
	if [ $NUMNODES -le 1 ] ; then
		echo "Abort: This machine has less than 2 nodes."
		exit 1
	fi

	case "$(uname -m)" in
	ppc64|ppc64le)
		THREADS=`ppc64_cpu --smt -n | cut -f2 -d '='`
		CORES=`ppc64_cpu --cores-on | cut -f2 -d '='`
		SIBLINGS=$[CORES*THREADS]
		;;
	*)
		SIBLINGS=`grep -m 1 'siblings' /proc/cpuinfo | cut -f2 -d':'`
		CORES=`grep -m 1 'cpu cores' /proc/cpuinfo | cut -f2 -d':'`
		;;
	esac

	if [ $MOF -ne 0 -a $MOF -le $CORES ]; then
		MOF=$[MOF*NUMNODES]
		echo "Migrate on fault test, use $MOF CPUs."
	fi
}

test_smt()
{
	if [ $SIBLINGS -eq $CORES ] ; then
		echo "SMT IS NOT enabled."
		SMT=0
	else
		echo "SMT IS enabled."
	fi
}

parse_numa() 
{
}

do_run_test()
{
	echo "$TESTNAME"
	nice -n -20 ./plot.sh $TESTNAME &
	PLOT_PID=$!
	/usr/bin/time -f"%e" ./$TESTNAME
	kill -s SIGTERM $PLOT_PID
	gawk -f genplot.awk $TESTNAME.txt | gnuplot
}

run_test()
{
	do_run_test
	ORIG_TESTNAME=$TESTNAME
	if [ $HARDBIND -eq 1 ] ; then
		TESTNAME="$ORIG_TESTNAME"_HARD_BIND
		do_run_test
	fi
	if [ $INVERSEBIND -eq 1 ] ; then
		TESTNAME="$ORIG_TESTNAME"_INVERSE_BIND
		do_run_test
	fi
}

run_bench()
{
	make
	test_smt
	TESTNAME=numa01
	run_test
	if [ $TALLOC -eq 1 ] ; then
		TESTNAME=numa01_THREAD_ALLOC
		run_test
	fi
	TESTNAME=numa02
	run_test
	if [ $SMT -eq 1 ] ; then
		TESTNAME=numa02_SMT
		run_test
	fi
}

cleanup() 
{
	make clean
}

SMT=0
TALLOC=0
HARDBIND=0
INVERSEBIND=0
MOF=0

while getopts "stnbiAmh" opt; do
	case $opt in
		s)
			SMT=1
			;;
		t)
			TALLOC=1
			;;
		b)
			HARDBIND=1
			;;
		i)
			INVERSEBIND=1
			;;
		A)
			SMT=1
			TALLOC=1
			HARDBIND=1
			INVERSEBIND=1
			;;
		m)
			MOF=2
			;;
		h)
			usage
			exit 0
			;;
		\?)
			echo "Wrong argument $opt"
			usage
			exit 1
			;;
	esac
done

test_numa
cleanup
parse_numa
run_bench
