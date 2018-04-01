/* omprog.c
 * This output plugin enables rsyslog to execute a program and
 * feed it the message stream as standard input.
 *
 * NOTE: read comments in module-template.h for more specifics!
 *
 * File begun on 2009-04-01 by RGerhards
 *
 * Copyright 2009-2017 Adiscon GmbH.
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
#if defined(__linux__) && defined(_GNU_SOURCE)
#include <sys/syscall.h>
#include <sys/types.h>
#endif
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

/* internal structures
 */
DEF_OMOD_STATIC_DATA
DEFobjCurrIf(errmsg)

#define NO_HUP_FORWARD -1	/* indicates that HUP should NOT be forwarded */
/* linux specific: how long to wait for process to terminate gracefully before issuing SIGKILL */
#define DEFAULT_FORCED_TERMINATION_TIMEOUT_MS 5000
#define READLINE_BUFFER_SIZE 1024

typedef struct _instanceData {
	uchar *szBinary;	/* name of binary to call */
	char **aParams;		/* optional parameters for binary command */
	uchar *tplName;		/* assigned output template */
	int iParams;		/* holds the count of parameters if set */
	int bConfirmMessages;	/* does the program provide feedback via stdout? */
	int bUseTransactions;	/* send begin/end transaction marks to program? */
	uchar *szBeginTransactionMark;	/* mark message for begin transaction */
	uchar *szCommitTransactionMark;	/* mark message for commit transaction */
	int bForceSingleInst;	/* only a single wrkr instance of program permitted? */
	int iHUPForward;	/* signal to forward on HUP (or NO_HUP_FORWARD) */
	int bSignalOnClose;	/* should signal process at shutdown */
	uchar *outputFileName;	/* name of file to write the program output to, or NULL */
	pthread_mutex_t mut;	/* make sure only one instance is active */
} instanceData;

typedef struct wrkrInstanceData {
	instanceData *pData;
	pid_t pid;		/* pid of currently running process */
	int fdPipeOut;		/* fd for sending messages to the program */
	int fdPipeIn;		/* fd for receiving status messages from the program */
	int fdPipeErr;		/* fd for receiving error output from the program */
	int fdOutputFile;	/* fd to write the program output to (-1 if to discard) */
	int bIsRunning;		/* is binary currently running? 0-no, 1-yes */
} wrkrInstanceData_t;

typedef struct configSettings_s {
	uchar *szBinary;	/* name of binary to call */
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
	{ "output", eCmdHdlrString, 0 },
	{ "forcesingleinstance", eCmdHdlrBinary, 0 },
	{ "hup.signal", eCmdHdlrGetWord, 0 },
	{ "template", eCmdHdlrGetWord, 0 },
	{ "signalOnClose", eCmdHdlrBinary, 0 }
};

static struct cnfparamblk actpblk =
	{ CNFPARAMBLK_VERSION,
	  sizeof(actpdescr)/sizeof(struct cnfparamdescr),
	  actpdescr
	};

/* execute the child process (must be called in child context
 * after fork).
 */
static __attribute__((noreturn)) void
execBinary(wrkrInstanceData_t *pWrkrData, int fdStdin, int fdStdout, int fdStderr)
{
	int i, iRet;
	struct sigaction sigAct;
	sigset_t set;
	char errStr[1024];
	char *newenviron[] = { NULL };

	if(dup2(fdStdin, STDIN_FILENO) == -1) {
		DBGPRINTF("omprog: dup() stdin failed\n");
		/* do some more error handling here? Maybe if the module
		 * gets some more widespread use...
		 */
	}

	if(pWrkrData->pData->bConfirmMessages) {
		/* send message confirmations via stdout */
		if(dup2(fdStdout, STDOUT_FILENO) == -1) {
			DBGPRINTF("omprog: dup() stdout failed\n");
		}
		/* redirect stderr to the output file, if specified */
		if (pWrkrData->pData->outputFileName != NULL) {
			if(dup2(fdStderr, STDERR_FILENO) == -1) {
				DBGPRINTF("omprog: dup() stderr failed\n");
			}
		} else {
			close(fdStderr);
		}
	} else if (pWrkrData->pData->outputFileName != NULL) {
		/* redirect both stdout and stderr to the output file */
		if(dup2(fdStderr, STDOUT_FILENO) == -1) {
			DBGPRINTF("omprog: dup() stdout failed\n");
		}
		if(dup2(fdStderr, STDERR_FILENO) == -1) {
			DBGPRINTF("omprog: dup() stderr failed\n");
		}
		close(fdStdout);
	} else {
		/* no need to send data to parent via stdout or stderr */
		close(fdStdout);
		close(fdStderr);
	}

	/* we close all file handles as we fork soon
	 * Is there a better way to do this? - mail me! rgerhards@adiscon.com
	 */
#	ifndef VALGRIND /* we can not use this with valgrind - too many errors... */
	for(i = 3 ; i <= 65535 ; ++i)
		close(i);
#	endif

	/* reset signal handlers to default */
	memset(&sigAct, 0, sizeof(sigAct));
	sigemptyset(&sigAct.sa_mask);
	sigAct.sa_handler = SIG_DFL;
	for(i = 1 ; i < NSIG ; ++i)
		sigaction(i, &sigAct, NULL);
	/* we need to block SIGINT, otherwise our program is cancelled when we are
	 * stopped in debug mode.
	 */
	sigAct.sa_handler = SIG_IGN;
	sigaction(SIGINT, &sigAct, NULL);
	sigemptyset(&set);
        sigprocmask(SIG_SETMASK, &set, NULL);

	alarm(0);

	/* finally exec child */
	iRet = execve((char*)pWrkrData->pData->szBinary, pWrkrData->pData->aParams, newenviron);
	if(iRet == -1) {
		/* Note: this will go to stdout of the **child**, so rsyslog will never
		 * see it except when stdout is captured. If we use the plugin interface,
		 * we can use this to convey a proper status back!
		 */
		rs_strerror_r(errno, errStr, sizeof(errStr));
		DBGPRINTF("omprog: failed to execute binary '%s': %s\n",
			  pWrkrData->pData->szBinary, errStr);
		openlog("rsyslogd", 0, LOG_SYSLOG);
		syslog(LOG_ERR, "omprog: failed to execute binary '%s': %s\n",
			  pWrkrData->pData->szBinary, errStr);
	}

	/* we should never reach this point, but if we do, we terminate */
	exit(1);
}

/* creates a pipe and starts program, uses pipe as stdin for program.
 * rgerhards, 2009-04-01
 */
static rsRetVal
openPipe(wrkrInstanceData_t *pWrkrData)
{
	int pipeStdin[2];
	int pipeStdout[2];
	int pipeStderr[2];
	pid_t cpid;
	int flags;
	DEFiRet;

	if(pipe(pipeStdin) == -1) {
		ABORT_FINALIZE(RS_RET_ERR_CREAT_PIPE);
	}
	if(pipe(pipeStdout) == -1) {
		close(pipeStdin[0]); close(pipeStdin[1]);
		ABORT_FINALIZE(RS_RET_ERR_CREAT_PIPE);
	}
	if(pipe(pipeStderr) == -1) {
		close(pipeStdin[0]); close(pipeStdin[1]);
		close(pipeStdout[0]); close(pipeStdout[1]);
		ABORT_FINALIZE(RS_RET_ERR_CREAT_PIPE);
	}

	DBGPRINTF("omprog: executing program '%s' with '%d' parameters\n",
		  pWrkrData->pData->szBinary, pWrkrData->pData->iParams);

	/* final sanity check */
	assert(pWrkrData->pData->szBinary != NULL);
	assert(pWrkrData->pData->aParams != NULL);

	/* NO OUTPUT AFTER FORK! */

	cpid = fork();
	if(cpid == -1) {
		ABORT_FINALIZE(RS_RET_ERR_FORK);
	}
	pWrkrData->pid = cpid;

	if(cpid == 0) {
		/* we are now the child, just exec the binary. */
		close(pipeStdin[1]); /* close those pipe "ports" that */
		close(pipeStdout[0]); /* ... we don't need */
		close(pipeStderr[0]);
		execBinary(pWrkrData, pipeStdin[0], pipeStdout[1], pipeStderr[1]);
		/*NO CODE HERE - WILL NEVER BE REACHED!*/
	}

	DBGPRINTF("omprog: child has pid %d\n", (int) cpid);

	close(pipeStdin[0]);
	close(pipeStdout[1]);
	close(pipeStderr[1]);

	/* we'll send messages to the program via fdPipeOut */
	pWrkrData->fdPipeOut = pipeStdin[1];

	if(pWrkrData->pData->bConfirmMessages) {
		/* we'll receive message confirmations via fdPipeIn */
		pWrkrData->fdPipeIn = pipeStdout[0];
		/* we'll capture stderr to the output file, if specified */
		if (pWrkrData->pData->outputFileName != NULL) {
			pWrkrData->fdPipeErr = pipeStderr[0];
		}
		else {
			close(pipeStderr[0]);
			pWrkrData->fdPipeErr = -1;
		}
	} else if (pWrkrData->pData->outputFileName != NULL) {
		/* we'll capture both stdout and stderr via fdPipeErr */
		close(pipeStdout[0]);
		pWrkrData->fdPipeIn = -1;
		pWrkrData->fdPipeErr = pipeStderr[0];
	} else {
		/* no need to read the program stdout or stderr */
		close(pipeStdout[0]);
		close(pipeStderr[0]);
		pWrkrData->fdPipeIn = -1;
		pWrkrData->fdPipeErr = -1;
	}

	if(pWrkrData->fdPipeErr != -1) {
		/* set our fd to be non-blocking */
		flags = fcntl(pWrkrData->fdPipeErr, F_GETFL);
		flags |= O_NONBLOCK;
		if(fcntl(pWrkrData->fdPipeErr, F_SETFL, flags) == -1) {
			LogError(errno, RS_RET_ERR, "omprog: set pipe fd to "
				"nonblocking failed");
			ABORT_FINALIZE(RS_RET_ERR);
		}
	}

	pWrkrData->bIsRunning = 1;
finalize_it:
	RETiRet;
}

/* As this is assume to be a debug function, we only make
 * best effort to write the message but do *not* try very
 * hard to handle errors. -- rgerhards, 2014-01-16
 */
static void
writeProgramOutput(wrkrInstanceData_t *__restrict__ const pWrkrData,
	const char *__restrict__ const buf,
	const ssize_t lenBuf)
{
	char errStr[1024];
	ssize_t r;

	if(pWrkrData->fdOutputFile == -1) {
		pWrkrData->fdOutputFile = open((char*)pWrkrData->pData->outputFileName,
				       O_WRONLY | O_APPEND | O_CREAT, 0600);
		if(pWrkrData->fdOutputFile == -1) {
			DBGPRINTF("omprog: error opening output file %s: %s\n",
				   pWrkrData->pData->outputFileName,
				   rs_strerror_r(errno, errStr, sizeof(errStr)));
			goto done;
		}
	}

	r = write(pWrkrData->fdOutputFile, buf, (size_t) lenBuf);
	if(r != lenBuf) {
		DBGPRINTF("omprog: problem writing output file %s: bytes "
			  "requested %lld, written %lld, msg: %s\n",
			   pWrkrData->pData->outputFileName, (long long) lenBuf, (long long) r,
			   rs_strerror_r(errno, errStr, sizeof(errStr)));
	}
done:	return;
}

/* check output of the executed program
 * If configured to care about the output, we check if there is some and,
 * if so, properly handle it.
 */
static void
checkProgramOutput(wrkrInstanceData_t *__restrict__ const pWrkrData)
{
	char buf[4096];
	ssize_t r;

	if(pWrkrData->fdPipeErr == -1)
		goto done;

	do {
		r = read(pWrkrData->fdPipeErr, buf, sizeof(buf));
		if(r > 0) {
			writeProgramOutput(pWrkrData, buf, r);
		}
	} while(r > 0);

done:	return;
}

#if defined(__linux__) && defined(_GNU_SOURCE)
typedef struct subprocess_timeout_desc_s {
	pthread_attr_t thd_attr;
	pthread_t thd;
	pthread_mutex_t lock;
	pthread_cond_t cond;
	int timeout_armed;
	pid_t waiter_tid;
	long timeout_ms;
	struct timespec timeout;
} subprocess_timeout_desc_t;

static void *
killSubprocessOnTimeout(void *_subpTimeOut_p) {
	subprocess_timeout_desc_t *subpTimeOut = (subprocess_timeout_desc_t *) _subpTimeOut_p;
	if (pthread_mutex_lock(&subpTimeOut->lock) == 0) {
		while (subpTimeOut->timeout_armed) {
			int ret = pthread_cond_timedwait(&subpTimeOut->cond, &subpTimeOut->lock,
				&subpTimeOut->timeout);
			if (subpTimeOut->timeout_armed && ((ret == ETIMEDOUT) || (timeoutVal(&subpTimeOut->timeout)
				== 0))) {
				errmsg.LogError(0, RS_RET_CONC_CTRL_ERR, "omprog: child-process wasn't "
					"reaped within timeout (%ld ms) preparing to force-kill.",
					subpTimeOut->timeout_ms);
				if (syscall(SYS_tgkill, getpid(), subpTimeOut->waiter_tid, SIGINT) != 0) {
					errmsg.LogError(errno, RS_RET_SYS_ERR, "omprog: couldn't interrupt "
						"thread(%d) which was waiting to reap child-process.",
						subpTimeOut->waiter_tid);
				}
			}
		}
		pthread_mutex_unlock(&subpTimeOut->lock);
	}
	return NULL;
}

static rsRetVal
setupSubprocessTimeout(subprocess_timeout_desc_t *subpTimeOut, long timeout_ms)
{
	int attr_initialized = 0, mutex_initialized = 0, cond_initialized = 0;
	DEFiRet;

	CHKiConcCtrl(pthread_attr_init(&subpTimeOut->thd_attr));
	attr_initialized = 1;
	CHKiConcCtrl(pthread_mutex_init(&subpTimeOut->lock, NULL));
	mutex_initialized = 1;
	CHKiConcCtrl(pthread_cond_init(&subpTimeOut->cond, NULL));
	cond_initialized = 1;
	/* mutex look to keep Coverity scan happen - not really necessary */
	pthread_mutex_lock(&subpTimeOut->lock);
	subpTimeOut->timeout_armed = 1;
	subpTimeOut->waiter_tid = syscall(SYS_gettid);
	subpTimeOut->timeout_ms = timeout_ms;
	pthread_mutex_unlock(&subpTimeOut->lock);
	CHKiRet(timeoutComp(&subpTimeOut->timeout, timeout_ms));
	CHKiConcCtrl(pthread_create(&subpTimeOut->thd, &subpTimeOut->thd_attr, killSubprocessOnTimeout, subpTimeOut));
finalize_it:
	if (iRet != RS_RET_OK) {
		if (attr_initialized) pthread_attr_destroy(&subpTimeOut->thd_attr);
		if (mutex_initialized) pthread_mutex_destroy(&subpTimeOut->lock);
		if (cond_initialized) pthread_cond_destroy(&subpTimeOut->cond);
	}
	RETiRet;
}

static void
doForceKillSubprocess(subprocess_timeout_desc_t *subpTimeOut, int do_kill, pid_t pid)
{
	if (pthread_mutex_lock(&subpTimeOut->lock) == 0) {
	        subpTimeOut->timeout_armed = 0;
	        pthread_cond_signal(&subpTimeOut->cond);
	        pthread_mutex_unlock(&subpTimeOut->lock);
	}
	pthread_join(subpTimeOut->thd, NULL);
	if (do_kill) {
		if (kill(pid, 9) == 0) {
			errmsg.LogError(0, RS_RET_NO_ERRCODE, "omprog: child-process FORCE-killed");
		} else {
			errmsg.LogError(errno, RS_RET_SYS_ERR, "omprog: child-process cound't be FORCE-killed");
		}
	}
	pthread_cond_destroy(&subpTimeOut->cond);
	pthread_mutex_destroy(&subpTimeOut->lock);
	pthread_attr_destroy(&subpTimeOut->thd_attr);
}
#endif

static void
waitForChild(wrkrInstanceData_t *pWrkrData, const long timeout_ms)
{
	int status;
	int ret;

#if defined(__linux__) && defined(_GNU_SOURCE)
	subprocess_timeout_desc_t subpTimeOut;
	int timeoutSetupStatus = 0;
	int waitpid_interrupted;

	if (timeout_ms > 0)
		timeoutSetupStatus = setupSubprocessTimeout(&subpTimeOut, timeout_ms);
#endif

	ret = waitpid(pWrkrData->pid, &status, 0);

#if defined(__linux__) && defined(_GNU_SOURCE)
	waitpid_interrupted = (ret != pWrkrData->pid) && (errno == EINTR);
	if ((timeout_ms > 0) && (timeoutSetupStatus == RS_RET_OK))
		doForceKillSubprocess(&subpTimeOut, waitpid_interrupted, pWrkrData->pid);
	if (waitpid_interrupted) {
		waitForChild(pWrkrData, -1);
		return;
	}
#endif

	if(ret != pWrkrData->pid) {
		if (errno == ECHILD) {
			errmsg.LogError(errno, RS_RET_OK_WARN, "Child %d doesn't seem to exist, "
							"hence couldn't be reaped (reaped by main-loop?)",
			pWrkrData->pid);
		} else {
			errmsg.LogError(errno, RS_RET_SYS_ERR, "Cleanup failed for child %d", pWrkrData->pid);
		}
	} else {
		/* check if we should print out some diagnostic information */
		DBGPRINTF("omprog: waitpid status return for program '%s': %2.2x\n",
				  pWrkrData->pData->szBinary, status);
		if(WIFEXITED(status)) {
			errmsg.LogError(0, NO_ERRCODE, "program '%s' exited normally, state %d",
							pWrkrData->pData->szBinary, WEXITSTATUS(status));
		} else if(WIFSIGNALED(status)) {
			errmsg.LogError(0, NO_ERRCODE, "program '%s' terminated by signal %d.",
							pWrkrData->pData->szBinary, WTERMSIG(status));
		}
	}
}

/* clean up the state after a terminated child
 */
static void
cleanupChild(wrkrInstanceData_t *pWrkrData, long timeout_ms)
{
	assert(pWrkrData->bIsRunning == 1);

	if(pWrkrData->pData->bSignalOnClose) {
		waitForChild(pWrkrData, timeout_ms);
	}

	checkProgramOutput(pWrkrData);  /* try to catch any late messages */

	if(pWrkrData->fdOutputFile != -1) {
		close(pWrkrData->fdOutputFile);
		pWrkrData->fdOutputFile = -1;
	}
	if(pWrkrData->fdPipeErr != -1) {
		close(pWrkrData->fdPipeErr);
		pWrkrData->fdPipeErr = -1;
	}
	if(pWrkrData->fdPipeIn != -1) {
		close(pWrkrData->fdPipeIn);
		pWrkrData->fdPipeIn = -1;
	}
	if(pWrkrData->fdPipeOut != -1) {
		close(pWrkrData->fdPipeOut);
		pWrkrData->fdPipeOut = -1;
	}
	pWrkrData->bIsRunning = 0;
}

/* Send SIGTERM to child process (if configured to do so), wait for
 * termination, and clean up the state.
 */
static void
terminateChild(wrkrInstanceData_t *pWrkrData)
{
	assert(pWrkrData->bIsRunning == 1);

	if (pWrkrData->pData->bSignalOnClose) {
		kill(pWrkrData->pid, SIGTERM);
	}
	cleanupChild(pWrkrData, DEFAULT_FORCED_TERMINATION_TIMEOUT_MS);
}

/* write to pipe
 * note that we do not try to run block-free. If the users fears something
 * may block (and this not be acceptable), the action should be run on its
 * own action queue.
 */
static rsRetVal
writePipe(wrkrInstanceData_t *pWrkrData, uchar *szMsg)
{
	int lenWritten;
	int lenWrite;
	int writeOffset;
	char errStr[1024];
	DEFiRet;

	lenWrite = strlen((char*)szMsg);
	writeOffset = 0;

	do {
		checkProgramOutput(pWrkrData);
		lenWritten = write(pWrkrData->fdPipeOut, ((char*)szMsg)+writeOffset, lenWrite);
		if(lenWritten == -1) {
			if(errno == EPIPE) {
				DBGPRINTF("omprog: program '%s' terminated, will be restarted\n",
					  pWrkrData->pData->szBinary);
				/* force restart in tryResume() */
				cleanupChild(pWrkrData, 0);
			} else {
				DBGPRINTF("omprog: error %d writing to pipe: %s\n", errno,
					   rs_strerror_r(errno, errStr, sizeof(errStr)));
			}
			ABORT_FINALIZE(RS_RET_SUSPENDED);
		}
		writeOffset += lenWritten;
	} while(lenWritten != lenWrite);

	checkProgramOutput(pWrkrData);

finalize_it:
	RETiRet;
}

/* Reads a line from a blocking pipe, using the unistd.h read() function.
 * Returns the line as a null-terminated string in *lineptr, not including
 * the \n or \r\n terminator.
 * On success, returns the line length.
 * On error, returns -1 and sets errno.
 * On EOF, returns -1 and sets errno to EPIPE.
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
		DBGPRINTF("omprog: program '%s' returned: %s\n",
			  pWrkrData->pData->szBinary, line);
		iRet = RS_RET_SUSPENDED;
	}
	RETiRet;
}

static rsRetVal
readPipe(wrkrInstanceData_t *pWrkrData)
{
	char *line;
	ssize_t lineLen;
	char errStr[1024];
	DEFiRet;

	lineLen = readline(pWrkrData->fdPipeIn, &line);
	if (lineLen == -1) {
		if (errno == EPIPE) {
			DBGPRINTF("omprog: program '%s' terminated, will be restarted\n",
				  pWrkrData->pData->szBinary);
			/* force restart in tryResume() */
			cleanupChild(pWrkrData, 0);
		} else {
			DBGPRINTF("omprog: error %d reading from pipe: %s\n", errno,
				   rs_strerror_r(errno, errStr, sizeof(errStr)));
		}
		ABORT_FINALIZE(RS_RET_SUSPENDED);
	}

	iRet = lineToStatusCode(pWrkrData, line);
	free(line);
finalize_it:
	RETiRet;
}

static rsRetVal
startChild(wrkrInstanceData_t *pWrkrData)
{
	DEFiRet;

	assert(pWrkrData->bIsRunning == 0);

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
	pWrkrData->fdPipeOut = -1;
	pWrkrData->fdPipeIn = -1;
	pWrkrData->fdPipeErr = -1;
	pWrkrData->fdOutputFile = -1;
	pWrkrData->bIsRunning = 0;

	iRet = startChild(pWrkrData);
ENDcreateWrkrInstance


BEGINisCompatibleWithFeature
CODESTARTisCompatibleWithFeature
	if(eFeat == sFEATURERepeatedMsgReduction)
		iRet = RS_RET_OK;
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
	free(pData->szBinary);
	free(pData->outputFileName);
	free(pData->tplName);
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
	pData->aParams = NULL;
	pData->iParams = 0;
	pData->bConfirmMessages = 0;
	pData->bUseTransactions = 0;
	pData->szBeginTransactionMark = (uchar*) "BEGIN TRANSACTION";
	pData->szCommitTransactionMark = (uchar*) "COMMIT TRANSACTION";
	pData->bForceSingleInst = 0;
	pData->iHUPForward = NO_HUP_FORWARD;
	pData->bSignalOnClose = 0;
	pData->outputFileName = NULL;
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
		} else if(!strcmp(actpblk.descr[i].name, "output")) {
			pData->outputFileName = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(actpblk.descr[i].name, "forcesingleinstance")) {
			pData->bForceSingleInst = (int) pvals[i].val.d.n;
		} else if(!strcmp(actpblk.descr[i].name, "signalOnClose")) {
			pData->bSignalOnClose = (int) pvals[i].val.d.n;
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
				errmsg.LogError(0, RS_RET_CONF_PARAM_INVLD,
					"omprog: hup.signal '%s' in hup.signal parameter", sig);
				ABORT_FINALIZE(RS_RET_CONF_PARAM_INVLD);
			}
			free((void*)sig);
		} else if(!strcmp(actpblk.descr[i].name, "template")) {
			pData->tplName = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else {
			DBGPRINTF("omprog: program error, non-handled param '%s'\n", actpblk.descr[i].name);
		}
	}

	CHKiRet(OMSRsetEntry(*ppOMSR, 0, (uchar*)strdup((pData->tplName == NULL) ?
						"RSYSLOG_FileFormat" : (char*)pData->tplName),
						OMSR_NO_RQD_TPL_OPTS));
	DBGPRINTF("omprog: bForceSingleInst %d\n", pData->bForceSingleInst);
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
		errmsg.LogError(0, RS_RET_CONF_RQRD_PARAM_MISSING,
			"no binary to execute specified");
		ABORT_FINALIZE(RS_RET_CONF_RQRD_PARAM_MISSING);
	}

	CHKiRet(createInstance(&pData));

	if(cs.szBinary == NULL) {
		errmsg.LogError(0, RS_RET_CONF_RQRD_PARAM_MISSING,
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


BEGINdoHUPWrkr
CODESTARTdoHUPWrkr
	DBGPRINTF("omprog: processing HUP for work instance %p, pid %d, forward: %d\n",
		pWrkrData, (int) pWrkrData->pid, pWrkrData->pData->iHUPForward);
	if(pWrkrData->pData->iHUPForward != NO_HUP_FORWARD)
		kill(pWrkrData->pid, pWrkrData->pData->iHUPForward);
ENDdoHUPWrkr


BEGINmodExit
CODESTARTmodExit
	free(cs.szBinary);
	cs.szBinary = NULL;
	iRet = objRelease(errmsg, CORE_COMPONENT);
ENDmodExit


BEGINqueryEtryPt
CODESTARTqueryEtryPt
CODEqueryEtryPt_STD_OMOD_QUERIES
CODEqueryEtryPt_STD_OMOD8_QUERIES
CODEqueryEtryPt_STD_CONF2_OMOD_QUERIES
CODEqueryEtryPt_TXIF_OMOD_QUERIES /* we support the transactional interface */
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
	CHKiRet(objUse(errmsg, CORE_COMPONENT));

	/* check that rsyslog core supports transactional plugins */
	INITChkCoreFeature(bCoreSupportsBatching, CORE_FEATURE_BATCHING);
	if (!bCoreSupportsBatching) {
		errmsg.LogError(0, NO_ERRCODE, "omprog: rsyslog core too old (does not support batching)");
		ABORT_FINALIZE(RS_RET_ERR);
	}

	CHKiRet(omsdRegCFSLineHdlr((uchar *)"actionomprogbinary", 0, eCmdHdlrGetWord, NULL, &cs.szBinary,
		STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"resetconfigvariables", 1, eCmdHdlrCustomHandler, resetConfigVariables,
		NULL, STD_LOADABLE_MODULE_ID));
CODEmodInit_QueryRegCFSLineHdlr
ENDmodInit
