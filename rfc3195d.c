/** 
 * testsrvr.cpp : This is a small sample C++ server app
 * using liblogging. It just demonstrates how things can be 
 * done. It accepts incoming messages and just dumps them
 * to stdout. It is single-threaded.
 *
 * \author  Rainer Gerhards <rgerhards@adiscon.com>
 * \date    2003-08-13
 *          file created.
 *
 * Copyright 2003 
 *     Rainer Gerhards and Adiscon GmbH. All Rights Reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 * 
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 * 
 *     * Neither the name of Adiscon GmbH or Rainer Gerhards
 *       nor the names of its contributors may be used to
 *       endorse or promote products derived from this software without
 *       specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include "rsyslog.h"
#include "liblogging.h"
#include "srAPI.h"
#include "syslogmessage.h"

/* configurable params! */
#define	_PATH_LOGNAME	"/dev/log"

/* quick hack, so all can access it. Do NOT do this in your server ;-) */
static srAPIObj* pAPI;

static int	LogFile = -1;		/* fd for log */
static int	connected;		/* have done connect */
static struct sockaddr SyslogAddr;	/* AF_UNIX address of local logger */

/*
 * OPENLOG -- open system log
 */
static void openlog()
{
	if (LogFile == -1) {
		SyslogAddr.sa_family = AF_UNIX;
		strncpy(SyslogAddr.sa_data, _PATH_LOGNAME,
		    sizeof(SyslogAddr.sa_data));
		LogFile = socket(AF_UNIX, SOCK_DGRAM, 0);
	}
	if (LogFile != -1 && !connected &&
	    connect(LogFile, &SyslogAddr, sizeof(SyslogAddr.sa_family)+
			strlen(SyslogAddr.sa_data)) != -1)
		connected = 1;
}

/* This method is called when a message has been fully received.
 * In a real sample, you would do all your enqueuing and/or
 * processing here.
 * 
 * It is highly recommended that no lengthy processing is done in
 * this callback. Please see \ref architecture.c for a suggested
 * threading model.
 */
void OnReceive(srAPIObj* pAPI, srSLMGObj* pSLMG)
{
	unsigned char *pszRawMsg;

	srSLMGGetRawMSG(pSLMG, &pszRawMsg);

	if (LogFile < 0 || !connected)
		openlog();

	/* output the message to the local logger */
	write(LogFile, pszRawMsg, strlen(pszRawMsg));

	printf("RAW:%s\n\n", pszRawMsg);
}


/* As we are single-threaded in this example, we need
 * one way to shut down the listener running on this
 * single thread. We use SIG_INT to do so - it effectively
 * provides a short-lived second thread ;-)
 */
void doSIGINT(int i)
{
	printf("SIG_INT - shutting down listener. Be patient, can take up to 30 seconds...\n");
	srAPIShutdownListener(pAPI);
}

int main(int argc, char* argv[])
{
	srRetVal iRet;

#	ifdef	WIN32
		_CrtSetDbgFlag(_CRTDBG_CHECK_ALWAYS_DF | _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#	endif

	printf("testsrvr test server - just a quick debuging aid and sample....\n");
	printf("Compiled with liblogging version %d.%d.%d.\n", LIBLOGGING_VERSION_MAJOR, LIBLOGGING_VERSION_MINOR, LIBLOGGING_VERSION_SUBMINOR);
	printf("See http://www.monitorware.com/liblogging/ for updates.\n");
	printf("Listening for incoming requests....\n");

	signal(SIGINT, doSIGINT);

	if((pAPI = srAPIInitLib()) == NULL)
	{
		printf("Error initializing lib!\n");
		exit(1);
	}

	if((iRet = srAPISetupListener(pAPI, OnReceive)) != SR_RET_OK)
	{
		printf("Error %d setting up listener!\n", iRet);
		exit(100);
	}

	/* now move the listener to running state. Control will only
	 * return after SIG_INT.
	 */
	if((iRet = srAPIRunListener(pAPI)) != SR_RET_OK)
	{
		printf("Error %d running the listener!\n", iRet);
		exit(100);
	}

	/** control will reach this point after SIG_INT */

	srAPIExitLib(pAPI);
	return 0;
}

