/* Opens a large number of tcp connections and sends
 * messages over them. This is used for stress-testing.
 *
 * Params
 * argv[1]	target address
 * argv[2]	target port
 * argv[3]	number of connections
 * argv[4]	number of messages to send (connection is random)
 * argv[5]	initial message number (optional)
 *
 * Part of the testbench for rsyslog.
 *
 * Copyright 2009 Rainer Gerhards and Adiscon GmbH.
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

#define EXIT_FAILURE 1
#define INVALID_SOCKET -1
/* Name of input file, must match $IncludeConfig in test suite .conf files */
#define NETTEST_INPUT_CONF_FILE "nettest.input.conf" /* name of input file, must match $IncludeConfig in .conf files */

static char *targetIP;
static int targetPort;
static int numMsgsToSend; /* number of messages to send */
static int numConnections; /* number of connections to create */
static int *sockArray;  /* array of sockets to use */
static int msgNum = 0;	/* initial message number to start with */


/* open a single tcp connection
 */
int openConn(int *fd)
{
	int sock;
	struct sockaddr_in addr;
	int retries = 0;

	if((sock=socket(AF_INET, SOCK_STREAM, 0))==-1) {
		perror("socket()");
		return(1);
	}

	memset((char *) &addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(targetPort);
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
	int i;
	char msgBuf[128];
	size_t lenMsg;

	write(1, "      open connections", sizeof("      open connections")-1);
	sockArray = calloc(numConnections, sizeof(int));
	for(i = 0 ; i < numConnections ; ++i) {
		if(i % 10 == 0) {
			printf("\r%5.5d", i);
			//lenMsg = sprintf(msgBuf, "\r%5.5d", i);
			//write(1, msgBuf, lenMsg);
		}
		if(openConn(&(sockArray[i])) != 0) {
			printf("error in trying to open connection i=%d\n", i);
			return 1;
		}
	}
	lenMsg = sprintf(msgBuf, "\r%5.5d open connections\n", i);
	write(1, msgBuf, lenMsg);

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
	int i;
	char msgBuf[128];
	size_t lenMsg;

	write(1, "      close connections", sizeof("      close connections")-1);
	for(i = 0 ; i < numConnections ; ++i) {
		if(i % 10 == 0) {
			lenMsg = sprintf(msgBuf, "\r%5.5d", i);
			write(1, msgBuf, lenMsg);
		}
		close(sockArray[i]);
	}
	lenMsg = sprintf(msgBuf, "\r%5.5d close connections\n", i);
	write(1, msgBuf, lenMsg);

}


/* send messages to the tcp connections we keep open. We use
 * a very basic format that helps identify the message
 * (via msgnum:<number>: e.g. msgnum:00000001:). This format is suitable
 * for extracton to field-based properties.
 * The first numConnection messages are sent sequentially, as are the
 * last. All messages in between are sent over random connections.
 * Note that message numbers start at 0.
 */
int sendMessages(void)
{
	int i;
	int socknum;
	int lenBuf;
	int lenSend;
	char buf[2048];

	srand(time(NULL));	/* seed is good enough for our needs */

	printf("Sending %d messages.\n", numMsgsToSend);
	printf("\r%5.5d messages sent", 0);
	for(i = 0 ; i < numMsgsToSend ; ++i) {
		if(i < numConnections)
			socknum = i;
		else if(i >= numMsgsToSend - numConnections)
			socknum = i - (numMsgsToSend - numConnections);
		else
			socknum = rand() % numConnections;
		lenBuf = sprintf(buf, "<167>Mar  1 01:00:00 172.20.245.8 tag msgnum:%8.8d:\n", msgNum);
		lenSend = send(sockArray[socknum], buf, lenBuf, 0);
		if(lenSend != lenBuf) {
			printf("\r%5.5d\n", i);
			fflush(stdout);
			perror("send test data");
			printf("send() failed at socket %d, index %d, msgNum %d\n", socknum, i, msgNum);
			fflush(stderr);
			return(1);
		}
		if(i % 100 == 0) {
			printf("\r%5.5d", i);
		}
		++msgNum;
	}
	printf("\r%5.5d messages sent\n", i);

	return 0;
}


/* send a message via TCP
 * We open the connection on the initial send, and never close it
 * (let the OS do that). If a conneciton breaks, we do NOT try to
 * recover, so all test after that one will fail (and the test
 * driver probably hang. returns 0 if ok, something else otherwise.
 * We use traditional framing '\n' at EOR for this tester. It may be
 * worth considering additional framing modes.
 * rgerhards, 2009-04-08
 */
int
tcpSend(char *buf, int lenBuf)
{
	static int sock = INVALID_SOCKET;
	struct sockaddr_in addr;

	if(sock == INVALID_SOCKET) {
		/* first time, need to connect to target */
		if((sock=socket(AF_INET, SOCK_STREAM, 0))==-1) {
			perror("socket()");
			return(1);
		}

		memset((char *) &addr, 0, sizeof(addr));
		addr.sin_family = AF_INET;
		addr.sin_port = htons(13514);
		if(inet_aton("127.0.0.1", &addr.sin_addr)==0) {
			fprintf(stderr, "inet_aton() failed\n");
			return(1);
		}
		if(connect(sock, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
			fprintf(stderr, "connect() failed\n");
			return(1);
		}
	}

	/* send test data */
	if(send(sock, buf, lenBuf, 0) != lenBuf) {
		perror("send test data");
		fprintf(stderr, "send() failed\n");
		return(1);
	}

	/* send record terminator */
	if(send(sock, "\n", 1, 0) != 1) {
		perror("send record terminator");
		fprintf(stderr, "send() failed\n");
		return(1);
	}

	return 0;
}


/* Run the test suite. This must be called with exactly one parameter, the
 * name of the test suite. For details, see file header comment at the top
 * of this file.
 * rgerhards, 2009-04-03
 */
int main(int argc, char *argv[])
{
	int ret = 0;
	struct sigaction sigAct;
	static char buf[1024];

	/* on Solaris, we do not HAVE MSG_NOSIGNAL, so for this reason
	 * we block SIGPIPE (not an issue for this program)
	 */
	memset(&sigAct, 0, sizeof(sigAct));
	sigemptyset(&sigAct.sa_mask);
	sigAct.sa_handler = SIG_IGN;
	sigaction(SIGPIPE, &sigAct, NULL);

	setvbuf(stdout, buf, _IONBF, 48);

	if(argc != 5 && argc != 6) {
		printf("Invalid call of tcpflood\n");
		printf("Usage: tcpflood target-host target-port num-connections num-messages [initial msgnum]\n");
		exit(1);
	}
	
	targetIP = argv[1];
	targetPort = atoi(argv[2]);
	numConnections = atoi(argv[3]);
	numMsgsToSend = atoi(argv[4]);
	if(argc == 6)
		msgNum = atoi(argv[5]);

	if(openConnections() != 0) {
		printf("error opening connections\n");
		exit(1);
	}

	if(sendMessages() != 0) {
		printf("error sending messages\n");
		exit(1);
	}

	//closeConnections();
	printf("End of tcpflood Run\n");

	exit(ret);
}
