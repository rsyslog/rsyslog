/* omprog.c
 * This output plugin enables rsyslog to execute a program and
 * feed it the message stream as standard input.
 *
 * NOTE: read comments in module-template.h for more specifics!
 *
 * File begun on 2009-04-01 by RGerhards
 *
 * Copyright 2009-2020 Adiscon GmbH.
 *
 * This file is part of rsyslog.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *       -or-
 *       see COPYING.ASL20 in the source distribution
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "config.h"
#include <stdio.h>
#include <syslog.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <pthread.h>
#include <poll.h>

#include "rsyslog.h"
#include "conf.h"
#include "syslogd-types.h"
#include "srUtils.h"
#include "template.h"
#include "module-template.h"
#include "errmsg.h"
#include "cfsysline.h"
#include "glbl.h"
#include "rsconf.h"

MODULE_TYPE_OUTPUT;
MODULE_TYPE_NOKEEP;
MODULE_CNFNAME("omprog")

extern char **environ; /* POSIX environment ptr, by std not in a header... (see man 7 environ) */

/* internal structures
 */
DEF_OMOD_STATIC_DATA;

#define NO_HUP_FORWARD -1	/* indicates that HUP should NOT be forwarded */
#define DEFAULT_CONFIRM_TIMEOUT_MS 10000
#define DEFAULT_CLOSE_TIMEOUT_MS 5000
#define RESPONSE_LINE_BUFFER_SIZE 4096
#define OUTPUT_CAPTURE_BUFFER_SIZE 4096
#define MAX_FD_TO_CLOSE 65535

typedef struct childProcessCtx {
	int bIsRunning;		/* is the program running? (if 0, next fields are uninitialized) */
	pid_t pid;			/* pid of currently running child process */
	int fdPipeOut;		/* fd for sending messages to the program */
	int fdPipeIn;		/* fd for receiving status messages from the program, or -1 */
} childProcessCtx_t;

typedef struct outputCaptureCtx {
	uchar *szFileName;		/* name of file to write the program output to, or NULL */
	mode_t fCreateMode;		/* output file creation permissions */
	pthread_mutex_t mutStart;	/* mutex for starting the output-capture thread */
	pthread_mutex_t mutWrite;	/* mutex for reopening the output file on HUP while being written */
	pthread_mutex_t mutTerm;	/* mutex for signaling the termination of the thread */
	pthread_cond_t condTerm;	/* condition for signaling the termination of the thread */
	int bIsRunning;			/* is the thread running? (if 0, next fields are uninitialized) */
	pthread_t thrdID;		/* ID of the output-capture thread */
	int fdPipe[2];			/* pipe for capturing the output of the child processes */
	int fdFile;				/* fd of the output file (-1 if it could not be opened) */
	int bFileErr;			/* file open error occurred? (to avoid reporting too many errors) */
	int bReadErr;			/* read error occurred? (to avoid reporting too many errors) */
	int bWriteErr;			/* write error occurred? (to avoid reporting too many errors) */
} outputCaptureCtx_t;

typedef struct _instanceData {
	uchar *szBinary;		/* name of external program to call */
	char **aParams;			/* optional parameters to pass to external program */
	int iParams;			/* holds the count of parameters if set */
	uchar *szTemplateName;	/* assigned output template */
	int bConfirmMessages;	/* does the program provide feedback via stdout? */
	long lConfirmTimeout;	/* how long to wait for feedback from the program (ms) */
	int bReportFailures;	/* report failures returned by the program as warning logs? */
	int bUseTransactions;	/* send begin/end transaction marks to program? */
	uchar *szBeginTransactionMark;	/* mark message for begin transaction */
	uchar *szCommitTransactionMark;	/* mark message for commit transaction */
	int iHUPForward;		/* signal to forward on HUP (or NO_HUP_FORWARD) */
	int bSignalOnClose;		/* should send SIGTERM to program before closing pipe? */
	long lCloseTimeout;		/* how long to wait for program to terminate after closing pipe (ms) */
	int bKillUnresponsive;	/* should send SIGKILL if closeTimeout is reached? */
	int bForceSingleInst;	/* start only one instance of program, even with multiple workers? */
	childProcessCtx_t *pSingleChildCtx;		/* child process context when bForceSingleInst=true */
	pthread_mutex_t *pSingleChildMut;		/* mutex for interacting with single child process */
	outputCaptureCtx_t *pOutputCaptureCtx;	/* settings and state for the output capture thread */
	time_t block_if_err;			/* time until which interface error is not to be shown */
} instanceData;

typedef struct wrkrInstanceData {
	instanceData *pData;
	childProcessCtx_t *pChildCtx;	/* child process context (can be equal to pSingleChildCtx) */
} wrkrInstanceData_t;

typedef struct configSettings_s {
	uchar *szBinary;	/* name of external program to call */
} configSettings_t;
static configSettings_t cs;

/* tables for interfacing with the v6 config system */
/* action (instance) parameters */
static struct cnfparamdescr actpdescr[] = {
	{ "binary", eCmdHdlrString, CNFPARAM_REQUIRED },
	{ "confirmMessages", eCmdHdlrBinary, 0 },
	{ "confirmTimeout", eCmdHdlrInt, 0 },
	{ "reportFailures", eCmdHdlrBinary, 0 },
	{ "useTransactions", eCmdHdlrBinary, 0 },
	{ "beginTransactionMark", eCmdHdlrString, 0 },
	{ "commitTransactionMark", eCmdHdlrString, 0 },
	{ "forceSingleInstance", eCmdHdlrBinary, 0 },
	{ "hup.signal", eCmdHdlrGetWord, 0 },
	{ "template", eCmdHdlrGetWord, 0 },
	{ "signalOnClose", eCmdHdlrBinary, 0 },
	{ "closeTimeout", eCmdHdlrInt, 0 },
	{ "killUnresponsive", eCmdHdlrBinary, 0 },
	{ "output", eCmdHdlrString, 0 },
	{ "fileCreateMode", eCmdHdlrFileCreateMode, 0 }
};

static struct cnfparamblk actpblk =
	{ CNFPARAMBLK_VERSION,
	  sizeof(actpdescr)/sizeof(struct cnfparamdescr),
	  actpdescr
	};

/* execute the external program (must be called in child context after fork).
 */
static __attribute__((noreturn)) void
execBinary(const instanceData *pData, int fdStdin, int fdStdout)
{
	int fdOutput, maxFd, fd, sigNum;
	struct sigaction sigAct;
	sigset_t sigSet;
	char errStr[1024];

	if(dup2(fdStdin, STDIN_FILENO) == -1) {
		goto failed;
	}

	if(pData->pOutputCaptureCtx != NULL) {
		fdOutput = pData->pOutputCaptureCtx->fdPipe[1];
	} else {
		fdOutput = open("/dev/null", O_WRONLY);
		if(fdOutput == -1) {
			goto failed;
		}
	}

	if(fdStdout != -1) {
		/* confirmMessages enabled: redirect stdout to parent via pipe. After
		 * this point, anything written to the child's stdout will be treated
		 * by omprog as initialization feedback (see startChild). This
		 * includes debug messages (DBGPRINTF) when in debug mode. So we
		 * cannot use DBGPRINTF from this point on, except for error cases.
		 */
		if(dup2(fdStdout, STDOUT_FILENO) == -1) {
			goto failed;
		}
	} else {
		/* confirmMessages disabled: redirect stdout to file or /dev/null */
		if(dup2(fdOutput, STDOUT_FILENO) == -1) {
			goto failed;
		}
	}

	/* redirect stderr to file or /dev/null */
	if(dup2(fdOutput, STDERR_FILENO) == -1) {
		goto failed;
	}

	/* close the file handles the child process doesn't need (all above STDERR).
	 * The following way is simple and portable, though not perfect.
	 * See https://stackoverflow.com/a/918469 for alternatives.
	 */
	maxFd = sysconf(_SC_OPEN_MAX);
	if(maxFd < 0 || maxFd > MAX_FD_TO_CLOSE) {
		maxFd = MAX_FD_TO_CLOSE;
	}
#	ifdef VALGRIND
	else {  /* don't close valgrind reserved fds, to avoid warnings */
		maxFd -= 10;
	}
#	endif
	for(fd = STDERR_FILENO + 1 ; fd <= maxFd ; ++fd) {
		close(fd);
	}

	/* reset signal handlers to default */
	memset(&sigAct, 0, sizeof(sigAct));
	sigemptyset(&sigAct.sa_mask);
	sigAct.sa_handler = SIG_DFL;
	for(sigNum = 1 ; sigNum < NSIG ; ++sigNum) {
		sigaction(sigNum, &sigAct, NULL);
	}

	/* we need to block SIGINT, otherwise our program is cancelled when we are
	 * stopped in debug mode.
	 */
	sigAct.sa_handler = SIG_IGN;
	sigaction(SIGINT, &sigAct, NULL);
	sigemptyset(&sigSet);
	sigprocmask(SIG_SETMASK, &sigSet, NULL);

	alarm(0);

	/* finally exec program */
	execve((char*)pData->szBinary, pData->aParams, environ);

failed:
	/* an error occurred: log it and exit the child process. We use the
	 * 'syslog' system call to log the error (we cannot use LogMsg/LogError,
	 * since these functions add directly to the rsyslog input queue).
	 */
	rs_strerror_r(errno, errStr, sizeof(errStr));
	DBGPRINTF("omprog: failed to execute program '%s': %s\n",
			pData->szBinary, errStr);
	openlog("rsyslogd", 0, LOG_SYSLOG);
	syslog(LOG_ERR, "omprog: failed to execute program '%s': %s\n",
			pData->szBinary, errStr);
	exit(1);
}

/* creates a pipe and starts program, uses pipe as stdin for program.
 * rgerhards, 2009-04-01
 */
static rsRetVal
openPipe(instanceData *pData, childProcessCtx_t *pChildCtx)
{
	int pipeStdin[2] = { -1, -1 };
	int pipeStdout[2] = { -1, -1 };
	pid_t cpid;
	DEFiRet;

	/* open a pipe to send messages to the program */
	if(pipe(pipeStdin) == -1) {
		ABORT_FINALIZE(RS_RET_ERR_CREAT_PIPE);
	}

	/* if the 'confirmMessages' setting is enabled, open a pipe to receive
	   message confirmations from the program */
	if(pData->bConfirmMessages && pipe(pipeStdout) == -1) {
		ABORT_FINALIZE(RS_RET_ERR_CREAT_PIPE);
	}

	DBGPRINTF("omprog: executing program '%s' with '%d' parameters\n", pData->szBinary,
			pData->iParams);

	cpid = fork();
	if(cpid == -1) {
		ABORT_FINALIZE(RS_RET_ERR_FORK);
	}

	if(cpid == 0) {  /* we are now the child process: execute the program */
		/* close the pipe ends that the child doesn't need */
		close(pipeStdin[1]);
		if(pipeStdout[0] != -1) {
			close(pipeStdout[0]);
		}

		execBinary(pData, pipeStdin[0], pipeStdout[1]);
		/* NO CODE HERE - WILL NEVER BE REACHED! */
	}

	DBGPRINTF("omprog: child has pid %d\n", (int) cpid);

	/* close the pipe ends that the parent doesn't need */
	close(pipeStdin[0]);
	if(pipeStdout[1] != -1) {
		close(pipeStdout[1]);
	}

	pChildCtx->fdPipeOut = pipeStdin[1];  /* we'll send messages to the program via this fd */
	pChildCtx->fdPipeIn = pipeStdout[0];  /* we'll receive message confirmations via this fd */
	pChildCtx->pid = cpid;
	pChildCtx->bIsRunning = 1;

finalize_it:
	if(iRet != RS_RET_OK) {
		if(pipeStdin[0] != -1) {
			close(pipeStdin[0]);
			close(pipeStdin[1]);
		}
		if(pipeStdout[0] != -1) {
			close(pipeStdout[0]);
			close(pipeStdout[1]);
		}
	}
	RETiRet;
}

static void
waitForChild(instanceData *pData, childProcessCtx_t *pChildCtx)
{
	int status;
	int ret;
	long counter;

	counter = pData->lCloseTimeout / 10;
	while ((ret = waitpid(pChildCtx->pid, &status, WNOHANG)) == 0 && counter > 0) {
		srSleep(0, 10000);  /* 0 seconds, 10 milliseconds */
		--counter;
	}

	if (ret == 0) {  /* timeout reached */
		if (!pData->bKillUnresponsive) {
			LogMsg(0, NO_ERRCODE, LOG_WARNING, "omprog: program '%s' (pid %ld) did not terminate "
					"within timeout (%ld ms); ignoring it", pData->szBinary,
					(long) pChildCtx->pid, pData->lCloseTimeout);
			return;
		}

		LogMsg(0, NO_ERRCODE, LOG_WARNING, "omprog: program '%s' (pid %ld) did not terminate "
				"within timeout (%ld ms); killing it", pData->szBinary, (long) pChildCtx->pid,
				pData->lCloseTimeout);
		if (kill(pChildCtx->pid, SIGKILL) == -1) {
			LogError(errno, RS_RET_SYS_ERR, "omprog: could not send SIGKILL to child process");
			return;
		}

		ret = waitpid(pChildCtx->pid, &status, 0);
	}

	/* waitpid will fail with errno == ECHILD if the child process has already
	   been reaped by the rsyslogd main loop (see rsyslogd.c) */
	if(ret == pChildCtx->pid) {
		glblReportChildProcessExit(runConf, pData->szBinary, pChildCtx->pid, status);
	}
}

/* close pipe and wait for child to terminate
 */
static void
cleanupChild(instanceData *pData, childProcessCtx_t *pChildCtx)
{
	assert(pChildCtx->bIsRunning);

	if(pChildCtx->fdPipeIn != -1) {
		close(pChildCtx->fdPipeIn);
		pChildCtx->fdPipeIn = -1;
	}
	if(pChildCtx->fdPipeOut != -1) {
		close(pChildCtx->fdPipeOut);
		pChildCtx->fdPipeOut = -1;
	}

	/* wait for the child AFTER closing the pipe, so it receives EOF */
	waitForChild(pData, pChildCtx);

	pChildCtx->bIsRunning = 0;
}

/* Send SIGTERM to child process if configured to do so, close pipe
 * and wait for child to terminate.
 */
static void
terminateChild(instanceData *pData, childProcessCtx_t *pChildCtx)
{
	DBGPRINTF("terminateChild called\n");
	assert(pChildCtx->bIsRunning);

	if (pData->bSignalOnClose) {
		kill(pChildCtx->pid, SIGTERM);
	}

	cleanupChild(pData, pChildCtx);
}

/* write message to pipe
 * note that we do not try to run block-free. If the user fears something
 * may block (and this is not acceptable), the action should be run on its
 * own action queue.
 */
static rsRetVal
sendMessage(instanceData *pData, childProcessCtx_t *pChildCtx, const uchar *szMsg)
{
	ssize_t written;
	size_t offset = 0;
	DEFiRet;

	const size_t len = strlen((char*)szMsg);

	do {
		written = write(pChildCtx->fdPipeOut, ((char*)szMsg) + offset, len - offset);
		if(written == -1) {
			if(errno == EINTR) {
				continue;  /* call interrupted: retry write */
			}
			if(errno == EPIPE) {
				LogMsg(0, RS_RET_ERR_WRITE_PIPE, LOG_WARNING,
						"omprog: program '%s' (pid %ld) terminated; will be restarted",
						pData->szBinary, (long) pChildCtx->pid);
				cleanupChild(pData, pChildCtx);  /* force restart in tryResume() */
				ABORT_FINALIZE(RS_RET_SUSPENDED);
			}
			LogError(errno, RS_RET_ERR_WRITE_PIPE, "omprog: error sending message to program");
			ABORT_FINALIZE(RS_RET_SUSPENDED);
		}
		offset += written;
	} while(offset < len);

finalize_it:
	RETiRet;
}

static rsRetVal
lineToStatusCode(instanceData *pData, const char* line)
{
	DEFiRet;

	/* strip leading dots (.) from the line, so the program can use them as a keep-alive mechanism */
	while(line[0] == '.') {
		++line;
	}

	if(strcmp(line, "OK") == 0) {
		iRet = RS_RET_OK;
	} else if(strcmp(line, "DEFER_COMMIT") == 0) {
		iRet = RS_RET_DEFER_COMMIT;
	} else if(strcmp(line, "PREVIOUS_COMMITTED") == 0) {
		iRet = RS_RET_PREVIOUS_COMMITTED;
	} else {
		/* anything else is considered a recoverable error */
		DBGPRINTF("omprog: program '%s' returned: %s\n", pData->szBinary, line);
		if(pData->bReportFailures) {
			LogMsg(0, NO_ERRCODE, LOG_WARNING, "omprog: program '%s' returned: %s",
					pData->szBinary, line);
		}
		iRet = RS_RET_SUSPENDED;
	}
	RETiRet;
}

static rsRetVal
readStatus(instanceData *pData, childProcessCtx_t *pChildCtx)
{
	struct pollfd fdToPoll[1];
	int numReady;
	char lineBuf[RESPONSE_LINE_BUFFER_SIZE];
	ssize_t lenRead;
	size_t offset = 0;
	int lineEnded = 0;
	DEFiRet;

	fdToPoll[0].fd = pChildCtx->fdPipeIn;
	fdToPoll[0].events = POLLIN;

	do {
		numReady = poll(fdToPoll, 1, pData->lConfirmTimeout);
		if(numReady == -1) {
			if(errno == EINTR) {
				continue;  /* call interrupted: retry poll */
			}
			LogError(errno, RS_RET_SYS_ERR, "omprog: error polling for response from program");
			ABORT_FINALIZE(RS_RET_SUSPENDED);
		}

		if(numReady == 0) {  /* timeout reached */
			LogMsg(0, RS_RET_TIMED_OUT, LOG_WARNING, "omprog: program '%s' (pid %ld) did not respond "
					"within timeout (%ld ms); will be restarted", pData->szBinary,
					(long) pChildCtx->pid, pData->lConfirmTimeout);
			terminateChild(pData, pChildCtx);
			ABORT_FINALIZE(RS_RET_SUSPENDED);
		}

		lenRead = read(pChildCtx->fdPipeIn, lineBuf + offset, sizeof(lineBuf) - offset - 1);
		if(lenRead == -1) {
			if(errno == EINTR) {
				continue;  /* call interrupted: retry poll + read */
			}
			LogError(errno, RS_RET_READ_ERR, "omprog: error reading response from program");
			ABORT_FINALIZE(RS_RET_SUSPENDED);
		}

		if(lenRead == 0) {
			LogMsg(0, RS_RET_READ_ERR, LOG_WARNING, "omprog: program '%s' (pid %ld) terminated; "
					"will be restarted", pData->szBinary, (long) pChildCtx->pid);
			cleanupChild(pData, pChildCtx);
			ABORT_FINALIZE(RS_RET_SUSPENDED);
		}

		offset += lenRead;
		lineBuf[offset] = '\0';
		lineEnded = (lineBuf[offset-1] == '\n');

		/* check that the program has not returned multiple lines. This should not occur if
		 * the program honors the specified interface. Otherwise, we force a restart of the
		 * program, since we have probably lost synchronism with it.
		 */
		if(!lineEnded && strchr(lineBuf + offset - lenRead, '\n') != NULL) {
			DBGPRINTF("omprog: program '%s' returned: %s\n", pData->szBinary, lineBuf);
			LogMsg(0, NO_ERRCODE, LOG_WARNING, "omprog: program '%s' returned a multiline response; "
					"will be restarted", pData->szBinary);
			if(pData->bReportFailures) {
				LogMsg(0, NO_ERRCODE, LOG_WARNING, "omprog: program '%s' returned: %s",
						pData->szBinary, lineBuf);
			}
			terminateChild(pData, pChildCtx);
			ABORT_FINALIZE(RS_RET_SUSPENDED);
		}
	} while(!lineEnded && offset < sizeof(lineBuf) - 1);

	if(!lineEnded) {
		DBGPRINTF("omprog: program '%s' returned: %s\n", pData->szBinary, lineBuf);
		LogMsg(0, NO_ERRCODE, LOG_WARNING, "omprog: program '%s' returned a too long response; "
				"will be restarted", pData->szBinary);
		if(pData->bReportFailures) {
			LogMsg(0, NO_ERRCODE, LOG_WARNING, "omprog: program '%s' returned: %s",
					pData->szBinary, lineBuf);
		}
		terminateChild(pData, pChildCtx);
		ABORT_FINALIZE(RS_RET_SUSPENDED);
	}

	lineBuf[offset-1] = '\0';  /* strip newline char */

	/* NOTE: coverity does not like CHKiRet() if it is the last thing before finalize_it.
	 * Reason is that the if() inside that macro than does not lead to different code paths.
	 */
	iRet = lineToStatusCode(pData, lineBuf);

finalize_it:
	RETiRet;
}

static rsRetVal
allocChildCtx(childProcessCtx_t **ppChildCtx)
{
	childProcessCtx_t *pChildCtx;
	DEFiRet;

	CHKmalloc(pChildCtx = calloc(1, sizeof(childProcessCtx_t)));
	*ppChildCtx = pChildCtx;

	pChildCtx->bIsRunning = 0;
	pChildCtx->pid = -1;
	pChildCtx->fdPipeOut = -1;
	pChildCtx->fdPipeIn = -1;

finalize_it:
	RETiRet;
}

static rsRetVal
startChild(instanceData *pData, childProcessCtx_t *pChildCtx)
{
	DEFiRet;

	assert(!pChildCtx->bIsRunning);

	CHKiRet(openPipe(pData, pChildCtx));

	if(pData->bConfirmMessages) {
		/* wait for program to confirm successful initialization */
		CHKiRet(readStatus(pData, pChildCtx));
	}

finalize_it:
	if(iRet != RS_RET_OK && pChildCtx->bIsRunning) {
		/* if initialization has failed, terminate program */
		terminateChild(pData, pChildCtx);
	}
	RETiRet;
}

static rsRetVal
startSingleChildOnce(instanceData *pData)
{
	DEFiRet;

	assert(pData->bForceSingleInst);
	CHKiConcCtrl(pthread_mutex_lock(pData->pSingleChildMut));

	if(pData->pSingleChildCtx->bIsRunning)
		goto finalize_it;  /* child process already started: nothing to do */

	iRet = startChild(pData, pData->pSingleChildCtx);

finalize_it:
	pthread_mutex_unlock(pData->pSingleChildMut);
	RETiRet;
}

static void
writeOutputToFile(outputCaptureCtx_t *pCtx, char *buf, ssize_t len)
{
	ssize_t written;
	ssize_t offset = 0;

	assert(pCtx->bIsRunning);
	pthread_mutex_lock(&pCtx->mutWrite);

	if(pCtx->fdFile == -1) {
		if(pCtx->bFileErr) {  /* discarding output because file couldn't be opened */
			goto done;
		}

		pCtx->fdFile = open((char*)pCtx->szFileName, O_WRONLY | O_APPEND | O_CREAT,
				pCtx->fCreateMode);
		if(pCtx->fdFile == -1) {
			LogError(errno, RS_RET_NO_FILE_ACCESS, "omprog: error opening output file %s; "
					"output from program will be discarded", pCtx->szFileName);
			pCtx->bFileErr = 1;  /* avoid reporting too many errors */
			goto done;
		}
	}

	do {
		written = write(pCtx->fdFile, buf + offset, len - offset);
		if(written == -1) {
			if(errno == EINTR) {
				continue;  /* call interrupted: retry write */
			}

			if(!pCtx->bWriteErr) {
				LogError(errno, RS_RET_SYS_ERR, "omprog: error writing to output file "
						"(subsequent errors will not be reported)");
				pCtx->bWriteErr = 1;  /* avoid reporting too many errors */
			}
			break;
		}

		if(pCtx->bWriteErr) {
			LogMsg(0, NO_ERRCODE, LOG_WARNING, "omprog: resumed writing to output file");
			pCtx->bWriteErr = 0;
		}

		offset += written;
	} while(offset < len);

done:
	pthread_mutex_unlock(&pCtx->mutWrite);
}

static void
closeOutputFile(outputCaptureCtx_t *pCtx)
{
	DBGPRINTF("omprog: reopening output file upon reception of HUP signal\n");
	pthread_mutex_lock(&pCtx->mutWrite);

	if(pCtx->fdFile != -1) {
		close(pCtx->fdFile);
		pCtx->fdFile = -1;
	}
	pCtx->bFileErr = 0;  /* if there was an error opening the file, we'll retry */

	pthread_mutex_unlock(&pCtx->mutWrite);
}

/* This code runs in a dedicated thread. Captures the output of the child processes
 * through a shared pipe (one reader and multiple writers), and writes the output
 * to a file. The lines concurrently emmitted to stdout/stderr by the child processes
 * will not appear intermingled in the output file if 1) the lines are short enough
 * (less than PIPE_BUF bytes long: 4KB on Linux, and 512 bytes or more on other
 * POSIX systems), and 2) the program outputs each line using a single 'write'
 * syscall (line buffering mode). When a HUP signal is received, the output file is
 * reopened (this provides support for external rotation of the file).
 */
static void *
captureOutput(void *_pCtx) {
	outputCaptureCtx_t *pCtx = (outputCaptureCtx_t *)_pCtx;
	sigset_t sigSet;
	char readBuf[OUTPUT_CAPTURE_BUFFER_SIZE];
	ssize_t lenRead;

	DBGPRINTF("omprog: starting output capture thread\n");

	/* block signals for this thread (otherwise shutdown hangs on FreeBSD) */
	sigfillset(&sigSet);
	pthread_sigmask(SIG_SETMASK, &sigSet, NULL);

	for(;;) {
		lenRead = read(pCtx->fdPipe[0], readBuf, sizeof(readBuf));
		if(lenRead == -1) {
			if(errno == EINTR) {
				continue;  /* call interrupted: retry read */
			}

			if(!pCtx->bReadErr) {
				LogError(errno, RS_RET_SYS_ERR, "omprog: error capturing output from program "
						"(subsequent errors will not be reported)");
				pCtx->bReadErr = 1;  /* avoid reporting too many errors */
			}
			continue;  /* continue with next line */
		}

		if(lenRead == 0) {
			break;  /* all write ends of pipe closed: exit loop and terminate thread */
		}

		if(pCtx->bReadErr) {
			LogMsg(0, NO_ERRCODE, LOG_WARNING, "omprog: resumed capturing output from program");
			pCtx->bReadErr = 0;
		}

		writeOutputToFile(pCtx, readBuf, lenRead);
	}

	DBGPRINTF("omprog: all output-capture pipe ends closed, terminating output capture thread\n");
	pthread_mutex_lock(&pCtx->mutTerm);
	pCtx->bIsRunning = 0;
	pthread_cond_signal(&pCtx->condTerm);
	pthread_mutex_unlock(&pCtx->mutTerm);
	return NULL;
}

static rsRetVal
allocOutputCaptureCtx(outputCaptureCtx_t **ppCtx)
{
	outputCaptureCtx_t *pCtx;
	DEFiRet;

	CHKmalloc(pCtx = calloc(1, sizeof(outputCaptureCtx_t)));
	*ppCtx = pCtx;

	pCtx->szFileName = NULL;
	pCtx->fCreateMode = 0600;
	pCtx->bIsRunning = 0;

	CHKiConcCtrl(pthread_mutex_init(&pCtx->mutStart, NULL));
	CHKiConcCtrl(pthread_mutex_init(&pCtx->mutWrite, NULL));
	CHKiConcCtrl(pthread_mutex_init(&pCtx->mutTerm, NULL));
	CHKiConcCtrl(pthread_cond_init(&pCtx->condTerm, NULL));

finalize_it:
	RETiRet;
}

static void
freeOutputCaptureCtx(outputCaptureCtx_t *pCtx) {
	if(pCtx->szFileName != NULL) {
		free(pCtx->szFileName);
	}

	pthread_cond_destroy(&pCtx->condTerm);
	pthread_mutex_destroy(&pCtx->mutTerm);
	pthread_mutex_destroy(&pCtx->mutWrite);
	pthread_mutex_destroy(&pCtx->mutStart);

	free(pCtx);
}

static rsRetVal
startOutputCaptureOnce(outputCaptureCtx_t *pCtx)
{
	int pip[2] = { -1, -1 };
	DEFiRet;

	CHKiConcCtrl(pthread_mutex_lock(&pCtx->mutStart));

	if(pCtx->bIsRunning)
		goto finalize_it;  /* output capture thread already started: nothing to do */

	/* open a (single) pipe to capture output from (all) child processes */
	if(pipe(pip) == -1) {
		ABORT_FINALIZE(RS_RET_ERR_CREAT_PIPE);
	}

	pCtx->fdPipe[0] = pip[0];
	pCtx->fdPipe[1] = pip[1];
	pCtx->fdFile = -1;
	pCtx->bFileErr = 0;
	pCtx->bReadErr = 0;
	pCtx->bWriteErr = 0;

	/* start a thread to read lines from the pipe and write them to the output file */
	CHKiConcCtrl(pthread_create(&pCtx->thrdID, NULL, captureOutput, (void *)pCtx));

	pCtx->bIsRunning = 1;

finalize_it:
	if(iRet != RS_RET_OK && pip[0] != -1) {
		close(pip[0]);
		close(pip[1]);
	}
	pthread_mutex_unlock(&pCtx->mutStart);
	RETiRet;
}

static void
endOutputCapture(outputCaptureCtx_t *pCtx, long timeoutMs)
{
	struct timespec ts;
	int bTimedOut;

	assert(pCtx->bIsRunning);

	/* close our write end of the output-capture pipe */
	close(pCtx->fdPipe[1]);

	/* the output capture thread will now terminate because there are no more
	 * writers attached to the output-capture pipe. However, if a child becomes
	 * unresponsive without closing its pipe end (assuming killUnresponsive=off),
	 * we would wait forever. To avoid this, we wait for the thread to terminate
	 * during a maximum timeout (we reuse the 'closeTimeout' setting for this).
	 */
	timeoutComp(&ts, timeoutMs);
	pthread_mutex_lock(&pCtx->mutTerm);
	bTimedOut = 0;
	while(pCtx->bIsRunning && !bTimedOut) {
		if(pthread_cond_timedwait(&pCtx->condTerm, &pCtx->mutTerm, &ts) == ETIMEDOUT) {
			bTimedOut = 1;
		}
	}
	pthread_mutex_unlock(&pCtx->mutTerm);

	if(bTimedOut) {
		LogMsg(0, NO_ERRCODE, LOG_WARNING, "omprog: forcing termination of output capture "
				"thread because of unresponsive child process");
		pthread_cancel(pCtx->thrdID);
		pCtx->bIsRunning = 0;
	}

	pthread_join(pCtx->thrdID, NULL);

	/* close the read end of the output-capture pipe */
	close(pCtx->fdPipe[0]);

	/* close the output file (if it could be opened) */
	if(pCtx->fdFile != -1) {
		close(pCtx->fdFile);
	}
}


BEGINinitConfVars		/* (re)set config variables to default values */
CODESTARTinitConfVars;
	cs.szBinary = NULL;	/* name of binary to call */
ENDinitConfVars


BEGINcreateInstance
CODESTARTcreateInstance;
	pData->szBinary = NULL;
	pData->szTemplateName = NULL;
	pData->aParams = NULL;
	pData->iParams = 0;
	pData->bConfirmMessages = 0;
	pData->block_if_err = 0;
	pData->lConfirmTimeout = DEFAULT_CONFIRM_TIMEOUT_MS;
	pData->bReportFailures = 0;
	pData->bUseTransactions = 0;
	pData->szBeginTransactionMark = NULL;
	pData->szCommitTransactionMark = NULL;
	pData->iHUPForward = NO_HUP_FORWARD;
	pData->bSignalOnClose = 0;
	pData->lCloseTimeout = DEFAULT_CLOSE_TIMEOUT_MS;
	pData->bKillUnresponsive = -1;
	pData->bForceSingleInst = 0;
	pData->pSingleChildCtx = NULL;
	pData->pSingleChildMut = NULL;
	pData->pOutputCaptureCtx = NULL;
ENDcreateInstance


static rsRetVal
postInitInstance(instanceData *pData)
{
	DEFiRet;

	if(pData->bUseTransactions && pData->szBeginTransactionMark == NULL) {
		pData->szBeginTransactionMark = (uchar*)strdup("BEGIN TRANSACTION");
	}
	if(pData->bUseTransactions && pData->szCommitTransactionMark == NULL) {
		pData->szCommitTransactionMark = (uchar*)strdup("COMMIT TRANSACTION");
	}
	if(pData->bKillUnresponsive == -1) {  /* default value: bSignalOnClose */
		pData->bKillUnresponsive = pData->bSignalOnClose;
	}

	if(pData->pOutputCaptureCtx != NULL && pData->pOutputCaptureCtx->szFileName == NULL) {
		LogError(0, RS_RET_CONF_PARAM_INVLD, "omprog: the 'fileCreateMode' parameter requires "
				"specifying the 'output' parameter also");
		ABORT_FINALIZE(RS_RET_CONF_PARAM_INVLD);
	}

	if(pData->bForceSingleInst) {
		CHKmalloc(pData->pSingleChildMut = calloc(1, sizeof(pthread_mutex_t)));
		CHKiConcCtrl(pthread_mutex_init(pData->pSingleChildMut, NULL));
		CHKiRet(allocChildCtx(&pData->pSingleChildCtx));
		/* do not start the child here. The config is still being parsed, and the daemon
		   has not been forked yet. When the daemon is forked, all fds will be closed! */
	}

finalize_it:
	/* no cleanup needed on error: newActInst() will call freeInstance() */
	RETiRet;
}


BEGINcreateWrkrInstance
CODESTARTcreateWrkrInstance;
	pWrkrData->pChildCtx = NULL;

	if(pWrkrData->pData->pOutputCaptureCtx != NULL) {
		CHKiRet(startOutputCaptureOnce(pWrkrData->pData->pOutputCaptureCtx));
	}

	if(pWrkrData->pData->bForceSingleInst) {
		CHKiRet(startSingleChildOnce(pWrkrData->pData));
		pWrkrData->pChildCtx = pData->pSingleChildCtx;
	} else {
		CHKiRet(allocChildCtx(&pWrkrData->pChildCtx));
		CHKiRet(startChild(pWrkrData->pData, pWrkrData->pChildCtx));
	}

finalize_it:
ENDcreateWrkrInstance


BEGINisCompatibleWithFeature
CODESTARTisCompatibleWithFeature;
	if(eFeat == sFEATURERepeatedMsgReduction) {
		iRet = RS_RET_OK;
	}
ENDisCompatibleWithFeature


BEGINdbgPrintInstInfo
CODESTARTdbgPrintInstInfo;
ENDdbgPrintInstInfo


BEGINtryResume
CODESTARTtryResume;
	if(pWrkrData->pData->bForceSingleInst) {
		CHKiConcCtrl(pthread_mutex_lock(pWrkrData->pData->pSingleChildMut));
	}
	if(!pWrkrData->pChildCtx->bIsRunning) {
		CHKiRet(startChild(pWrkrData->pData, pWrkrData->pChildCtx));
	}

finalize_it:
	if(pWrkrData->pData->bForceSingleInst) {
		pthread_mutex_unlock(pWrkrData->pData->pSingleChildMut);
	}
ENDtryResume


BEGINbeginTransaction
CODESTARTbeginTransaction;
	if(pWrkrData->pData->bForceSingleInst) {
		CHKiConcCtrl(pthread_mutex_lock(pWrkrData->pData->pSingleChildMut));
	}
	if(!pWrkrData->pData->bUseTransactions) {
		FINALIZE;
	}

	CHKiRet(sendMessage(pWrkrData->pData, pWrkrData->pChildCtx,
			pWrkrData->pData->szBeginTransactionMark));
	CHKiRet(sendMessage(pWrkrData->pData, pWrkrData->pChildCtx, (uchar*) "\n"));

	if(pWrkrData->pData->bConfirmMessages) {
		CHKiRet(readStatus(pWrkrData->pData, pWrkrData->pChildCtx));
	}

finalize_it:
	if(pWrkrData->pData->bForceSingleInst) {
		pthread_mutex_unlock(pWrkrData->pData->pSingleChildMut);
	}
ENDbeginTransaction


BEGINdoAction
CODESTARTdoAction;
	if(pWrkrData->pData->bForceSingleInst) {
		CHKiConcCtrl(pthread_mutex_lock(pWrkrData->pData->pSingleChildMut));
	}
	if(!pWrkrData->pChildCtx->bIsRunning) {  /* should not occur */
		ABORT_FINALIZE(RS_RET_SUSPENDED);
	}

	const uchar *const szMsg = ppString[0];
	const size_t len = strlen((char*)szMsg);
	CHKiRet(sendMessage(pWrkrData->pData, pWrkrData->pChildCtx, szMsg));
	if(szMsg[len-1] != '\n') {
		const time_t tt = time(NULL);
		if(tt > pWrkrData->pData->block_if_err) {
			LogMsg(0, NO_ERRCODE, LOG_WARNING, "omprog: messages must be terminated with \\n "
				"at end of message, but this message is not: '%s'\n", ppString[0]);
			pWrkrData->pData->block_if_err = tt + 30;
		}
		CHKiRet(sendMessage(pWrkrData->pData, pWrkrData->pChildCtx, (uchar*) "\n"));
	}

	if(pWrkrData->pData->bConfirmMessages) {
		CHKiRet(readStatus(pWrkrData->pData, pWrkrData->pChildCtx));
	} else if(pWrkrData->pData->bUseTransactions) {
		/* ensure endTransaction will be called */
		iRet = RS_RET_DEFER_COMMIT;
	}

finalize_it:
	if(pWrkrData->pData->bForceSingleInst) {
		pthread_mutex_unlock(pWrkrData->pData->pSingleChildMut);
	}
ENDdoAction


BEGINendTransaction
CODESTARTendTransaction;
	if(pWrkrData->pData->bForceSingleInst) {
		CHKiConcCtrl(pthread_mutex_lock(pWrkrData->pData->pSingleChildMut));
	}
	if(!pWrkrData->pData->bUseTransactions) {
		FINALIZE;
	}

	CHKiRet(sendMessage(pWrkrData->pData, pWrkrData->pChildCtx,
			pWrkrData->pData->szCommitTransactionMark));
	CHKiRet(sendMessage(pWrkrData->pData, pWrkrData->pChildCtx, (uchar*) "\n"));

	if(pWrkrData->pData->bConfirmMessages) {
		CHKiRet(readStatus(pWrkrData->pData, pWrkrData->pChildCtx));
	}

finalize_it:
	if(pWrkrData->pData->bForceSingleInst) {
		pthread_mutex_unlock(pWrkrData->pData->pSingleChildMut);
	}
ENDendTransaction


BEGINfreeWrkrInstance
CODESTARTfreeWrkrInstance;
	if(!pWrkrData->pData->bForceSingleInst) {
		if(pWrkrData->pChildCtx->bIsRunning) {
			terminateChild(pWrkrData->pData, pWrkrData->pChildCtx);
		}
		free(pWrkrData->pChildCtx);
	}
ENDfreeWrkrInstance


BEGINfreeInstance
	int i;
CODESTARTfreeInstance;
	if(pData->pSingleChildCtx != NULL) {
		if(pData->pSingleChildCtx->bIsRunning) {
			terminateChild(pData, pData->pSingleChildCtx);
		}
		free(pData->pSingleChildCtx);
	}

	if(pData->pSingleChildMut != NULL) {
		pthread_mutex_destroy(pData->pSingleChildMut);
		free(pData->pSingleChildMut);
	}

	if(pData->pOutputCaptureCtx != NULL) {
		if(pData->pOutputCaptureCtx->bIsRunning) {
			endOutputCapture(pData->pOutputCaptureCtx, pData->lCloseTimeout);
		}
		freeOutputCaptureCtx(pData->pOutputCaptureCtx);
	}

	free(pData->szBinary);
	free(pData->szTemplateName);
	free(pData->szBeginTransactionMark);
	free(pData->szCommitTransactionMark);

	if(pData->aParams != NULL) {
		for (i = 0; i < pData->iParams; i++) {
			free(pData->aParams[i]);
		}
		free(pData->aParams);
	}
ENDfreeInstance


BEGINnewActInst
	struct cnfparamvals *pvals;
	int i;
CODESTARTnewActInst;
	if((pvals = nvlstGetParams(lst, &actpblk, NULL)) == NULL) {
		ABORT_FINALIZE(RS_RET_MISSING_CNFPARAMS);
	}

	CHKiRet(createInstance(&pData));

	for(i = 0 ; i < actpblk.nParams ; ++i) {
		if(!pvals[i].bUsed)
			continue;
		if(!strcmp(actpblk.descr[i].name, "binary")) {
			CHKiRet(split_binary_parameters(&pData->szBinary, &pData->aParams, &pData->iParams,
				pvals[i].val.d.estr));
		} else if(!strcmp(actpblk.descr[i].name, "confirmMessages")) {
			pData->bConfirmMessages = (int) pvals[i].val.d.n;
		} else if(!strcmp(actpblk.descr[i].name, "confirmTimeout")) {
			pData->lConfirmTimeout = (long) pvals[i].val.d.n;
		} else if(!strcmp(actpblk.descr[i].name, "reportFailures")) {
			pData->bReportFailures = (int) pvals[i].val.d.n;
		} else if(!strcmp(actpblk.descr[i].name, "useTransactions")) {
			pData->bUseTransactions = (int) pvals[i].val.d.n;
		} else if(!strcmp(actpblk.descr[i].name, "beginTransactionMark")) {
			pData->szBeginTransactionMark = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(actpblk.descr[i].name, "commitTransactionMark")) {
			pData->szCommitTransactionMark = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(actpblk.descr[i].name, "forceSingleInstance")) {
			pData->bForceSingleInst = (int) pvals[i].val.d.n;
		} else if(!strcmp(actpblk.descr[i].name, "signalOnClose")) {
			pData->bSignalOnClose = (int) pvals[i].val.d.n;
		} else if(!strcmp(actpblk.descr[i].name, "closeTimeout")) {
			pData->lCloseTimeout = (long) pvals[i].val.d.n;
		} else if(!strcmp(actpblk.descr[i].name, "killUnresponsive")) {
			pData->bKillUnresponsive = (int) pvals[i].val.d.n;
		} else if(!strcmp(actpblk.descr[i].name, "hup.signal")) {
			const char *const sig = es_str2cstr(pvals[i].val.d.estr, NULL);
			if(!strcmp(sig, "HUP"))
				pData->iHUPForward = SIGHUP;
			else if(!strcmp(sig, "USR1"))
				pData->iHUPForward = SIGUSR1;
			else if(!strcmp(sig, "USR2"))
				pData->iHUPForward = SIGUSR2;
			else if(!strcmp(sig, "INT"))
				pData->iHUPForward = SIGINT;
			else if(!strcmp(sig, "TERM"))
				pData->iHUPForward = SIGTERM;
			else {
				LogError(0, RS_RET_CONF_PARAM_INVLD,
					"omprog: hup.signal '%s' in hup.signal parameter", sig);
				ABORT_FINALIZE(RS_RET_CONF_PARAM_INVLD);
			}
			free((void*)sig);
		} else if(!strcmp(actpblk.descr[i].name, "template")) {
			pData->szTemplateName = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(actpblk.descr[i].name, "output")) {
			if(pData->pOutputCaptureCtx == NULL) {
				CHKiRet(allocOutputCaptureCtx(&pData->pOutputCaptureCtx));
			}
			pData->pOutputCaptureCtx->szFileName = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(actpblk.descr[i].name, "fileCreateMode")) {
			if(pData->pOutputCaptureCtx == NULL) {
				CHKiRet(allocOutputCaptureCtx(&pData->pOutputCaptureCtx));
			}
			pData->pOutputCaptureCtx->fCreateMode = (mode_t) pvals[i].val.d.n;
		} else {
			DBGPRINTF("omprog: program error, non-handled param '%s'\n", actpblk.descr[i].name);
		}
	}

	CODE_STD_STRING_REQUESTnewActInst(1);
	CHKiRet(OMSRsetEntry(*ppOMSR, 0, (uchar*)strdup(pData->szTemplateName == NULL ?
			"RSYSLOG_FileFormat" : (char*)pData->szTemplateName), OMSR_NO_RQD_TPL_OPTS));

	iRet = postInitInstance(pData);

CODE_STD_FINALIZERnewActInst;
	cnfparamvalsDestruct(pvals, &actpblk);
ENDnewActInst


BEGINparseSelectorAct
CODESTARTparseSelectorAct;
CODE_STD_STRING_REQUESTparseSelectorAct(1)
	/* first check if this config line is actually for us */
	if(strncmp((char*) p, ":omprog:", sizeof(":omprog:") - 1)) {
		ABORT_FINALIZE(RS_RET_CONFLINE_UNPROCESSED);
	}

	/* ok, if we reach this point, we have something for us */
	p += sizeof(":omprog:") - 1; /* eat indicator sequence  (-1 because of '\0'!) */
	if(cs.szBinary == NULL) {
		LogError(0, RS_RET_CONF_RQRD_PARAM_MISSING, "no binary to execute specified");
		ABORT_FINALIZE(RS_RET_CONF_RQRD_PARAM_MISSING);
	}

	CHKiRet(createInstance(&pData));
	CHKmalloc(pData->szBinary = (uchar*) strdup((char*)cs.szBinary));

	/* check if a non-standard template is to be applied */
	if(*(p-1) == ';')
		--p;
	CHKiRet(cflineParseTemplateName(&p, *ppOMSR, 0, 0, (uchar*) "RSYSLOG_FileFormat"));

	iRet = postInitInstance(pData);

CODE_STD_FINALIZERparseSelectorAct
ENDparseSelectorAct


BEGINdoHUP
CODESTARTdoHUP;
	if(pData->bForceSingleInst && pData->iHUPForward != NO_HUP_FORWARD &&
			pData->pSingleChildCtx->bIsRunning) {
		DBGPRINTF("omprog: forwarding HUP to program '%s' (pid %ld) as signal %d\n",
				pData->szBinary, (long) pData->pSingleChildCtx->pid, pData->iHUPForward);
		kill(pData->pSingleChildCtx->pid, pData->iHUPForward);
	}

	if(pData->pOutputCaptureCtx != NULL && pData->pOutputCaptureCtx->bIsRunning) {
		closeOutputFile(pData->pOutputCaptureCtx);
	}
ENDdoHUP


BEGINdoHUPWrkr
CODESTARTdoHUPWrkr;
	if(!pWrkrData->pData->bForceSingleInst && pWrkrData->pData->iHUPForward != NO_HUP_FORWARD &&
	 		pWrkrData->pChildCtx->bIsRunning) {
		DBGPRINTF("omprog: forwarding HUP to program '%s' (pid %ld) as signal %d\n",
				pWrkrData->pData->szBinary, (long) pWrkrData->pChildCtx->pid,
				pWrkrData->pData->iHUPForward);
		kill(pWrkrData->pChildCtx->pid, pWrkrData->pData->iHUPForward);
	}
ENDdoHUPWrkr


BEGINmodExit
CODESTARTmodExit;
	free(cs.szBinary);
	cs.szBinary = NULL;
ENDmodExit


BEGINqueryEtryPt
CODESTARTqueryEtryPt;
CODEqueryEtryPt_STD_OMOD_QUERIES;
CODEqueryEtryPt_STD_OMOD8_QUERIES;
CODEqueryEtryPt_STD_CONF2_OMOD_QUERIES;
CODEqueryEtryPt_TXIF_OMOD_QUERIES /* we support the transactional interface */
CODEqueryEtryPt_doHUP
CODEqueryEtryPt_doHUPWrkr
ENDqueryEtryPt


/* Reset legacy config variables for this module to default values.
 */
static rsRetVal
resetConfigVariables(uchar __attribute__((unused)) *pp, void __attribute__((unused)) *pVal)
{
	DEFiRet;
	free(cs.szBinary);
	cs.szBinary = NULL;
	RETiRet;
}

BEGINmodInit()
CODESTARTmodInit;
INITLegCnfVars;
	*ipIFVersProvided = CURR_MOD_IF_VERSION; /* we only support the current interface specification */
CODEmodInit_QueryRegCFSLineHdlr
	/* tell engine which objects we need */

	/* check that rsyslog core supports transactional plugins */
	INITChkCoreFeature(bCoreSupportsBatching, CORE_FEATURE_BATCHING);
	if (!bCoreSupportsBatching) {
		LogError(0, NO_ERRCODE, "omprog: rsyslog core too old (does not support batching)");
		ABORT_FINALIZE(RS_RET_ERR);
	}

	CHKiRet(omsdRegCFSLineHdlr((uchar *)"actionomprogbinary", 0, eCmdHdlrGetWord, NULL, &cs.szBinary,
		STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"resetconfigvariables", 1, eCmdHdlrCustomHandler, resetConfigVariables,
		NULL, STD_LOADABLE_MODULE_ID));
CODEmodInit_QueryRegCFSLineHdlr
ENDmodInit
