/* Opens a large number of tcp connections and sends
 * messages over them. This is used for stress-testing.
 *
 * Params
 * -t	target address (default 127.0.0.1)
 * -p	target port (default 13514)
 * -n	number of target ports (targets are in range -p..(-p+-n-1)
 *      Note -c must also be set to at LEAST the number of -n!
 * -c	number of connections (default 1)
 * -m	number of messages to send (connection is random)
 * -i	initial message number (optional)
 * -P	PRI to be used for generated messages (default is 167).
 *      Specify the plain number without leading zeros
 * -d   amount of extra data to add to message. If present, the
 *      number itself will be added as third field, and the data
 *      bytes as forth. Add -r to randomize the amount of extra
 *      data included in the range 1..(value of -d).
 * -r	randomize amount of extra data added (-d must be > 0)
 * -s	(silent) do not show progress indicator (never done on non-tty)
 * -f	support for testing dynafiles. If given, include a dynafile ID
 *      in the range 0..(f-1) as the SECOND field, shifting all field values
 *      one field to the right. Zero (default) disables this functionality.
 * -M   the message to be sent. Disables all message format options, as
 *      only that exact same message is sent.
 * -I   read specified input file, do NOT generate own test data. The test
 *      completes when eof is reached.
 * -B   The specified file (-I) is binary. No data processing is done by
 *      tcpflood. If multiple connections are specified, data is read in
 *      chunks and spread across the connections without taking any record
 *      delemiters into account.
 * -C	when input from a file is read, this file is transmitted -C times
 *      (C like cycle, running out of meaningful option switches ;))
 * -D	randomly drop and re-establish connections. Useful for stress-testing
 *      the TCP receiver.
 * -F	USASCII value for frame delimiter (in octet-stuffing mode), default LF
 * -R	number of times the test shall be run (very useful for gathering performance
 *      data and other repetitive things). Default: 1
 * -S   number of seconds to sleep between different runs (-R) Default: 30
 * -X   generate sTats data records. Default: off
 * -e   encode output in CSV (not yet everywhere supported)
 *      for performance data:
 *      each inidividual line has the runtime of one test
 *      the last line has 0 in field 1, followed by numberRuns,TotalRuntime,
 *      Average,min,max
 * -T   transport to use. Currently supported: "udp", "tcp" (default)
 *      Note: UDP supports a single target port, only
 * -W	wait time between sending batches of messages, in microseconds (Default: 0)
 * -b   number of messages within a batch (default: 100,000,000 millions)
 * -Y	use multiple threads, one per connection (which means 1 if one only connection
 *  	is configured!)
 * -z	private key file for TLS mode
 * -Z	cert (public key) file for TLS mode
 * -L	loglevel to use for GnuTLS troubleshooting (0-off to 10-all, 0 default)
 *
 * Part of the testbench for rsyslog.
 *
 * Copyright 2009, 2010 Rainer Gerhards and Adiscon GmbH.
 *
 * This file is part of rsyslog.
 *
 * Rsyslog is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Rsyslog is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Rsyslog.  If not, see <http://www.gnu.org/licenses/>.
 *
 * A copy of the GPL can be found in the file "COPYING" in this distribution.
 */
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <errno.h>
#ifdef ENABLE_GNUTLS
#	include <gnutls/gnutls.h>
#	if GNUTLS_VERSION_NUMBER <= 0x020b00
#		include <gcrypt.h>
	GCRY_THREAD_OPTION_PTHREAD_IMPL;
#	endif
#endif

#define EXIT_FAILURE 1
#define INVALID_SOCKET -1
/* Name of input file, must match $IncludeConfig in test suite .conf files */
#define NETTEST_INPUT_CONF_FILE "nettest.input.conf" /* name of input file, must match $IncludeConfig in .conf files */

#define MAX_EXTRADATA_LEN 100*1024
#define MAX_SENDBUF 2 * MAX_EXTRADATA_LEN

static char *targetIP = "127.0.0.1";
static char *msgPRI = "167";
static int targetPort = 13514;
static int numTargetPorts = 1;
static int dynFileIDs = 0;
static int extraDataLen = 0; /* amount of extra data to add to message */
static int bRandomizeExtraData = 0; /* randomize amount of extra data added */
static int numMsgsToSend; /* number of messages to send */
static unsigned numConnections = 1; /* number of connections to create */
static int *sockArray;  /* array of sockets to use */
static int msgNum = 0;	/* initial message number to start with */
static int bShowProgress = 1; /* show progress messages */
static int bSilent = 0; /* completely silent operation */
static int bRandConnDrop = 0; /* randomly drop connections? */
static char *MsgToSend = NULL; /* if non-null, this is the actual message to send */
static int bBinaryFile = 0;	/* is -I file binary */
static char *dataFile = NULL;	/* name of data file, if NULL, generate own data */
static int numFileIterations = 1;/* how often is file data to be sent? */
static char frameDelim = '\n';	/* default frame delimiter */
FILE *dataFP = NULL;		/* file pointer for data file, if used */
static long nConnDrops = 0;	/* counter: number of time connection was dropped (-D option) */
static int numRuns = 1;		/* number of times the test shall be run */
static int sleepBetweenRuns = 30; /* number of seconds to sleep between runs */
static int bStatsRecords = 0;	/* generate stats records */
static int bCSVoutput = 0;	/* generate output in CSV (where applicable) */
static long long batchsize = 100000000ll;
static int waittime = 0;
static int runMultithreaded = 0; /* run tests in multithreaded mode */
static int numThrds = 1;	/* number of threads to use */
static char *tlsCertFile = NULL;
static char *tlsKeyFile = NULL;
static int tlsLogLevel = 0;

#ifdef ENABLE_GNUTLS
static gnutls_session_t *sessArray;	/* array of TLS sessions to use */
static gnutls_certificate_credentials tlscred;
#endif

/* variables for managing multi-threaded operations */
int runningThreads;		/* number of threads currently running */
int doRun;			/* shall sender thread begin to run? */
pthread_mutex_t thrdMgmt;	/* mutex for controling startup/shutdown */
pthread_cond_t condStarted;
pthread_cond_t condDoRun;

/* the following struct provides information for a generator instance (thread) */
struct instdata {
	/* lower and upper bounds for the thread in question */
	unsigned long long lower;
	unsigned long long numMsgs; /* number of messages to send */
	unsigned long long numSent; /* number of messages already sent */
	unsigned idx;	/**< index of fd to be used for sending */
	pthread_t thread; /**< thread processing this instance */
} *instarray = NULL;

/* the following structure is used to gather performance data */
struct runstats {
	unsigned long long totalRuntime;
	unsigned long minRuntime;
	unsigned long maxRuntime;
	int numRuns;
};

static int udpsock;			/* socket for sending in UDP mode */
static struct sockaddr_in udpRcvr;	/* remote receiver in UDP mode */

static enum { TP_UDP, TP_TCP, TP_TLS } transport = TP_TCP;

/* forward definitions */
static void initTLSSess(int);
static int sendTLS(int i, char *buf, int lenBuf);
static void closeTLSSess(int __attribute__((unused)) i);

/* prepare send subsystem for UDP send */
static inline int
setupUDP(void)
{
	if((udpsock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
		return 1;

	memset((char *) &udpRcvr, 0, sizeof(udpRcvr));
	udpRcvr.sin_family = AF_INET;
	udpRcvr.sin_port = htons(targetPort);
	if(inet_aton(targetIP, &udpRcvr.sin_addr)==0) {
		fprintf(stderr, "inet_aton() failed\n");
		return(1);
	}

	return 0;
}


/* open a single tcp connection
 */
int openConn(int *fd)
{
	int sock;
	struct sockaddr_in addr;
	int port;
	int retries = 0;
	int rnd;

	if((sock=socket(AF_INET, SOCK_STREAM, 0))==-1) {
		perror("socket()");
		return(1);
	}

	/* randomize port if required */
	if(numTargetPorts > 1) {
		rnd = rand(); /* easier if we need value for debug messages ;) */
		port = targetPort + (rnd % numTargetPorts);
	} else {
		port = targetPort;
	}
	memset((char *) &addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	if(inet_aton(targetIP, &addr.sin_addr)==0) {
		fprintf(stderr, "inet_aton() failed\n");
		return(1);
	}
	while(1) { /* loop broken inside */
		if(connect(sock, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
			break;
		} else {
			if(retries++ == 50) {
				perror("connect()");
				fprintf(stderr, "connect() failed\n");
				return(1);
			} else {
				usleep(100000); /* ms = 1000 us! */
			}
		}
	} 

	*fd = sock;
	return 0;
}


/* open all requested tcp connections
 * this includes allocating the connection array
 */
int openConnections(void)
{
	unsigned i;
	char msgBuf[128];
	size_t lenMsg;

	if(transport == TP_UDP)
		return setupUDP();

	if(bShowProgress)
		if(write(1, "      open connections", sizeof("      open connections")-1)){}
#	ifdef ENABLE_GNUTLS
	sessArray = calloc(numConnections, sizeof(gnutls_session_t));
#	endif
	sockArray = calloc(numConnections, sizeof(int));
	for(i = 0 ; i < numConnections ; ++i) {
		if(i % 10 == 0) {
			if(bShowProgress)
				printf("\r%5.5d", i);
		}
		if(openConn(&(sockArray[i])) != 0) {
			printf("error in trying to open connection i=%d\n", i);
			return 1;
		}
		if(transport == TP_TLS) {
			initTLSSess(i);
		}
	}
	if(bShowProgress) {
		lenMsg = sprintf(msgBuf, "\r%5.5d open connections\n", i);
		if(write(1, msgBuf, lenMsg)) {}
	}

	return 0;
}


/* we also close all connections because otherwise we may get very bad
 * timing for the syslogd - it may not be able to process all incoming
 * messages fast enough if we immediately shut down.
 * TODO: it may be an interesting excercise to handle that situation
 * at the syslogd level, too
 * rgerhards, 2009-04-14
 */
void closeConnections(void)
{
	unsigned i;
	size_t lenMsg;
	struct linger ling;
	char msgBuf[128];

	if(transport == TP_UDP)
		return;

	if(bShowProgress)
		if(write(1, "      close connections", sizeof("      close connections")-1)){}
	for(i = 0 ; i < numConnections ; ++i) {
		if(i % 10 == 0) {
			if(bShowProgress) {
				lenMsg = sprintf(msgBuf, "\r%5.5d", i);
				if(write(1, msgBuf, lenMsg)){}
			}
		}
		if(sockArray[i] != -1) {
			/* we try to not overrun the receiver by trying to flush buffers
			 * *during* close(). -- rgerhards, 2010-08-10
			 */
			ling.l_onoff = 1;
			ling.l_linger = 1;
			setsockopt(sockArray[i], SOL_SOCKET, SO_LINGER, &ling, sizeof(ling));
			if(transport == TP_TLS)
				closeTLSSess(i);
			close(sockArray[i]);
		}
	}
	if(bShowProgress) {
		lenMsg = sprintf(msgBuf, "\r%5.5d close connections\n", i);
		if(write(1, msgBuf, lenMsg)){}
	}

}


/* generate the message to be sent according to program command line parameters.
 * this has been moved to its own function as we now have various different ways
 * of constructing test messages. -- rgerhards, 2010-03-31
 */
static inline void
genMsg(char *buf, size_t maxBuf, int *pLenBuf, struct instdata *inst)
{
	int edLen; /* actual extra data length to use */
	char extraData[MAX_EXTRADATA_LEN + 1];
	char dynFileIDBuf[128] = "";
	int done;

	if(dataFP != NULL) {
		/* get message from file */
		do {
			done = 1;
			*pLenBuf = fread(buf, 1, MAX_EXTRADATA_LEN + 1024, dataFP);
			if(*pLenBuf == 0) {
				if(--numFileIterations > 0)  {
					rewind(dataFP);
					done = 0; /* need new iteration */
				} else {
					*pLenBuf = 0;
					goto finalize_it;
				}
			}
		} while(!done); /* Attention: do..while()! */
	} else if(MsgToSend == NULL) {
		if(dynFileIDs > 0) {
			snprintf(dynFileIDBuf, sizeof(dynFileIDBuf), "%d:", rand() % dynFileIDs);
		}
		if(extraDataLen == 0) {
			*pLenBuf = snprintf(buf, maxBuf, "<%s>Mar  1 01:00:00 172.20.245.8 tag msgnum:%s%8.8d:%c",
					       msgPRI, dynFileIDBuf, msgNum, frameDelim);
		} else {
			if(bRandomizeExtraData)
				edLen = ((long) rand() + extraDataLen) % extraDataLen + 1;
			else
				edLen = extraDataLen;
			memset(extraData, 'X', edLen);
			extraData[edLen] = '\0';
			*pLenBuf = snprintf(buf, maxBuf, "<%s>Mar  1 01:00:00 172.20.245.8 tag msgnum:%s%8.8d:%d:%s%c",
					       msgPRI, dynFileIDBuf, msgNum, edLen, extraData, frameDelim);
		}
	} else {
		/* use fixed message format from command line */
		*pLenBuf = snprintf(buf, maxBuf, "%s\n", MsgToSend);
	}
	++inst->numSent;

finalize_it: /*EMPTY to keep the compiler happy */;
}

/* send messages to the tcp connections we keep open. We use
 * a very basic format that helps identify the message
 * (via msgnum:<number>: e.g. msgnum:00000001:). This format is suitable
 * for extracton to field-based properties.
 * The first numConnection messages are sent sequentially, as are the
 * last. All messages in between are sent over random connections.
 * Note that message numbers start at 0.
 */
int sendMessages(struct instdata *inst)
{
	unsigned i = 0;
	int socknum;
	int lenBuf;
	int lenSend = 0;
	char *statusText = "";
	char buf[MAX_EXTRADATA_LEN + 1024];
	char sendBuf[MAX_SENDBUF];
	int offsSendBuf = 0;

	if(!bSilent) {
		if(dataFile == NULL) {
			printf("Sending %llu messages.\n", inst->numMsgs);
			statusText = "messages";
		} else {
			printf("Sending file '%s' %d times.\n", dataFile,
			       numFileIterations);
			statusText = "kb";
		}
	}
	if(bShowProgress)
		printf("\r%8.8d %s sent", 0, statusText);
	while(i < inst->numMsgs) {
		if(runMultithreaded) {
			socknum = inst->idx;
		} else {
			if(i < numConnections)
				socknum = i;
			else if(i >= inst->numMsgs - numConnections) {
				socknum = i - (inst->numMsgs - numConnections);
			} else {
				int rnd = rand();
				socknum = rnd % numConnections;
			}
		}
		genMsg(buf, sizeof(buf), &lenBuf, inst); /* generate the message to send according to params */
		if(lenBuf == 0)
			break;	/* terminate when no message could be generated */
		if(transport == TP_TCP) {
			if(sockArray[socknum] == -1) {
				/* connection was dropped, need to re-establish */
				if(openConn(&(sockArray[socknum])) != 0) {
					printf("error in trying to re-open connection %d\n", socknum);
					exit(1);
				}
			}
			lenSend = send(sockArray[socknum], buf, lenBuf, 0);
		} else if(transport == TP_UDP) {
			lenSend = sendto(udpsock, buf, lenBuf, 0, &udpRcvr, sizeof(udpRcvr));
		} else if(transport == TP_TLS) {
			if(offsSendBuf + lenBuf < MAX_SENDBUF) {
				memcpy(sendBuf+offsSendBuf, buf, lenBuf);
				offsSendBuf += lenBuf;
				lenSend = lenBuf; /* simulate "good" call */
			} else {
				lenSend = sendTLS(socknum, sendBuf, offsSendBuf);
				lenSend = (lenSend == offsSendBuf) ? lenBuf : -1;
				memcpy(sendBuf, buf, lenBuf);
				offsSendBuf = lenBuf;
			}
		}
		if(lenSend != lenBuf) {
			printf("\r%5.5d\n", i);
			fflush(stdout);
			perror("send test data");
			printf("send() failed at socket %d, index %d, msgNum %lld\n",
				sockArray[socknum], i, inst->numSent);
			fflush(stderr);
			return(1);
		}
		if(i % 100 == 0) {
			if(bShowProgress)
				printf("\r%8.8d", i);
		}
		if(!runMultithreaded && bRandConnDrop) {
			/* if we need to randomly drop connections, see if we 
			 * are a victim
			 */
			if(rand() > (int) (RAND_MAX * 0.95)) {
				++nConnDrops;
				close(sockArray[socknum]);
				sockArray[socknum] = -1;
			}
		}
		if(inst->numSent % batchsize == 0) {
			usleep(waittime);
		}
		++msgNum;
		++i;
	}
	if(transport == TP_TLS && offsSendBuf != 0) {
		/* send remaining buffer */
		lenSend = sendTLS(socknum, sendBuf, offsSendBuf);
	}
	if(!bSilent)
		printf("\r%8.8d %s sent\n", i, statusText);

	return 0;
}


/* this is the thread that starts a generator
 */
static void *
thrdStarter(void *arg)
{
	struct instdata *inst = (struct instdata*) arg;
	pthread_mutex_lock(&thrdMgmt);
	runningThreads++;
	pthread_cond_signal(&condStarted);
	while(doRun == 0) {
		pthread_cond_wait(&condDoRun, &thrdMgmt);
	}
	pthread_mutex_unlock(&thrdMgmt);
	if(sendMessages(inst) != 0) {
		printf("error sending messages\n");
	}
	return NULL;
}


/* This function initializes the actual traffic generators. The function sets up all required
 * parameter blocks and starts threads. It returns when all threads are ready to run
 * and the main task must just enable them.
 */
static inline void
prepareGenerators()
{
	int i;
	long long msgsThrd;
	long long starting = 0;
	
	if(runMultithreaded) {
		bSilent = 1;
		numThrds = numConnections;
	} else {
		numThrds = 1;
	}

	runningThreads = 0;
	doRun = 0;
	pthread_mutex_init(&thrdMgmt, NULL);
	pthread_cond_init(&condStarted, NULL);
	pthread_cond_init(&condDoRun, NULL);

	if(instarray != NULL) {
		free(instarray);
	}
	instarray = calloc(numThrds, sizeof(struct instdata));
	msgsThrd = numMsgsToSend / numThrds;

	for(i = 0 ; i < numThrds ; ++i)  {
		instarray[i].lower = starting;
		instarray[i].numMsgs = msgsThrd;
		instarray[i].numSent = 0;
		instarray[i].idx = i;
		pthread_create(&(instarray[i].thread), NULL, thrdStarter, instarray + i); 
		/*printf("started thread %x\n", (unsigned) instarray[i].thread);*/
		starting += msgsThrd;
	}
}

/* Let all generators run. Threads must have been started. Here we wait until
 * all threads are initialized and then broadcast that they can begin to run.
 */
static inline void
runGenerators()
{
	pthread_mutex_lock(&thrdMgmt);
	while(runningThreads != numThrds){
		pthread_cond_wait(&condStarted, &thrdMgmt);
	}
	doRun = 1;
	pthread_cond_broadcast(&condDoRun);
	pthread_mutex_unlock(&thrdMgmt);
}


/* Wait for all traffic generators to stop.
 */
static inline void
waitGenerators()
{
	int i;
	for(i = 0 ; i < numThrds ; ++i)  {
		pthread_join(instarray[i].thread, NULL);
		/*printf("thread %x stopped\n", (unsigned) instarray[i].thread);*/
	}
	pthread_mutex_destroy(&thrdMgmt);
	pthread_cond_destroy(&condStarted);
	pthread_cond_destroy(&condDoRun);
}

/* functions related to computing statistics on the runtime of a test. This is
 * a separate function primarily not to mess up the test driver.
 * rgerhards, 2010-12-08
 */
static inline void
endTiming(struct timeval *tvStart, struct runstats *stats)
{
	long sec, usec;
	unsigned long runtime;
	struct timeval tvEnd;

	gettimeofday(&tvEnd, NULL);
	if(tvStart->tv_usec > tvEnd.tv_usec) {
		tvEnd.tv_sec--;
		tvEnd.tv_usec += 1000000;
	}

	sec = tvEnd.tv_sec - tvStart->tv_sec;
	usec = tvEnd.tv_usec - tvStart->tv_usec;

	runtime = sec * 1000 + (usec / 1000);
	stats->totalRuntime += runtime;
	if(runtime < stats->minRuntime)
		stats->minRuntime = runtime;
	if(runtime > stats->maxRuntime)
		stats->maxRuntime = runtime;

	if(!bSilent || bStatsRecords) {
		if(bCSVoutput) {
			printf("%ld.%3.3ld\n", runtime / 1000, runtime % 1000);
		} else {
			printf("runtime: %ld.%3.3ld\n", runtime / 1000, runtime % 1000);
		}
	}
}


/* generate stats summary record at end of run
 */
static inline void
genStats(struct runstats *stats)
{
	long unsigned avg;
	avg = stats->totalRuntime / stats->numRuns;

	if(bCSVoutput) {
		printf("#numRuns,TotalRuntime,AvgRuntime,MinRuntime,MaxRuntime\n");
		printf("%d,%llu.%3.3d,%lu.%3.3lu,%lu.%3.3lu,%lu.%3.3lu\n",
			stats->numRuns,
		        stats->totalRuntime / 1000, (int) stats->totalRuntime % 1000,
		        avg / 1000, avg % 1000,
		        stats->minRuntime / 1000, stats->minRuntime % 1000,
		        stats->maxRuntime / 1000, stats->maxRuntime % 1000);
	} else {
		printf("Runs:     %d\n",   stats->numRuns);
		printf("Runtime:\n");
		printf("  total:  %llu.%3.3d\n", stats->totalRuntime / 1000,
						 (int) stats->totalRuntime % 1000);
		printf("  avg:    %lu.%3.3lu\n",  avg / 1000, avg % 1000);
		printf("  min:    %lu.%3.3lu\n",  stats->minRuntime / 1000, stats->minRuntime % 1000);
		printf("  max:    %lu.%3.3lu\n",  stats->maxRuntime / 1000, stats->maxRuntime % 1000);
		printf("All times are wallclock time.\n");
	}
}


/* Run the actual test. This function handles various meta-parameters, like
 * a specified number of iterations, performance measurement and so on...
 * rgerhards, 2010-12-08
 */
static int
runTests(void)
{
	struct timeval tvStart;
	struct runstats stats;
	int run;

	stats.totalRuntime = 0;
	stats.minRuntime = 0xffffffffllu;
	stats.maxRuntime = 0;
	stats.numRuns = numRuns;
	run = 1;
	while(1) { /* loop broken inside */
		if(!bSilent)
			printf("starting run %d\n", run);
		prepareGenerators();
		gettimeofday(&tvStart, NULL);
		runGenerators();
		waitGenerators();
		endTiming(&tvStart, &stats);
		if(run == numRuns)
			break;
		if(!bSilent)
			printf("sleeping %d seconds before next run\n", sleepBetweenRuns);
		sleep(sleepBetweenRuns);
		++run;
	}

	if(bStatsRecords) {
		genStats(&stats);
	}

	return 0;
}

#	if defined(ENABLE_GNUTLS)
/* This defines a log function to be provided to GnuTLS. It hopefully
 * helps us track down hard to find problems.
 * rgerhards, 2008-06-20
 */
static void tlsLogFunction(int level, const char *msg)
{
	printf("GnuTLS (level %d): %s", level, msg);

}


/* global init GnuTLS
 */
static void
initTLS(void)
{
	int r;

	/* order of gcry_control and gnutls_global_init matters! */
	#if GNUTLS_VERSION_NUMBER <= 0x020b00
	gcry_control(GCRYCTL_SET_THREAD_CBS, &gcry_threads_pthread);
	#endif
	gnutls_global_init();
	/* set debug mode, if so required by the options */
	if(tlsLogLevel > 0) {
		gnutls_global_set_log_function(tlsLogFunction);
		gnutls_global_set_log_level(tlsLogLevel);
	}

	r = gnutls_certificate_allocate_credentials(&tlscred);
	if(r != GNUTLS_E_SUCCESS) {
		printf("error allocating credentials\n");
		gnutls_perror(r);
		exit(1);
	}
	r = gnutls_certificate_set_x509_key_file(tlscred, tlsCertFile, tlsKeyFile, GNUTLS_X509_FMT_PEM);
	if(r != GNUTLS_E_SUCCESS) {
		printf("error setting certificate files -- have you mixed up key and certificate?\n");
		printf("If in doubt, try swapping the files in -z/-Z\n");
		printf("Certifcate is: '%s'\n", tlsCertFile);
		printf("Key        is: '%s'\n", tlsKeyFile);
		gnutls_perror(r);
		r = gnutls_certificate_set_x509_key_file(tlscred, tlsKeyFile, tlsCertFile,
							 GNUTLS_X509_FMT_PEM);
		if(r == GNUTLS_E_SUCCESS) {
			printf("Tried swapping files, this seems to work "
			       "(but results may be unpredictable!)\n");
		} else {
			exit(1);
		}
	}
}


static void
initTLSSess(int i)
{
	int r;
	gnutls_init(sessArray + i, GNUTLS_CLIENT);

	/* Use default priorities */
	gnutls_set_default_priority(sessArray[i]);

	/* put our credentials to the current session */
	r = gnutls_credentials_set(sessArray[i], GNUTLS_CRD_CERTIFICATE, tlscred);
	if(r != GNUTLS_E_SUCCESS) {
		fprintf (stderr, "Setting credentials failed\n");
		gnutls_perror(r);
		exit(1);
	}

	/* NOTE: the following statement generates a cast warning, but there seems to
	 * be no way around it with current GnuTLS. Do NOT try to "fix" the situation!
	 */
	gnutls_transport_set_ptr(sessArray[i], (gnutls_transport_ptr_t) sockArray[i]);

	/* Perform the TLS handshake */
	r = gnutls_handshake(sessArray[i]);
	if(r < 0) {
		fprintf (stderr, "TLS Handshake failed\n");
		gnutls_perror(r);
		exit(1);
	}
}

static int
sendTLS(int i, char *buf, int lenBuf)
{
	int lenSent;
	int r;

	lenSent = 0;
	while(lenSent != lenBuf) {
		r = gnutls_record_send(sessArray[i], buf + lenSent, lenBuf - lenSent);
		if(r < 0)
			break;
		lenSent += r;
	}

	return lenSent;
}

static void
closeTLSSess(int i)
{
	gnutls_bye(sessArray[i], GNUTLS_SHUT_RDWR);
	gnutls_deinit(sessArray[i]);
}
#	else	/* NO TLS available */
static void initTLS(void) {}
static void initTLSSess(int __attribute__((unused)) i) {}
static int sendTLS(int __attribute__((unused)) i, char __attribute__((unused)) *buf, int __attribute__((unused)) lenBuf) { return 0; }
static void closeTLSSess(int __attribute__((unused)) i) {}
#	endif

/* Run the test.
 * rgerhards, 2009-04-03
 */
int main(int argc, char *argv[])
{
	int ret = 0;
	int opt;
	struct sigaction sigAct;
	struct rlimit maxFiles;
	static char buf[1024];

	srand(time(NULL));	/* seed is good enough for our needs */

	/* on Solaris, we do not HAVE MSG_NOSIGNAL, so for this reason
	 * we block SIGPIPE (not an issue for this program)
	 */
	memset(&sigAct, 0, sizeof(sigAct));
	sigemptyset(&sigAct.sa_mask);
	sigAct.sa_handler = SIG_IGN;
	sigaction(SIGPIPE, &sigAct, NULL);

	setvbuf(stdout, buf, _IONBF, 48);
	
	while((opt = getopt(argc, argv, "b:ef:F:t:p:c:C:m:i:I:P:d:Dn:L:M:rsBR:S:T:XW:Yz:Z:")) != -1) {
		switch (opt) {
		case 'b':	batchsize = atoll(optarg);
				break;
		case 't':	targetIP = optarg;
				break;
		case 'p':	targetPort = atoi(optarg);
				break;
		case 'n':	numTargetPorts = atoi(optarg);
				break;
		case 'c':	numConnections = (unsigned) atoi(optarg);
				break;
		case 'C':	numFileIterations = atoi(optarg);
				break;
		case 'm':	numMsgsToSend = atoi(optarg);
				break;
		case 'i':	msgNum = atoi(optarg);
				break;
		case 'P':	msgPRI = optarg;
				break;
		case 'd':	extraDataLen = atoi(optarg);
				if(extraDataLen > MAX_EXTRADATA_LEN) {
					fprintf(stderr, "-d max is %d!\n",
						MAX_EXTRADATA_LEN);
					exit(1);
				}
				break;
		case 'D':	bRandConnDrop = 1;
				break;
		case 'r':	bRandomizeExtraData = 1;
				break;
		case 'f':	dynFileIDs = atoi(optarg);
				break;
		case 'F':	frameDelim = atoi(optarg);
				break;
		case 'L':	tlsLogLevel = atoi(optarg);
				break;
		case 'M':	MsgToSend = optarg;
				break;
		case 'I':	dataFile = optarg;
				/* in this mode, we do not know the num messages to send, so
				 * we set a (high) number to keep the code happy.
				 */
				numMsgsToSend = 1000000;
				break;
		case 's':	bSilent = 1;
				break;
		case 'B':	bBinaryFile = 1;
				break;
		case 'R':	numRuns = atoi(optarg);
				break;
		case 'S':	sleepBetweenRuns = atoi(optarg);
				break;
		case 'X':	bStatsRecords = 1;
				break;
		case 'e':	bCSVoutput = 1;
				break;
		case 'T':	if(!strcmp(optarg, "udp")) {
					transport = TP_UDP;
				} else if(!strcmp(optarg, "tcp")) {
					transport = TP_TCP;
				} else if(!strcmp(optarg, "tls")) {
#					if defined(ENABLE_GNUTLS)
						transport = TP_TLS;
#					else
						fprintf(stderr, "compiled without TLS support: "
							"\"-Ttls\" not supported!\n");
						exit(1);
#					endif
				} else {
					fprintf(stderr, "unknown transport '%s'\n", optarg);
					exit(1);
				}
				break;
		case 'W':	waittime = atoi(optarg);
				break;
		case 'Y':	runMultithreaded = 1;
				break;
		case 'z':	tlsKeyFile = optarg;
				break;
		case 'Z':	tlsCertFile = optarg;
				break;
		default:	printf("invalid option '%c' or value missing - terminating...\n", opt);
				exit (1);
				break;
		}
	}

	if(bStatsRecords && waittime) {
		fprintf(stderr, "warning: generating performance stats and using a waittime "
				"is somewhat contradictory!\n");
	}

	if(!isatty(1) || bSilent)
		bShowProgress = 0;

	if(numConnections > 20) {
		/* if we use many (whatever this means, 20 is randomly picked)
		 * connections, we need to make sure we have a high enough
		 * limit. -- rgerhards, 2010-03-25
		 */
		maxFiles.rlim_cur = numConnections + 20;
		maxFiles.rlim_max = numConnections + 20;
		if(setrlimit(RLIMIT_NOFILE, &maxFiles) < 0) {
			perror("setrlimit to increase file handles failed");
			fprintf(stderr,
			        "could net set sufficiently large number of "
			        "open files for required connection count!\n");
			exit(1);
		}
	}
	
	if(dataFile != NULL) {
		if((dataFP = fopen(dataFile, "r")) == NULL) {
			perror(dataFile);
			exit(1);
		}
	}

	if(transport == TP_TLS) {
		initTLS();
	}

	if(openConnections() != 0) {
		printf("error opening connections\n");
		exit(1);
	}

	if(runTests() != 0) {
		printf("error running tests\n");
		exit(1);
	}

	closeConnections(); /* this is important so that we do not finish too early! */

	if(nConnDrops > 0 && !bSilent)
		printf("-D option initiated %ld connection closures\n", nConnDrops);

	if(!bSilent)
		printf("End of tcpflood Run\n");

	exit(ret);
}
