/* omprog.c
 * This output plugin enables rsyslog to execute a program and
 * feed it the message stream as standard input.
 *
 * NOTE: read comments in module-template.h for more specifics!
 *
 * File begun on 2009-04-01 by RGerhards
 *
 * Copyright 2009-2018 Adiscon GmbH.
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
#include "rsyslog.h"
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
#include "conf.h"
#include "syslogd-types.h"
#include "srUtils.h"
#include "template.h"
#include "module-template.h"
#include "errmsg.h"
#include "cfsysline.h"

MODULE_TYPE_OUTPUT
MODULE_TYPE_NOKEEP
MODULE_CNFNAME("omprog")

extern char **environ; /* POSIX environment ptr, by std not in a header... (see man 7 environ) */

/* internal structures
 */
DEF_OMOD_STATIC_DATA

#define NO_HUP_FORWARD -1	/* indicates that HUP should NOT be forwarded */
#define DEFAULT_CLOSE_TIMEOUT_MS 5000
#define READLINE_BUFFER_SIZE 1024
#define OUTPUT_CAPTURE_BUFFER_SIZE 4096
#define MAX_FD_TO_CLOSE 65535

typedef struct outputCaptureCtx {
	uchar *szFileName;		/* name of file to write the program output to, or NULL */
	mode_t fCreateMode;		/* output file creation permissions */
	int bIsRunning;			/* is the output-capture thread running? (if 0, next fields are meaningless) */
	pthread_t thrdID;		/* ID of the output-capture thread */
	int fdPipe[2];			/* pipe for capturing the output of the child processes */
	int fdFile;				/* fd of the output file (-1 if it could not be opened) */
	int bFileErr;			/* file open error occurred? (to avoid reporting too many errors) */
	int bReadErr;			/* read error occurred? (to avoid reporting too many errors) */
	int bWriteErr;			/* write error occurred? (to avoid reporting too many errors) */
	pthread_mutex_t mutWrite;	/* mutex for reopening the output file on HUP while being written */
	pthread_mutex_t mutTerm;	/* mutex for signaling the termination of the thread */
	pthread_cond_t condTerm;	/* condition for signaling the termination of the thread */
} outputCaptureCtx_t;

typedef struct _instanceData {
	uchar *szBinary;		/* name of external program to call */
	char **aParams;			/* optional parameters to pass to external program */
	int iParams;			/* holds the count of parameters if set */
	uchar *szTemplateName;	/* assigned output template */
	int bConfirmMessages;	/* does the program provide feedback via stdout? */
	int bUseTransactions;	/* send begin/end transaction marks to program? */
	uchar *szBeginTransactionMark;	/* mark message for begin transaction */
	uchar *szCommitTransactionMark;	/* mark message for commit transaction */
	int bForceSingleInst;	/* only a single wrkr instance of program permitted? */
	int iHUPForward;		/* signal to forward on HUP (or NO_HUP_FORWARD) */
	int bSignalOnClose;		/* should send SIGTERM to program before closing pipe? */
	long lCloseTimeout;		/* how long to wait for program to terminate after closing pipe (ms) */
	int bKillUnresponsive;	/* should send SIGKILL if closeTimeout is reached? */
	outputCaptureCtx_t outputCaptureCtx;	/* settings and state for the output capture thread */
	pthread_mutex_t mut;	/* make sure only one instance is active (buggy! - see #2813) */
} instanceData;

typedef struct wrkrInstanceData {
	instanceData *pData;
	pid_t pid;			/* pid of currently running child process */
	int fdPipeOut;		/* fd for sending messages to the program */
	int fdPipeIn;		/* fd for receiving status messages from the program, or -1 */
	int bIsRunning;		/* is program currently running? 0-no, 1-yes */
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
	{ "useTransactions", eCmdHdlrBinary, 0 },
	{ "beginTransactionMark", eCmdHdlrString, 0 },
	{ "commitTransactionMark", eCmdHdlrString, 0 },
	{ "forcesingleinstance", eCmdHdlrBinary, 0 },
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

	if(pData->outputCaptureCtx.bIsRunning) {
		fdOutput = pData->outputCaptureCtx.fdPipe[1];
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
openPipe(wrkrInstanceData_t *pWrkrData)
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
	if(pWrkrData->pData->bConfirmMessages && pipe(pipeStdout) == -1) {
		ABORT_FINALIZE(RS_RET_ERR_CREAT_PIPE);
	}

	DBGPRINTF("omprog: executing program '%s' with '%d' parameters\n",
			pWrkrData->pData->szBinary, pWrkrData->pData->iParams);

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

		execBinary(pWrkrData->pData, pipeStdin[0], pipeStdout[1]);
		/* NO CODE HERE - WILL NEVER BE REACHED! */
	}

	DBGPRINTF("omprog: child has pid %d\n", (int) cpid);

	/* close the pipe ends that the parent doesn't need */
	close(pipeStdin[0]);
	if(pipeStdout[1] != -1) {
		close(pipeStdout[1]);
	}

	/* we'll send messages to the program via fdPipeOut */
	pWrkrData->fdPipeOut = pipeStdin[1];

	/* we'll receive message confirmations via fdPipeIn */
	pWrkrData->fdPipeIn = pipeStdout[0];

	pWrkrData->pid = cpid;
	pWrkrData->bIsRunning = 1;

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
waitForChild(wrkrInstanceData_t *pWrkrData)
{
	int status;
	int ret;
	long counter;

	counter = pWrkrData->pData->lCloseTimeout / 10;
	while ((ret = waitpid(pWrkrData->pid, &status, WNOHANG)) == 0 && counter > 0) {
		srSleep(0, 10000);  /* 0 seconds, 10 milliseconds */
		--counter;
	}

	if (ret == 0) {  /* timeout reached */
		if (!pWrkrData->pData->bKillUnresponsive) {
			LogMsg(0, NO_ERRCODE, LOG_WARNING, "omprog: program '%s' (pid %d) did not terminate "
					"within timeout (%ld ms); ignoring it", pWrkrData->pData->szBinary,
					pWrkrData->pid, pWrkrData->pData->lCloseTimeout);
			return;
		}

		LogMsg(0, NO_ERRCODE, LOG_WARNING, "omprog: program '%s' (pid %d) did not terminate "
				"within timeout (%ld ms); killing it", pWrkrData->pData->szBinary,
				pWrkrData->pid, pWrkrData->pData->lCloseTimeout);
		if (kill(pWrkrData->pid, SIGKILL) == -1) {
			LogError(errno, RS_RET_SYS_ERR, "omprog: could not send SIGKILL to child process");
			return;
		}
		ret = waitpid(pWrkrData->pid, &status, 0);
	}

	if (ret != pWrkrData->pid) {
		if (errno == ECHILD) {  /* child reaped by the rsyslogd main loop (see rsyslogd.c) */
			LogMsg(0, NO_ERRCODE, LOG_INFO, "omprog: program '%s' (pid %d) exited; reaped by main loop",
					pWrkrData->pData->szBinary, pWrkrData->pid);
		} else {
			LogError(errno, RS_RET_SYS_ERR, "omprog: waitpid failed for program '%s' (pid %d)",
					pWrkrData->pData->szBinary, pWrkrData->pid);
		}
	} else {
		/* check if we should print out some diagnostic information */
		DBGPRINTF("omprog: waitpid status return for program '%s' (pid %d): %2.2x\n",
				pWrkrData->pData->szBinary, pWrkrData->pid, status);
		if(WIFEXITED(status)) {
			LogMsg(0, NO_ERRCODE, LOG_INFO, "omprog: program '%s' (pid %d) exited normally, status %d",
					pWrkrData->pData->szBinary, pWrkrData->pid, WEXITSTATUS(status));
		} else if(WIFSIGNALED(status)) {
			LogMsg(0, NO_ERRCODE, LOG_WARNING, "omprog: program '%s' (pid %d) terminated by signal %d",
					pWrkrData->pData->szBinary, pWrkrData->pid, WTERMSIG(status));
		}
	}
}

/* close pipe and wait for child to terminate
 */
static void
cleanupChild(wrkrInstanceData_t *pWrkrData)
{
	assert(pWrkrData->bIsRunning);

	if(pWrkrData->fdPipeIn != -1) {
		close(pWrkrData->fdPipeIn);
		pWrkrData->fdPipeIn = -1;
	}
	if(pWrkrData->fdPipeOut != -1) {
		close(pWrkrData->fdPipeOut);
		pWrkrData->fdPipeOut = -1;
	}

	/* wait for the child AFTER closing the pipe, so it receives EOF */
	waitForChild(pWrkrData);

	pWrkrData->bIsRunning = 0;
}

/* Send SIGTERM to child process if configured to do so, close pipe
 * and wait for child to terminate.
 */
static void
terminateChild(wrkrInstanceData_t *pWrkrData)
{
	assert(pWrkrData->bIsRunning);

	if (pWrkrData->pData->bSignalOnClose) {
		kill(pWrkrData->pid, SIGTERM);
	}

	cleanupChild(pWrkrData);
}

/* write to pipe
 * note that we do not try to run block-free. If the users fears something
 * may block (and this not be acceptable), the action should be run on its
 * own action queue.
 */
static rsRetVal
writePipe(wrkrInstanceData_t *pWrkrData, uchar *szMsg)
{
	ssize_t len;
	ssize_t written;
	ssize_t offset = 0;
	char errStr[1024];
	DEFiRet;

	len = strlen((char*)szMsg);

	do {
		written = write(pWrkrData->fdPipeOut, ((char*)szMsg) + offset, len - offset);
		if(written == -1) {
			if(errno == EPIPE) {
				DBGPRINTF("omprog: program '%s' terminated, will be restarted\n",
						pWrkrData->pData->szBinary);
				/* force restart in tryResume() */
				cleanupChild(pWrkrData);
			} else {
				DBGPRINTF("omprog: error %d writing to pipe: %s\n", errno,
						rs_strerror_r(errno, errStr, sizeof(errStr)));
			}
			ABORT_FINALIZE(RS_RET_SUSPENDED);
		}
		offset += written;
	} while(offset < len);

finalize_it:
	RETiRet;
}

/* Reads a line from a blocking pipe, using the unistd.h read() function.
 * Returns the line as a null-terminated string in *lineptr, not including
 * the \n or \r\n terminator.
 * On success, returns the line length (>= 0).
 * On error, returns -1 and sets errno.
 * On EOF, returns -1 and sets errno to EPIPE (if EOF is read in the middle of
 * a line, discards the read characters).
 * On success, the caller is responsible for freeing the returned line buffer.
 */
static ssize_t
readline(int fd, char **lineptr)
{
	char *buf = NULL;
	char *new_buf;
	size_t buf_size = 0;
	size_t len = 0;
	ssize_t nr;
	char ch;

	nr = read(fd, &ch, 1);
	while (nr == 1 && ch != '\n') {
		if (len == buf_size) {
			buf_size += READLINE_BUFFER_SIZE;
			new_buf = (char*) realloc(buf, buf_size);
			if (new_buf == NULL) {
				free(buf);
				errno = ENOMEM;
				return -1;
			}
			buf = new_buf;
		}

		buf[len++] = ch;
		nr = read(fd, &ch, 1);
	}

	if (nr == 0) {  /* EOF */
		free(buf);
		errno = EPIPE;
		return -1;
	}

	if (nr == -1) {
		free(buf);
		return -1;  /* errno already set by 'read' */
	}

	/* Ignore \r (if any) before \n */
	if (len > 0 && buf[len-1] == '\r') {
		--len;
	}

	/* If necessary, make room for the null terminator */
	if (len == buf_size) {
		new_buf = (char*) realloc(buf, ++buf_size);
		if (new_buf == NULL) {
			free(buf);
			errno = ENOMEM;
			return -1;
		}
		buf = new_buf;
	}

	buf[len] = '\0';
	*lineptr = buf;
	return len;
}

static rsRetVal
lineToStatusCode(wrkrInstanceData_t *pWrkrData, const char* line)
{
	DEFiRet;

	if(strcmp(line, "OK") == 0) {
		iRet = RS_RET_OK;
	} else if(strcmp(line, "DEFER_COMMIT") == 0) {
		iRet = RS_RET_DEFER_COMMIT;
	} else if(strcmp(line, "PREVIOUS_COMMITTED") == 0) {
		iRet = RS_RET_PREVIOUS_COMMITTED;
	} else {
		/* anything else is considered a recoverable error */
		DBGPRINTF("omprog: program '%s' returned: %s\n", pWrkrData->pData->szBinary, line);
		iRet = RS_RET_SUSPENDED;
	}
	RETiRet;
}

static rsRetVal
readPipe(wrkrInstanceData_t *pWrkrData)
{
	char *line = NULL;
	ssize_t lineLen;
	char errStr[1024];
	DEFiRet;

	lineLen = readline(pWrkrData->fdPipeIn, &line);
	if (lineLen == -1) {
		if (errno == EPIPE) {
			DBGPRINTF("omprog: program '%s' terminated, will be restarted\n",
					pWrkrData->pData->szBinary);
			/* force restart in tryResume() */
			cleanupChild(pWrkrData);
		} else {
			DBGPRINTF("omprog: error %d reading from pipe: %s\n", errno,
				   rs_strerror_r(errno, errStr, sizeof(errStr)));
		}
		ABORT_FINALIZE(RS_RET_SUSPENDED);
	}

	CHKiRet(lineToStatusCode(pWrkrData, line));

finalize_it:
	free(line);
	RETiRet;
}

static rsRetVal
startChild(wrkrInstanceData_t *pWrkrData)
{
	DEFiRet;

	assert(!pWrkrData->bIsRunning);

	CHKiRet(openPipe(pWrkrData));

	if(pWrkrData->pData->bConfirmMessages) {
		/* wait for program to confirm successful initialization */
		CHKiRet(readPipe(pWrkrData));
	}

finalize_it:
	if (iRet != RS_RET_OK && pWrkrData->bIsRunning) {
		/* if initialization has failed, terminate program */
		terminateChild(pWrkrData);
	}
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
			return;
		}

		pCtx->fdFile = open((char*)pCtx->szFileName, O_WRONLY | O_APPEND | O_CREAT,
				pCtx->fCreateMode);
		if(pCtx->fdFile == -1) {
			LogError(errno, RS_RET_NO_FILE_ACCESS, "omprog: error opening output file %s; "
					"output from program will be discarded", pCtx->szFileName);
			pCtx->bFileErr = 1;  /* avoid reporting too many errors */
			return;
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

	pthread_mutex_unlock(&pCtx->mutWrite);
}

static void
closeOutputFile(outputCaptureCtx_t *pCtx)
{
	assert(pCtx->bIsRunning);
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
startOutputCapture(outputCaptureCtx_t *pCtx)
{
	int pip[2] = { -1, -1 };
	DEFiRet;

	assert(!pCtx->bIsRunning);

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
	CHKiConcCtrl(pthread_mutex_init(&pCtx->mutWrite, NULL));
	CHKiConcCtrl(pthread_mutex_init(&pCtx->mutTerm, NULL));
	CHKiConcCtrl(pthread_cond_init(&pCtx->condTerm, NULL));

	/* start a thread to read lines from the pipe and write them to the output file */
	CHKiConcCtrl(pthread_create(&pCtx->thrdID, NULL, captureOutput, (void *)pCtx));

	pCtx->bIsRunning = 1;

finalize_it:
	if(iRet != RS_RET_OK && pip[0] != -1) {
		close(pip[0]);
		close(pip[1]);
	}
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
	pthread_cond_destroy(&pCtx->condTerm);
	pthread_mutex_destroy(&pCtx->mutTerm);
	pthread_mutex_destroy(&pCtx->mutWrite);

	/* close the read end of the output-capture pipe */
	close(pCtx->fdPipe[0]);

	/* close the output file (if it could be opened) */
	if(pCtx->fdFile != -1) {
		close(pCtx->fdFile);
	}
}


BEGINinitConfVars		/* (re)set config variables to default values */
CODESTARTinitConfVars
	cs.szBinary = NULL;	/* name of binary to call */
ENDinitConfVars


BEGINcreateInstance
CODESTARTcreateInstance
	pthread_mutex_init(&pData->mut, NULL);
ENDcreateInstance


BEGINcreateWrkrInstance
CODESTARTcreateWrkrInstance
	pWrkrData->pid = -1;
	pWrkrData->fdPipeOut = -1;
	pWrkrData->fdPipeIn = -1;
	pWrkrData->bIsRunning = 0;

	iRet = startChild(pWrkrData);
ENDcreateWrkrInstance


BEGINisCompatibleWithFeature
CODESTARTisCompatibleWithFeature
	if(eFeat == sFEATURERepeatedMsgReduction) {
		iRet = RS_RET_OK;
	}
ENDisCompatibleWithFeature


BEGINdbgPrintInstInfo
CODESTARTdbgPrintInstInfo
ENDdbgPrintInstInfo


BEGINtryResume
CODESTARTtryResume
	if (pWrkrData->bIsRunning == 0) {
		CHKiRet(startChild(pWrkrData));
	}
finalize_it:
ENDtryResume


BEGINbeginTransaction
CODESTARTbeginTransaction
	if(!pWrkrData->pData->bUseTransactions) {
		FINALIZE;
	}

	CHKiRet(writePipe(pWrkrData, pWrkrData->pData->szBeginTransactionMark));
	CHKiRet(writePipe(pWrkrData, (uchar*) "\n"));

	if(pWrkrData->pData->bConfirmMessages) {
		CHKiRet(readPipe(pWrkrData));
	}
finalize_it:
ENDbeginTransaction


BEGINdoAction
CODESTARTdoAction
	if(pWrkrData->pData->bForceSingleInst) {
		pthread_mutex_lock(&pWrkrData->pData->mut);
	}
	if(pWrkrData->bIsRunning == 0) {  /* should not occur */
		ABORT_FINALIZE(RS_RET_SUSPENDED);
	}

	CHKiRet(writePipe(pWrkrData, ppString[0]));

	if(pWrkrData->pData->bConfirmMessages) {
		CHKiRet(readPipe(pWrkrData));
	} else if(pWrkrData->pData->bUseTransactions) {
		/* ensure endTransaction will be called */
		iRet = RS_RET_DEFER_COMMIT;
	}

finalize_it:
	if(pWrkrData->pData->bForceSingleInst) {
		pthread_mutex_unlock(&pWrkrData->pData->mut);
	}
ENDdoAction


BEGINendTransaction
CODESTARTendTransaction
	if(!pWrkrData->pData->bUseTransactions) {
		FINALIZE;
	}

	CHKiRet(writePipe(pWrkrData, pWrkrData->pData->szCommitTransactionMark));
	CHKiRet(writePipe(pWrkrData, (uchar*) "\n"));

	if(pWrkrData->pData->bConfirmMessages) {
		CHKiRet(readPipe(pWrkrData));
	}
finalize_it:
ENDendTransaction


BEGINfreeWrkrInstance
CODESTARTfreeWrkrInstance
	if (pWrkrData->bIsRunning) {
		terminateChild(pWrkrData);
	}
ENDfreeWrkrInstance


BEGINfreeInstance
	int i;
CODESTARTfreeInstance
	pthread_mutex_destroy(&pData->mut);

	if(pData->outputCaptureCtx.bIsRunning) {
		endOutputCapture(&pData->outputCaptureCtx, pData->lCloseTimeout);
	}

	free(pData->szBinary);
	free(pData->szTemplateName);
	free(pData->szBeginTransactionMark);
	free(pData->szCommitTransactionMark);
	free(pData->outputCaptureCtx.szFileName);

	if(pData->aParams != NULL) {
		for (i = 0; i < pData->iParams; i++) {
			free(pData->aParams[i]);
		}
		free(pData->aParams);
	}
ENDfreeInstance


static void
setInstParamDefaults(instanceData *pData)
{
	pData->szBinary = NULL;
	pData->szTemplateName = NULL;
	pData->aParams = NULL;
	pData->iParams = 0;
	pData->bConfirmMessages = 0;
	pData->bUseTransactions = 0;
	pData->szBeginTransactionMark = NULL;
	pData->szCommitTransactionMark = NULL;
	pData->bForceSingleInst = 0;
	pData->iHUPForward = NO_HUP_FORWARD;
	pData->bSignalOnClose = 0;
	pData->lCloseTimeout = DEFAULT_CLOSE_TIMEOUT_MS;
	pData->bKillUnresponsive = -1;
	pData->outputCaptureCtx.szFileName = NULL;
	pData->outputCaptureCtx.fCreateMode = 0600;
	pData->outputCaptureCtx.bIsRunning = 0;
}

static void
setInstParamCalcDefaults(instanceData *pData)
{
	if(pData->bUseTransactions && pData->szBeginTransactionMark == NULL) {
		pData->szBeginTransactionMark = (uchar*)strdup("BEGIN TRANSACTION");
	}
	if(pData->bUseTransactions && pData->szCommitTransactionMark == NULL) {
		pData->szCommitTransactionMark = (uchar*)strdup("COMMIT TRANSACTION");
	}
	if(pData->bKillUnresponsive == -1) {  /* default value: bSignalOnClose */
		pData->bKillUnresponsive = pData->bSignalOnClose;
	}
}


BEGINnewActInst
	struct cnfparamvals *pvals;
	int i;
CODESTARTnewActInst
	if((pvals = nvlstGetParams(lst, &actpblk, NULL)) == NULL) {
		ABORT_FINALIZE(RS_RET_MISSING_CNFPARAMS);
	}

	CHKiRet(createInstance(&pData));
	setInstParamDefaults(pData);

	CODE_STD_STRING_REQUESTnewActInst(1)
	for(i = 0 ; i < actpblk.nParams ; ++i) {
		if(!pvals[i].bUsed)
			continue;
		if(!strcmp(actpblk.descr[i].name, "binary")) {
			CHKiRet(split_binary_parameters(&pData->szBinary, &pData->aParams, &pData->iParams,
				pvals[i].val.d.estr));
		} else if(!strcmp(actpblk.descr[i].name, "confirmMessages")) {
			pData->bConfirmMessages = (int) pvals[i].val.d.n;
		} else if(!strcmp(actpblk.descr[i].name, "useTransactions")) {
			pData->bUseTransactions = (int) pvals[i].val.d.n;
		} else if(!strcmp(actpblk.descr[i].name, "beginTransactionMark")) {
			pData->szBeginTransactionMark = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(actpblk.descr[i].name, "commitTransactionMark")) {
			pData->szCommitTransactionMark = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(actpblk.descr[i].name, "forcesingleinstance")) {
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
			pData->outputCaptureCtx.szFileName = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(actpblk.descr[i].name, "fileCreateMode")) {
			pData->outputCaptureCtx.fCreateMode = (mode_t) pvals[i].val.d.n;
		} else {
			DBGPRINTF("omprog: program error, non-handled param '%s'\n", actpblk.descr[i].name);
		}
	}

	setInstParamCalcDefaults(pData);
	CHKiRet(OMSRsetEntry(*ppOMSR, 0, (uchar*)strdup((pData->szTemplateName == NULL) ?
						"RSYSLOG_FileFormat" : (char*)pData->szTemplateName),
						OMSR_NO_RQD_TPL_OPTS));
	
	if(pData->outputCaptureCtx.szFileName != NULL) {
		CHKiRet(startOutputCapture(&pData->outputCaptureCtx));
	}

CODE_STD_FINALIZERnewActInst
	cnfparamvalsDestruct(pvals, &actpblk);
ENDnewActInst


BEGINparseSelectorAct
CODESTARTparseSelectorAct
CODE_STD_STRING_REQUESTparseSelectorAct(1)
	/* first check if this config line is actually for us */
	if(strncmp((char*) p, ":omprog:", sizeof(":omprog:") - 1)) {
		ABORT_FINALIZE(RS_RET_CONFLINE_UNPROCESSED);
	}

	/* ok, if we reach this point, we have something for us */
	p += sizeof(":omprog:") - 1; /* eat indicator sequence  (-1 because of '\0'!) */
	if(cs.szBinary == NULL) {
		LogError(0, RS_RET_CONF_RQRD_PARAM_MISSING,
			"no binary to execute specified");
		ABORT_FINALIZE(RS_RET_CONF_RQRD_PARAM_MISSING);
	}

	CHKiRet(createInstance(&pData));

	if(cs.szBinary == NULL) {
		LogError(0, RS_RET_CONF_RQRD_PARAM_MISSING,
			"no binary to execute specified");
		ABORT_FINALIZE(RS_RET_CONF_RQRD_PARAM_MISSING);
	}

	CHKmalloc(pData->szBinary = (uchar*) strdup((char*)cs.szBinary));
	/* check if a non-standard template is to be applied */
	if(*(p-1) == ';')
		--p;
	iRet = cflineParseTemplateName(&p, *ppOMSR, 0, 0, (uchar*) "RSYSLOG_FileFormat");
CODE_STD_FINALIZERparseSelectorAct
ENDparseSelectorAct


BEGINdoHUP
CODESTARTdoHUP
	if(pData->outputCaptureCtx.bIsRunning) {
		closeOutputFile(&pData->outputCaptureCtx);
	}
ENDdoHUP


BEGINdoHUPWrkr
CODESTARTdoHUPWrkr
	DBGPRINTF("omprog: processing HUP for work instance %p, pid %d, forward: %d\n",
			pWrkrData, (int) pWrkrData->pid, pWrkrData->pData->iHUPForward);
	if(pWrkrData->pData->iHUPForward != NO_HUP_FORWARD) {
		kill(pWrkrData->pid, pWrkrData->pData->iHUPForward);
	}
ENDdoHUPWrkr


BEGINmodExit
CODESTARTmodExit
	free(cs.szBinary);
	cs.szBinary = NULL;
ENDmodExit


BEGINqueryEtryPt
CODESTARTqueryEtryPt
CODEqueryEtryPt_STD_OMOD_QUERIES
CODEqueryEtryPt_STD_OMOD8_QUERIES
CODEqueryEtryPt_STD_CONF2_OMOD_QUERIES
CODEqueryEtryPt_TXIF_OMOD_QUERIES /* we support the transactional interface */
CODEqueryEtryPt_doHUP
CODEqueryEtryPt_doHUPWrkr
ENDqueryEtryPt


/* Reset config variables for this module to default values.
 */
static rsRetVal resetConfigVariables(uchar __attribute__((unused)) *pp, void __attribute__((unused)) *pVal)
{
	DEFiRet;
	free(cs.szBinary);
	cs.szBinary = NULL;
	RETiRet;
}

BEGINmodInit()
CODESTARTmodInit
INITLegCnfVars
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
