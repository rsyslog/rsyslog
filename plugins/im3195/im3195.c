/** 
 * rfc3195d.c
 * This is an RFC 3195 listener. All data received is forwarded to
 * local UNIX domain socket, where it can be picked up by a
 * syslog daemon (like rsyslogd ;)).
 *
 * \author  Rainer Gerhards <rgerhards@adiscon.com>
 *
 * Copyright 2003-2005 Rainer Gerhards and Adiscon GmbH.
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
#ifndef FEATURE_RFC3195
/* this is a trick: if RFC3195 is not to be supported, we just do an
 * error message.
 */
int main()
{
	fprintf(stderr, "error: not compiled with FEATURE_RFC3195 - terminating.\n");
	return(1);
}
#else
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include "rsyslog.h"
#include "liblogging.h"
#include "srAPI.h"
#include "syslogmessage.h"

/* configurable params! */
static char* pPathLogname = "/dev/log3195";
static char *PidFile;
static int NoFork = 0;
static int Debug = 0;
static int listenPort = 601;

/* we use a global API object below, because this listener is
 * not very complex. As such, this hack should not harm anything.
 * rgerhards, 2005-10-12
 */
static srAPIObj* pAPI;

static int	LogFile = -1;		/* fd for log */
static int	connected;		/* have done connect */
static struct sockaddr SyslogAddr;	/* AF_UNIX address of local logger */

/* small usage info */
static int usage()
{
	/* The following usage line is what we intend to have - it
	 * is commented out as a reminder. The one below is what we
	 * currently actually do...
	 fprintf(stderr, "usage: rfc3195d [-dv] [-i pidfile] [-n] [-p path]\n");
	 */
	fprintf(stderr, "usage: rfc3195d [-dv] [-r port] [-p path]\n");
	exit(1);
}

/* CLOSELOG -- close the system log
 */
static void closelog(void)
{
	close(LogFile);
	LogFile = -1;
	connected = 0;
}

/* OPENLOG -- open system log
 */
static void openlog()
{
	if (LogFile == -1) {
		SyslogAddr.sa_family = AF_UNIX;
		strncpy(SyslogAddr.sa_data, pPathLogname,
		    sizeof(SyslogAddr.sa_data));
		LogFile = socket(AF_UNIX, SOCK_DGRAM, 0);
		if(LogFile < 0) {
			char errStr[1024];
			printf("error opening '%s': %s\n", 
			       pPathLogname, rs_strerror_r(errno, errStr, sizeof(errStr)));
		}
	}
	if (LogFile != -1 && !connected &&
	    connect(LogFile, &SyslogAddr, sizeof(SyslogAddr.sa_family)+
			strlen(SyslogAddr.sa_data)) != -1)
		connected = 1;
	else {
		char errStr[1024];
		printf("error connecting '%s': %s\n", 
		       pPathLogname, rs_strerror_r(errno, errStr, sizeof(errStr)));
	}
}


/* This method is called when a message has been fully received.
 * It passes the received message to the specified unix domain
 * socket. Please note that this callback is synchronous, thus
 * liblogging will be on hold until it returns. This is important
 * to note because in an error case we might stay in this code
 * for an extended amount of time. So far, we think this is the
 * best solution, but real-world experience might tell us a
 * different truth ;)
 * rgerhards 2005-10-12
 */
void OnReceive(srAPIObj* pAPI, srSLMGObj* pSLMG)
{
	unsigned char *pszRawMsg;
	int iRetries; /* number of retries connecting to log socket */
	int iSleep;
	int iWriteOffset;
	ssize_t nToWrite;
	ssize_t nWritten;

	srSLMGGetRawMSG(pSLMG, &pszRawMsg);

	/* we need to loop writing the message. At least in
	 * theory, a single write might not send all data to the
	 * syslogd. So we need to continue until everything is written.
	 * Also, we need to check if there are any socket erros, in 
	 * which case we reconect. We will re-try indefinitely, if this
	 * is not acceptable, you need to change the code.
	 * rgerhards 2005-10-12
	 */
	iRetries = 0;
	nToWrite = strlen(pszRawMsg);
	iWriteOffset = 0;
	while(nToWrite != 0) {
		if(LogFile < 0 || !connected)
			openlog();
		if(LogFile < 0 || !connected) {
			/* still not connected, retry */
			if(iRetries > 0) {
				iSleep = (iRetries < 30) ? iRetries : 30;
				/* we sleep a little to prevent a thight loop */
				if(Debug)
					printf("multiple retries connecting to log socket"
					       " - doing sleep(%d)\n", iSleep);
				sleep(iSleep);
			}
			++iRetries;
		} else {
			nWritten = write(LogFile, pszRawMsg, strlen(pszRawMsg));
			if(nWritten < 0) {
				/* error, recover! */
				char errStr[1024];
				printf("error writing to domain socket: %s\r\n", rs_strerror_r(errno, errStr, sizeof(errStr)));
				closelog();
			} else {
				/* prepare for (potential) next write */
				nToWrite -= nWritten;
				iWriteOffset += nWritten;
			}
		}
	}

	if(Debug) {
		static int largest = 0;
		int sz = strlen(pszRawMsg);
		if(sz > largest)
			largest = sz;
		printf("Msg(%d/%d):%s\n\n", largest, sz, pszRawMsg);
	}
}


/* As we are single-threaded in this example, we need
 * one way to shut down the listener running on this
 * single thread. We use SIG_INT to do so - it effectively
 * provides a short-lived second thread ;-)
 */
void doShutdown(int i)
{
	printf("Shutting down rfc3195d. Be patient, this can take up to 30 seconds...\n");
	srAPIShutdownListener(pAPI);
}


/* on the the real program ;) */
int main(int argc, char* argv[])
{
	srRetVal iRet;
	int ch;
	struct sigaction sigAct;

	while ((ch = getopt(argc, argv, "di:np:r:v")) != EOF)
		switch((char)ch) {
		case 'd':		/* debug */
			Debug = 1;
			break;
		case 'i':		/* pid file name */
			PidFile = optarg;
			break;
		case 'n':		/* don't fork */
			NoFork = 1;
			break;
		case 'p':		/* path to regular log socket */
			pPathLogname = optarg;
			break;
		case 'r':		/* listen port */
			listenPort = atoi(optarg);
			if(listenPort < 1 || listenPort > 65535) {
				printf("Error: invalid listen port '%s', using 601 instead\n",
					optarg);
				listenPort = 601;
			}
			break;
		case 'v':
			printf("rfc3195d %s.%s (using liblogging version %d.%d.%d).\n",
			       VERSION, PATCHLEVEL,
			       LIBLOGGING_VERSION_MAJOR, LIBLOGGING_VERSION_MINOR,
			       LIBLOGGING_VERSION_SUBMINOR);
			printf("See http://www.rsyslog.com for more information.\n");
			exit(0);
		case '?':
		default:
			usage();
		}
	if ((argc -= optind))
		usage();

	memset(&sigAct, 0, sizeof(sigAct));
	sigemptyset(&sigAct.sa_mask);
	sigAct.sa_handler = doShutdown;
	sigaction(SIGUSR1, &sigAct, NULL);
	sigaction(SIGTERM, &sigAct, NULL);

	if(!Debug)
	{
		sigAct.sa_handler = SIG_IGN;
		sigaction(SIGINT, &sigAct, NULL);
	}

	if((pAPI = srAPIInitLib()) == NULL)
	{
		printf("Error initializing liblogging - aborting!\n");
		exit(1);
	}

	if((iRet = srAPISetOption(pAPI, srOPTION_BEEP_LISTENPORT, listenPort)) != SR_RET_OK)
	{
		printf("Error %d setting listen port - aborting\n", iRet);
		exit(100);
	}

	if((iRet = srAPISetupListener(pAPI, OnReceive)) != SR_RET_OK)
	{
		printf("Error %d setting up listener - aborting\n", iRet);
		exit(101);
	}

	/* now move the listener to running state. Control will only
	 * return after SIGUSR1.
	 */
	if((iRet = srAPIRunListener(pAPI)) != SR_RET_OK)
	{
		printf("Error %d running the listener - aborting\n", iRet);
		exit(102);
	}

	/** control will reach this point after shutdown */

	srAPIExitLib(pAPI);
	return 0;
}
#endif /* #ifndef FEATURE_RFC3195 - main wrapper */

/*
 * vi:set ai:
 */
