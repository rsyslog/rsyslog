/* syncdemo - a program to demonstrate the performance and validity of different
 * synchronization methods as well as some timing properties.
 *
 * The task to be done is very simple: a single gloabl integer is to to incremented 
 * by multiple threads. All this is done in a very-high concurrency environment. Note that
 * the test is unfair to mechanisms likes spinlocks, because we have almost only wait
 * time but no real processing time between the waits. However, the test provides
 * some good insight into atomic instructions vs. other synchronisation methods.
 * It also proves that garbling variables by not doing proper synchronisation is
 * highly likely. For best results, this program should be executed on a 
 * multiprocessor machine (on a uniprocessor, it will probably not display the
 * problems caused by missing synchronisation).
 *
 * compile with $ gcc -O0 -o syncdemo -lpthread syncdemo.c
 *
 * This program REQUIRES linux. With slight modification, it may run on Solaris.
 * Note that gcc on Sparc does NOT offer atomic instruction support!
 *
 * Copyright (C) 2010 by Rainer Gerhards <rgerhards@hq.adiscon.com>
 * Released under the GNU GPLv3.
 *
 * Inspired by (retrieved 2010-04-13)
 * http://www.alexonlinux.com/multithreaded-simple-data-type-access-and-atomic-variables
 */
#define _GNU_SOURCE
#include <sched.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <linux/unistd.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <errno.h>
#include <getopt.h>

/* config settings */
static int bCPUAffinity = 0;
static int procs = 0; /* number of processors */
static int numthrds = 0; /* if zero, => equal num of processors */
static unsigned goal = 50000000; /* 50 million */
static int bCSV = 0; /* generate CVS output? */
static int numIterations = 1; /* number of iterations */
static int dummyLoad = 0;     /* number of dummy load iterations to generate */
static enum { none, atomic, cas, mutex, spinlock } syncType;

static int global_int = 0;	/* our global counter */
static unsigned thrd_WorkToDo;	/* number of computations each thread must do */
static volatile int bStartRun = 0; /* indicate to flag when threads should start */

static struct timeval tvStart, tvEnd; /* used for timing one testing iteration */

/* statistic counters */
static long long totalRuntime;

/* sync objects (if needed) */
static pthread_mutex_t mut;
static pthread_spinlock_t spin;

static char*
getSyncMethName()
{
	switch(syncType) {
	case none    : return "none";
	case atomic  : return "atomic instruction";
	case mutex   : return "mutex";
	case spinlock: return "spin lock";
	case cas     : return "cas";
	}
}


static pid_t
gettid()
{
	return syscall( __NR_gettid );
}


void *workerThread( void *arg )
{
	int i, j;
	int oldval, newval; /* for CAS sync mode */
	int thrd_num = (int)(long)arg;
	cpu_set_t set;

	CPU_ZERO(&set);
	CPU_SET(thrd_num % procs, &set);

	/* if enabled, try to put thread on a fixed CPU (the one that corresponds to the
	 * thread ID). This may 
	 */
	if(bCPUAffinity) {
		if (sched_setaffinity( gettid(), sizeof( cpu_set_t ), &set )) {
			perror( "sched_setaffinity" );
			return NULL;
		}
	}

	/* wait for "go" */
	while(bStartRun == 0)
		/*WAIT!*/;

	for (i = 0; i < thrd_WorkToDo; i++) {
		switch(syncType) {
		case none:
			global_int++;
			break;
		case atomic:
			__sync_fetch_and_add(&global_int,1);
			break;
		case cas:
			do {
				oldval = global_int;
				newval = oldval + 1;
			} while(!__sync_bool_compare_and_swap(&global_int, oldval, newval));
			break;
		case mutex:
			pthread_mutex_lock(&mut);
			global_int++;
			pthread_mutex_unlock(&mut);
			break;
		case spinlock:
			pthread_spin_lock(&spin);
			global_int++;
			pthread_spin_unlock(&spin);
			break;
		}

		/* we now generate "dummy load" if instructed to do so. The idea is that
		 * we do some other work, as in real life, so that we have a better
		 * ratio of sync vs. actual work to do.
		 */
		for(j = 0 ; j < dummyLoad ; ++j) {
			/* be careful: compiler may optimize loop out! */;
		}
	}

	return NULL;
}


static void beginTiming(void)
{
	if(!bCSV) {
		printf("Test Parameters:\n");
		printf("\tNumber of Cores.........: %d\n", procs);
		printf("\tNumber of Threads.......: %d\n", numthrds);
		printf("\tSet Affinity............: %s\n", bCPUAffinity ? "yes" : "no");
		printf("\tCount to................: %u\n", goal);
		printf("\tWork for each Thread....: %u\n", thrd_WorkToDo);
		printf("\tDummy Load Counter......: %d\n", dummyLoad);
		printf("\tSync Method used........: %s\n", getSyncMethName());
	}
	gettimeofday(&tvStart, NULL);
}


static void endTiming(void)
{
	unsigned delta;
	long sec, usec;

	gettimeofday(&tvEnd, NULL);
	if(tvStart.tv_usec > tvEnd.tv_usec) {
		tvEnd.tv_sec--;
		tvEnd.tv_usec += 1000000;
	}

	sec = tvEnd.tv_sec - tvStart.tv_sec;
	usec = tvEnd.tv_usec - tvStart.tv_usec;

	delta = thrd_WorkToDo * numthrds - global_int;
	if(bCSV) {
		printf("%s,%d,%d,%d,%u,%u,%ld.%ld\n",
			getSyncMethName(), procs, numthrds, bCPUAffinity, goal, delta, sec, usec);
	} else {
		printf("measured (sytem time) runtime is %ld.%ld seconds\n", sec, usec);
		if(delta == 0) {
			printf("Computation was done correctly.\n");
		} else {
			printf("Computation INCORRECT,\n"
			       "\texpected %9u\n"
			       "\treal     %9u\n"
			       "\toff by   %9u\n",
				thrd_WorkToDo * numthrds,
				global_int,
				delta);
		}
	}

	totalRuntime += sec * 1000 + (usec / 1000);
}


static void
usage(void)
{
	fprintf(stderr, "Usage: syncdemo -a -c<num> -t<num>\n");
	fprintf(stderr, "\t-a        set CPU affinity\n");
	fprintf(stderr, "\t-c<num>   count to <num>\n");
	fprintf(stderr, "\t-d<num>   dummy load, <num> iterations\n");
	fprintf(stderr, "\t-t<num>   number of threads to use\n");
	fprintf(stderr, "\t-s<type>  sync-type to use (none, atomic, mutex, spin)\n");
	fprintf(stderr, "\t-C        generate CVS output\n");
	fprintf(stderr, "\t-I        number of iterations\n");
	exit(2);
}


/* carry out the actual test (one iteration)
 */
static void
singleTest(void)
{
	int i;
	pthread_t *thrs;

	global_int = 0;
	bStartRun = 0;

	thrs = malloc(sizeof(pthread_t) * numthrds);
	if (thrs == NULL) {
		perror( "malloc" );
		exit(1);
	}

	thrd_WorkToDo = goal / numthrds;

	for (i = 0; i < numthrds; i++) {
		if(pthread_create( &thrs[i], NULL, workerThread, (void *)(long)i )) {
			perror( "pthread_create" );
			procs = i;
			break;
		}
	}

	beginTiming();
	bStartRun = 1; /* start the threads (they are busy-waiting so far!) */

	for (i = 0; i < numthrds; i++)
		pthread_join( thrs[i], NULL );

	endTiming();

	free( thrs );

}


int
main(int argc, char *argv[])
{
	int i;
	int opt;

	while((opt = getopt(argc, argv, "ac:d:i:t:s:C")) != EOF) {
		switch((char)opt) {
		case 'a':
			bCPUAffinity = 1;
			break;
		case 'c':
			goal = (unsigned) atol(optarg);
			break;
		case 'd':
			dummyLoad = atoi(optarg);
			break;
		case 'i':
			numIterations = atoi(optarg);
			break;
		case 't':
			numthrds = atoi(optarg);
			break;
		case 'C':
			bCSV = 1;
			break;
		case 's':
			if(!strcmp(optarg, "none"))
				syncType = none;
			else if(!strcmp(optarg, "atomic"))
				syncType = atomic;
			else if(!strcmp(optarg, "cas"))
				syncType = cas;
			else if(!strcmp(optarg, "mutex")) {
				syncType = mutex;
				pthread_mutex_init(&mut, NULL);
			} else if(!strcmp(optarg, "spin")) {
				syncType = spinlock;
				pthread_spin_init(&spin, PTHREAD_PROCESS_PRIVATE);
			} else {
				fprintf(stderr, "error: invalid sync mode '%s'\n", optarg);
				usage();
			}
			break;
		default:usage();
			break;
		}
	}

	/* Getting number of CPUs */
	procs = (int)sysconf(_SC_NPROCESSORS_ONLN);
	if (procs < 0) {
		perror( "sysconf" );
		return -1;
	}

	if(numthrds < 1) {
		numthrds = procs;
	}

	totalRuntime = 0;
	for(i = 0 ; i < numIterations ; ++i) {
		singleTest();
	}

	printf("total runtime %ld, avg %ld\n", totalRuntime, totalRuntime / numIterations);
	return 0;
}
