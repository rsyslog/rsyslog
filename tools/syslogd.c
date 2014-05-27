/**
 * \brief This is the main file of the rsyslogd daemon.
 *
 * Please visit the rsyslog project at
 *
 * http://www.rsyslog.com
 *
 * to learn more about it and discuss any questions you may have.
 *
 * rsyslog had initially been forked from the sysklogd project.
 * I would like to express my thanks to the developers of the sysklogd
 * package - without it, I would have had a much harder start...
 *
 * Please note that while rsyslog started from the sysklogd code base,
 * it nowadays has almost nothing left in common with it. Allmost all
 * parts of the code have been rewritten.
 *
 * This Project was intiated and is maintained by
 * Rainer Gerhards <rgerhards@hq.adiscon.com>.
 *
 * For further information, please see http://www.rsyslog.com
 *
 * rsyslog - An Enhanced syslogd Replacement.
 * Copyright 2003-2014 Rainer Gerhards and Adiscon GmbH.
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
#include "rsyslog.h"

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <ctype.h>
#include <limits.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <assert.h>

#ifdef OS_SOLARIS
#	include <errno.h>
#	include <fcntl.h>
#	include <stropts.h>
#	include <sys/termios.h>
#	include <sys/types.h>
#else
#	include <libgen.h>
#	include <sys/errno.h>
#endif

#include <sys/ioctl.h>
#include <sys/wait.h>
#include <sys/file.h>
#include <sys/resource.h>
#include <grp.h>

#if HAVE_SYS_TIMESPEC_H
# include <sys/timespec.h>
#endif

#if HAVE_SYS_STAT_H
#	include <sys/stat.h>
#endif

#include <signal.h>

#if HAVE_PATHS_H
#include <paths.h>
#endif

extern int yydebug; /* interface to flex */

#include <netdb.h>

#include "pidfile.h"
#include "srUtils.h"
#include "stringbuf.h"
#include "syslogd-types.h"
#include "template.h"
#include "outchannel.h"
#include "syslogd.h"

#include "msg.h"
#include "iminternal.h"
#include "threads.h"
#include "errmsg.h"
#include "datetime.h"
#include "parser.h"
#include "unicode-helper.h"
#include "net.h"
#include "rsconf.h"
#include "dnscache.h"
#include "sd-daemon.h"
#include "ratelimit.h"

/* definitions for objects we access */
DEFobjCurrIf(obj)
DEFobjCurrIf(glbl)
DEFobjCurrIf(datetime) /* TODO: make go away! */
DEFobjCurrIf(module)
DEFobjCurrIf(errmsg)
DEFobjCurrIf(rsconf)
DEFobjCurrIf(net) /* TODO: make go away! */


/* forward definitions */
rsRetVal queryLocalHostname(void);

/* forward defintions from rsyslogd.c (ASL 2.0 code) */
extern ratelimit_t *internalMsg_ratelimiter;
extern uchar *ConfFile;
extern ratelimit_t *dflt_ratelimiter;
extern void rsyslogd_usage(void);
extern rsRetVal rsyslogdInit(void);
extern void rsyslogd_destructAllActions(void);
extern void rsyslogd_sigttin_handler();
void rsyslogd_submitErrMsg(const int severity, const int iErr, const uchar *msg);
rsRetVal rsyslogd_InitGlobalClasses(void);
rsRetVal rsyslogd_InitStdRatelimiters(void);


#if defined(SYSLOGD_PIDNAME)
#	undef _PATH_LOGPID
#	define _PATH_LOGPID "/etc/" SYSLOGD_PIDNAME
#else
#	ifndef _PATH_LOGPID
#		define _PATH_LOGPID "/etc/rsyslogd.pid"
#	endif
#endif

#ifndef _PATH_TTY
#	define _PATH_TTY	"/dev/tty"
#endif

rsconf_t *ourConf;				/* our config object */

static char	*PidFile = _PATH_LOGPID; /* read-only after startup */

/* mypid is read-only after the initial fork() */
int bHadHUP = 0; /* did we have a HUP? */

int bFinished = 0;	/* used by termination signal handler, read-only except there
				 * is either 0 or the number of the signal that requested the
 				 * termination.
				 */
int iConfigVerify = 0;	/* is this just a config verify run? */

#define LIST_DELIMITER	':'		/* delimiter between two hosts */

static pid_t ppid; /* This is a quick and dirty hack used for spliting main/startup thread */

int      send_to_all = 0;        /* send message to all IPv4/IPv6 addresses */
int	doFork = 1; 	/* fork - run in daemon mode - read-only after startup */


/* up to the next comment, prototypes that should be removed by reordering */
/* Function prototypes. */
static char **crunch_list(char *list);
static void reapchild();
static void debug_switch();
static void sighup_handler();


/* rgerhards, 2005-10-24: crunch_list is called only during option processing. So
 * it is never called once rsyslogd is running. This code
 * contains some exits, but they are considered safe because they only happen
 * during startup. Anyhow, when we review the code here, we might want to
 * reconsider the exit()s.
 * Note: this stems back to sysklogd, so we cannot put it under ASL 2.0. But
 * we may want to check if the code inside the BSD sources is exactly the same
 * (remember that sysklogd forked the BSD sources). If so, the BSD license applies
 * and permits us to move to ASL 2.0 (but we need to check the fine details). 
 * Probably it is best just to rewrite this code.
 */
static char **crunch_list(char *list)
{
	int count, i;
	char *p, *q;
	char **result = NULL;

	p = list;

	/* strip off trailing delimiters */
	while (p[strlen(p)-1] == LIST_DELIMITER) {
		count--;
		p[strlen(p)-1] = '\0';
	}
	/* cut off leading delimiters */
	while (p[0] == LIST_DELIMITER) {
		count--;
               p++;
	}

	/* count delimiters to calculate elements */
	for (count=i=0; p[i]; i++)
		if (p[i] == LIST_DELIMITER) count++;

	if ((result = (char **)MALLOC(sizeof(char *) * (count+2))) == NULL) {
		printf ("Sorry, can't get enough memory, exiting.\n");
		exit(0); /* safe exit, because only called during startup */
	}

	/*
	 * We now can assume that the first and last
	 * characters are different from any delimiters,
	 * so we don't have to care about this.
	 */
	count = 0;
	while ((q=strchr(p, LIST_DELIMITER))) {
		result[count] = (char *) MALLOC((q - p + 1) * sizeof(char));
		if (result[count] == NULL) {
			printf ("Sorry, can't get enough memory, exiting.\n");
			exit(0); /* safe exit, because only called during startup */
		}
		strncpy(result[count], p, q - p);
		result[count][q - p] = '\0';
		p = q; p++;
		count++;
	}
	if ((result[count] = \
	     (char *)MALLOC(sizeof(char) * strlen(p) + 1)) == NULL) {
		printf ("Sorry, can't get enough memory, exiting.\n");
		exit(0); /* safe exit, because only called during startup */
	}
	strcpy(result[count],p);
	result[++count] = NULL;

	return result;
}


/* also stems back to sysklogd in whole */
void untty(void)
#ifdef HAVE_SETSID
{
	if(!Debug) {
		setsid();
	}
	return;
}
#else
{
	int i;
	pid_t pid;

	if(!Debug) {
		/* Peng Haitao <penght@cn.fujitsu.com> contribution */
		pid = getpid();
		if (setpgid(pid, pid) < 0) {
			perror("setpgid");
			exit(1);
		}
		/* end Peng Haitao <penght@cn.fujitsu.com> contribution */

		i = open(_PATH_TTY, O_RDWR|O_CLOEXEC);
		if (i >= 0) {
#			if !defined(__hpux)
				(void) ioctl(i, (int) TIOCNOTTY, NULL);
#			else
				/* TODO: we need to implement something for HP UX! -- rgerhards, 2008-03-04 */
				/* actually, HP UX should have setsid, so the code directly above should
				 * trigger. So the actual question is why it doesn't do that...
				 */
#			endif
			close(i);
		}
	}
}
#endif

/* all functions stems back to sysklogd */
static void
reapchild()
{
	int saved_errno = errno;
	struct sigaction sigAct;

	memset(&sigAct, 0, sizeof (sigAct));
	sigemptyset(&sigAct.sa_mask);
	sigAct.sa_handler = reapchild;
	sigaction(SIGCHLD, &sigAct, NULL);  /* reset signal handler -ASP */

	while(waitpid(-1, NULL, WNOHANG) > 0);
	errno = saved_errno;
}


static void debug_switch()
{
	time_t tTime;
	struct tm tp;
	struct sigaction sigAct;

	datetime.GetTime(&tTime);
	localtime_r(&tTime, &tp);
	if(debugging_on == 0) {
		debugging_on = 1;
		dbgprintf("\n");
		dbgprintf("\n");
		dbgprintf("********************************************************************************\n");
		dbgprintf("Switching debugging_on to true at %2.2d:%2.2d:%2.2d\n",
			  tp.tm_hour, tp.tm_min, tp.tm_sec);
		dbgprintf("********************************************************************************\n");
	} else {
		dbgprintf("********************************************************************************\n");
		dbgprintf("Switching debugging_on to false at %2.2d:%2.2d:%2.2d\n",
			  tp.tm_hour, tp.tm_min, tp.tm_sec);
		dbgprintf("********************************************************************************\n");
		dbgprintf("\n");
		dbgprintf("\n");
		debugging_on = 0;
	}

	memset(&sigAct, 0, sizeof (sigAct));
	sigemptyset(&sigAct.sa_mask);
	sigAct.sa_handler = debug_switch;
	sigaction(SIGUSR1, &sigAct, NULL);
}


/* doDie() is a signal handler. If called, it sets the bFinished variable
 * to indicate the program should terminate. However, it does not terminate
 * it itself, because that causes issues with multi-threading. The actual
 * termination is then done on the main thread. This solution might introduce
 * a minimal delay, but it is much cleaner than the approach of doing everything
 * inside the signal handler.
 * rgerhards, 2005-10-26
 * Note:
 * - we do not call DBGPRINTF() as this may cause us to block in case something
 *   with the threading is wrong.
 * - we do not really care about the return state of write(), but we need this
 *   strange check we do to silence compiler warnings (thanks, Ubuntu!)
 */
static void doDie(int sig)
{
#	define MSG1 "DoDie called.\n"
#	define MSG2 "DoDie called 5 times - unconditional exit\n"
	static int iRetries = 0; /* debug aid */
	dbgprintf(MSG1);
	if(Debug == DEBUG_FULL) {
		if(write(1, MSG1, sizeof(MSG1) - 1)) {}
	}
	if(iRetries++ == 4) {
		if(Debug == DEBUG_FULL) {
			if(write(1, MSG2, sizeof(MSG2) - 1)) {}
		}
		abort();
	}
	bFinished = sig;
	if(glblDebugOnShutdown) {
		/* kind of hackish - set to 0, so that debug_swith will enable
		 * and AND emit the "start debug log" message.
		 */
		debugging_on = 0;
		debug_switch();
	}
#	undef MSG1
#	undef MSG2
}


/* GPL code - maybe check BSD sources? */
void
syslogd_die(void)
{
	remove_pid(PidFile);
}

/*
 * Signal handler to terminate the parent process.
 * rgerhards, 2005-10-24: this is only called during forking of the
 * detached syslogd. I consider this method to be safe.
 */
static void doexit()
{
	exit(0); /* "good" exit, only during child-creation */
}


/* INIT -- Initialize syslogd
 * Note that if iConfigVerify is set, only the config file is verified but nothing
 * else happens. -- rgerhards, 2008-07-28
 */
static rsRetVal
init(void)
{
	char bufStartUpMsg[512];
	struct sigaction sigAct;
	DEFiRet;

	memset(&sigAct, 0, sizeof (sigAct));
	sigemptyset(&sigAct.sa_mask);
	sigAct.sa_handler = sighup_handler;
	sigaction(SIGHUP, &sigAct, NULL);

	CHKiRet(rsconf.Activate(ourConf));
	DBGPRINTF(" started.\n");

	/* we now generate the startup message. It now includes everything to
	 * identify this instance. -- rgerhards, 2005-08-17
	 */
	if(ourConf->globals.bLogStatusMsgs) {
               snprintf(bufStartUpMsg, sizeof(bufStartUpMsg)/sizeof(char),
			 " [origin software=\"rsyslogd\" " "swVersion=\"" VERSION \
			 "\" x-pid=\"%d\" x-info=\"http://www.rsyslog.com\"] start",
			 (int) glblGetOurPid());
		logmsgInternal(NO_ERRCODE, LOG_SYSLOG|LOG_INFO, (uchar*)bufStartUpMsg, 0);
	}

finalize_it:
	RETiRet;
}


/*
 * The following function is resposible for handling a SIGHUP signal.  Since
 * we are now doing mallocs/free as part of init we had better not being
 * doing this during a signal handler.  Instead this function simply sets
 * a flag variable which will tells the main loop to do "the right thing".
 */
void sighup_handler()
{
	struct sigaction sigAct;

	bHadHUP = 1;

	memset(&sigAct, 0, sizeof (sigAct));
	sigemptyset(&sigAct.sa_mask);
	sigAct.sa_handler = sighup_handler;
	sigaction(SIGHUP, &sigAct, NULL);
}

/* print version and compile-time setting information.
 */
static void printVersion(void)
{
	printf("rsyslogd %s, ", VERSION);
	printf("compiled with:\n");
#ifdef FEATURE_REGEXP
	printf("\tFEATURE_REGEXP:\t\t\t\tYes\n");
#else
	printf("\tFEATURE_REGEXP:\t\t\t\tNo\n");
#endif
/* Yann Droneaud <yann@droneaud.fr> contribution */
#if defined(_LARGE_FILES) || (defined (_FILE_OFFSET_BITS) && _FILE_OFFSET_BITS >= 64)
/* end Yann Droneaud <yann@droneaud.fr> contribution */
	printf("\tFEATURE_LARGEFILE:\t\t\tYes\n");
#else
	printf("\tFEATURE_LARGEFILE:\t\t\tNo\n");
#endif
#if defined(SYSLOG_INET) && defined(USE_GSSAPI)
	printf("\tGSSAPI Kerberos 5 support:\t\tYes\n");
#else
	printf("\tGSSAPI Kerberos 5 support:\t\tNo\n");
#endif
#ifndef	NDEBUG
	printf("\tFEATURE_DEBUG (debug build, slow code):\tYes\n");
#else
	printf("\tFEATURE_DEBUG (debug build, slow code):\tNo\n");
#endif
#ifdef	HAVE_ATOMIC_BUILTINS
	printf("\t32bit Atomic operations supported:\tYes\n");
#else
	printf("\t32bit Atomic operations supported:\tNo\n");
#endif
/* 	mono_matsuko <aiueov@hotmail.co.jp> contribution */
#ifdef	HAVE_ATOMIC_BUILTINS_64BIT
/* end	mono_matsuko <aiueov@hotmail.co.jp> contribution */
	printf("\t64bit Atomic operations supported:\tYes\n");
#else
	printf("\t64bit Atomic operations supported:\tNo\n");
#endif
#ifdef	HAVE_JEMALLOC
	printf("\tmemory allocator:\t\t\tjemalloc\n");
#else
	printf("\tmemory allocator:\t\t\tsystem default\n");
#endif
#ifdef	RTINST
	printf("\tRuntime Instrumentation (slow code):\tYes\n");
#else
	printf("\tRuntime Instrumentation (slow code):\tNo\n");
#endif
#ifdef	USE_LIBUUID
	printf("\tuuid support:\t\t\t\tYes\n");
#else
	printf("\tuuid support:\t\t\t\tNo\n");
#endif
#ifdef HAVE_JSON_OBJECT_NEW_INT64
	printf("\tNumber of Bits in RainerScript integers: 64\n");
#else
	printf("\tNumber of Bits in RainerScript integers: 32 (due to too-old json-c lib)\n");
#endif
	printf("\nSee http://www.rsyslog.com for more information.\n");
}


/* obtain ptrs to all clases we need.  */
static rsRetVal
obtainClassPointers(void)
{
	DEFiRet;
	char *pErrObj; /* tells us which object failed if that happens (useful for troubleshooting!) */

	CHKiRet(objGetObjInterface(&obj)); /* this provides the root pointer for all other queries */

	/* Now tell the system which classes we need ourselfs */
	pErrObj = "glbl";
	CHKiRet(objUse(glbl,     CORE_COMPONENT));
	pErrObj = "errmsg";
	CHKiRet(objUse(errmsg,   CORE_COMPONENT));
	pErrObj = "module";
	CHKiRet(objUse(module,   CORE_COMPONENT));
	pErrObj = "datetime";
	CHKiRet(objUse(datetime, CORE_COMPONENT));
	pErrObj = "rsconf";
	CHKiRet(objUse(rsconf,     CORE_COMPONENT));

	/* TODO: the dependency on net shall go away! -- rgerhards, 2008-03-07 */
	pErrObj = "net";
	CHKiRet(objUse(net, LM_NET_FILENAME));

finalize_it:
	if(iRet != RS_RET_OK) {
		/* we know we are inside the init sequence, so we can safely emit
		 * messages to stderr. -- rgerhards, 2008-04-02
		 */
		fprintf(stderr, "Error obtaining object '%s' - failing...\n", pErrObj);
	}

	RETiRet;
}


void
syslogd_releaseClassPointers(void)
{
	objRelease(net,      LM_NET_FILENAME);/* TODO: the dependency on net shall go away! -- rgerhards, 2008-03-07 */
	objRelease(datetime, CORE_COMPONENT);
}


/* query our host and domain names - we need to do this early as we may emit
 * rgerhards, 2012-04-11
 */
rsRetVal
queryLocalHostname(void)
{
	uchar *LocalHostName;
	uchar *LocalDomain;
	uchar *LocalFQDNName;
	uchar *p;
	struct hostent *hent;
	DEFiRet;

	net.getLocalHostname(&LocalFQDNName);
	CHKmalloc(LocalHostName = (uchar*) strdup((char*)LocalFQDNName));
	glbl.SetLocalFQDNName(LocalFQDNName); /* set the FQDN before we modify it */
	if((p = (uchar*)strchr((char*)LocalHostName, '.'))) {
		*p++ = '\0';
		LocalDomain = p;
	} else {
		LocalDomain = (uchar*)"";

		/* It's not clearly defined whether gethostname()
		 * should return the simple hostname or the fqdn. A
		 * good piece of software should be aware of both and
		 * we want to distribute good software.  Joey
		 *
		 * Good software also always checks its return values...
		 * If syslogd starts up before DNS is up & /etc/hosts
		 * doesn't have LocalHostName listed, gethostbyname will
                * return NULL.
		 */
		/* TODO: gethostbyname() is not thread-safe, but replacing it is
		 * not urgent as we do not run on multiple threads here. rgerhards, 2007-09-25
		 */
		hent = gethostbyname((char*)LocalHostName);
		if(hent) {
			int i = 0;

			if(hent->h_aliases) {
				size_t hnlen;

				hnlen = strlen((char *) LocalHostName);

				for (i = 0; hent->h_aliases[i]; i++) {
					if (!strncmp(hent->h_aliases[i], (char *) LocalHostName, hnlen)
					    && hent->h_aliases[i][hnlen] == '.') {
						/* found a matching hostname */
						break;
					}
				}
			}

			free(LocalHostName);
			if(hent->h_aliases && hent->h_aliases[i]) {
				CHKmalloc(LocalHostName = (uchar*)strdup(hent->h_aliases[i]));
			} else {
				CHKmalloc(LocalHostName = (uchar*)strdup(hent->h_name));
			}

			if((p = (uchar*)strchr((char*)LocalHostName, '.')))
			{
				*p++ = '\0';
				LocalDomain = p;
			}
		}
	}

	/* Marius Tomaschewski <mt@suse.com> contribution */
	/* LocalDomain is "" or part of LocalHostName, allocate a new string */
	CHKmalloc(LocalDomain = (uchar*)strdup((char*)LocalDomain));
	/* Marius Tomaschewski <mt@suse.com> contribution */

	/* Convert to lower case to recognize the correct domain laterly */
	for(p = LocalDomain ; *p ; p++)
		*p = (char)tolower((int)*p);

	/* we now have our hostname and can set it inside the global vars.
	 * TODO: think if all of this would better be a runtime function
	 * rgerhards, 2008-04-17
	 */
	glbl.SetLocalHostName(LocalHostName);
	glbl.SetLocalDomain(LocalDomain);

	/* Canonical contribution - ASL 2.0 fine (email exchange 2014-05-27) */
	if ( strlen((char*)LocalDomain) )  {
		CHKmalloc(LocalFQDNName = (uchar*)malloc(strlen((char*)LocalDomain)+strlen((char*)LocalHostName)+2));/* one for dot, one for NUL! */
		if ( sprintf((char*)LocalFQDNName,"%s.%s",(char*)LocalHostName,(char*)LocalDomain) )
			glbl.SetLocalFQDNName(LocalFQDNName);
		}
	/* end canonical contrib */

	glbl.GenerateLocalHostNameProperty(); /* must be redone after conf processing, FQDN setting may have changed */
finalize_it:
	RETiRet;
}


/* some support for command line option parsing. Any non-trivial options must be
 * buffered until the complete command line has been parsed. This is necessary to
 * prevent dependencies between the options. That, in turn, means we need to have
 * something that is capable of buffering options and there values. The follwing
 * functions handle that.
 * rgerhards, 2008-04-04
 */
typedef struct bufOpt {
	struct bufOpt *pNext;
	char optchar;
	char *arg;
} bufOpt_t;
static bufOpt_t *bufOptRoot = NULL;
static bufOpt_t *bufOptLast = NULL;

/* add option buffer */
static rsRetVal
bufOptAdd(char opt, char *arg)
{
	DEFiRet;
	bufOpt_t *pBuf;

	if((pBuf = MALLOC(sizeof(bufOpt_t))) == NULL)
		ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);

	pBuf->optchar = opt;
	pBuf->arg = arg;
	pBuf->pNext = NULL;

	if(bufOptLast == NULL) {
		bufOptRoot = pBuf; /* then there is also no root! */
	} else {
		bufOptLast->pNext = pBuf;
	}
	bufOptLast = pBuf;

finalize_it:
	RETiRet;
}



/* remove option buffer from top of list, return values and destruct buffer itself.
 * returns RS_RET_END_OF_LINKEDLIST when no more options are present.
 * (we use int *opt instead of char *opt to keep consistent with getopt())
 */
static rsRetVal
bufOptRemove(int *opt, char **arg)
{
	DEFiRet;
	bufOpt_t *pBuf;

	if(bufOptRoot == NULL)
		ABORT_FINALIZE(RS_RET_END_OF_LINKEDLIST);
	pBuf = bufOptRoot;

	*opt = pBuf->optchar;
	*arg = pBuf->arg;

	bufOptRoot = pBuf->pNext;
	free(pBuf);

finalize_it:
	RETiRet;
}


/* global initialization, to be done only once and before the mainloop is started.
 * rgerhards, 2008-07-28 (extracted from realMain())
 */
static rsRetVal
doGlblProcessInit(void)
{
	struct sigaction sigAct;
	int num_fds;
	int i;
	DEFiRet;

	thrdInit();

	if(doFork) {
		DBGPRINTF("Checking pidfile '%s'.\n", PidFile);
		if (!check_pid(PidFile))
		{
			memset(&sigAct, 0, sizeof (sigAct));
			sigemptyset(&sigAct.sa_mask);
			sigAct.sa_handler = doexit;
			sigaction(SIGTERM, &sigAct, NULL);

			/* RH contribution */
			/* stop writing debug messages to stdout (if debugging is on) */
			stddbg = -1;
			/* end RH contribution */

			dbgprintf("ready for forking\n");
			if (fork()) {
				/* Parent process
				 */
				dbgprintf("parent process going to sleep for 60 secs\n");
				sleep(60);
				/* Not reached unless something major went wrong.  1
				 * minute should be a fair amount of time to wait.
				 * The parent should not exit before rsyslogd is 
				 * properly initilized (at least almost) or the init
				 * system may get a wrong impression of our readyness.
				 * Note that we exit before being completely initialized,
				 * but at this point it is very, very unlikely that something
				 * bad can happen. We do this here, because otherwise we would
				 * need to have much more code to handle priv drop (which we
				 * don't consider worth for the init system, especially as it
				 * is going away on the majority of distros).
				 */
				exit(1); /* "good" exit - after forking, not diasabling anything */
			}

			num_fds = getdtablesize();
			close(0);
			/* we keep stdout and stderr open in case we have to emit something */
			i = 3;
			dbgprintf("in child, finalizing initialization\n");

			/* if (sd_booted()) */ {
				const char *e;
				char buf[24] = { '\0' };
				char *p = NULL;
				unsigned long l;
				int sd_fds;

				/* fork & systemd socket activation:
				 * fetch listen pid and update to ours,
				 * when it is set to pid of our parent.
				 */
				if ( (e = getenv("LISTEN_PID"))) {
					errno = 0;
					l = strtoul(e, &p, 10);
					if (errno ==  0 && l > 0 && (!p || !*p)) {
						if (getppid() == (pid_t)l) {
							snprintf(buf, sizeof(buf), "%d",
								 getpid());
							setenv("LISTEN_PID", buf, 1);
						}
					}
				}

				/*
				 * close only all further fds, except
				 * of the fds provided by systemd.
				 */
				sd_fds = sd_listen_fds(0);
				if (sd_fds > 0)
					i = SD_LISTEN_FDS_START + sd_fds;
			}
			for ( ; i < num_fds; i++)
				if(i != dbgGetDbglogFd())
					close(i);

			untty();
		} else {
			fputs(" Already running. If you want to run multiple instances, you need "
			      "to specify different pid files (use -i option)\n", stderr);
			exit(1); /* "good" exit, done if syslogd is already running */
		}
	}

	/* tuck my process id away */
	DBGPRINTF("Writing pidfile '%s'.\n", PidFile);
	if (!check_pid(PidFile))
	{
		if (!write_pid(PidFile))
		{
			fputs("Can't write pid.\n", stderr);
			exit(1); /* exit during startup - questionable */
		}
	}
	else
	{
		fputs("Pidfile (and pid) already exist.\n", stderr);
		exit(1); /* exit during startup - questionable */
	}
	glblSetOurPid(getpid());

	memset(&sigAct, 0, sizeof (sigAct));
	sigemptyset(&sigAct.sa_mask);

	sigAct.sa_handler = sigsegvHdlr;
	sigaction(SIGSEGV, &sigAct, NULL);
	sigAct.sa_handler = sigsegvHdlr;
	sigaction(SIGABRT, &sigAct, NULL);
	sigAct.sa_handler = doDie;
	sigaction(SIGTERM, &sigAct, NULL);
	sigAct.sa_handler = Debug ? doDie : SIG_IGN;
	sigaction(SIGINT, &sigAct, NULL);
	sigaction(SIGQUIT, &sigAct, NULL);
	sigAct.sa_handler = reapchild;
	sigaction(SIGCHLD, &sigAct, NULL);
	sigAct.sa_handler = Debug ? debug_switch : SIG_IGN;
	sigaction(SIGUSR1, &sigAct, NULL);
	sigAct.sa_handler = rsyslogd_sigttin_handler;
	sigaction(SIGTTIN, &sigAct, NULL); /* (ab)used to interrupt input threads */
	sigAct.sa_handler = SIG_IGN;
	sigaction(SIGPIPE, &sigAct, NULL);
	sigaction(SIGXFSZ, &sigAct, NULL); /* do not abort if 2gig file limit is hit */

	RETiRet;
}


/* This is the main entry point into rsyslogd. Over time, we should try to
 * modularize it a bit more...
 */
void
syslogdInit(int argc, char **argv)
{
	rsRetVal localRet;
	int ch;
	extern int optind;
	extern char *optarg;
	int bEOptionWasGiven = 0;
	int iHelperUOpt;
	int bChDirRoot = 1; /* change the current working directory to "/"? */
	char *arg;	/* for command line option processing */
	char cwdbuf[128]; /* buffer to obtain/display current working directory */
	DEFiRet;

	/* first, parse the command line options. We do not carry out any actual work, just
	 * see what we should do. This relieves us from certain anomalies and we can process
	 * the parameters down below in the correct order. For example, we must know the
	 * value of -M before we can do the init, but at the same time we need to have
	 * the base classes init before we can process most of the options. Now, with the
	 * split of functionality, this is no longer a problem. Thanks to varmofekoj for
	 * suggesting this algo.
	 * Note: where we just need to set some flags and can do so without knowledge
	 * of other options, we do this during the inital option processing.
	 * rgerhards, 2008-04-04
	 */
	while((ch = getopt(argc, argv, "46a:Ac:dDef:g:hi:l:m:M:nN:op:qQr::s:S:t:T:u:vwx")) != EOF) {
		switch((char)ch) {
                case '4':
                case '6':
                case 'A':
                case 'a':
		case 'f': /* configuration file */
		case 'h':
		case 'i': /* pid file name */
		case 'l':
		case 'm': /* mark interval */
		case 'n': /* don't fork */
		case 'N': /* enable config verify mode */
                case 'o':
                case 'p':
		case 'q': /* add hostname if DNS resolving has failed */
		case 'Q': /* dont resolve hostnames in ACL to IPs */
		case 's':
		case 'S': /* Source IP for local client to be used on multihomed host */
		case 'T': /* chroot on startup (primarily for testing) */
		case 'u': /* misc user settings */
		case 'w': /* disable disallowed host warnings */
		case 'x': /* disable dns for remote messages */
		case 'g': /* enable tcp gssapi logging */
		case 'r': /* accept remote messages */
		case 't': /* enable tcp logging */
			CHKiRet(bufOptAdd(ch, optarg));
			break;
		case 'c':		/* compatibility mode */
			fprintf(stderr, "rsyslogd: error: option -c is no longer supported - ignored\n");
			break;
		case 'd': /* debug - must be handled now, so that debug is active during init! */
			debugging_on = 1;
			Debug = 1;
			yydebug = 1;
			break;
		case 'D': /* BISON debug */
			yydebug = 1;
			break;
		case 'e':		/* log every message (no repeat message supression) */
			bEOptionWasGiven = 1;
			break;
		case 'M': /* default module load path -- this MUST be carried out immediately! */
			glblModPath = (uchar*) optarg;
			break;
		case 'v': /* MUST be carried out immediately! */
			printVersion();
			exit(0); /* exit for -v option - so this is a "good one" */
		case '?':
		default:
			rsyslogd_usage();
		}
	}

	if(argc - optind)
		rsyslogd_usage();

	DBGPRINTF("rsyslogd %s startup, module path '%s', cwd:%s\n",
		  VERSION, glblModPath == NULL ? "" : (char*)glblModPath,
		  getcwd(cwdbuf, sizeof(cwdbuf)));

	/* we are done with the initial option parsing and processing. Now we init the system. */

	ppid = getpid();

	CHKiRet(rsyslogd_InitGlobalClasses());
	CHKiRet(obtainClassPointers());

	/* doing some core initializations */

	/* get our host and domain names - we need to do this early as we may emit
	 * error log messages, which need the correct hostname. -- rgerhards, 2008-04-04
	 */
	queryLocalHostname();

	/* initialize the objects */
	if((iRet = modInitIminternal()) != RS_RET_OK) {
		fprintf(stderr, "fatal error: could not initialize errbuf object (error code %d).\n",
			iRet);
		exit(1); /* "good" exit, leaving at init for fatal error */
	}


	/* END core initializations - we now come back to carrying out command line options*/

	while((iRet = bufOptRemove(&ch, &arg)) == RS_RET_OK) {
		DBGPRINTF("deque option %c, optarg '%s'\n", ch, (arg == NULL) ? "" : arg);
		switch((char)ch) {
                case '4':
	                glbl.SetDefPFFamily(PF_INET);
                        break;
                case '6':
                        glbl.SetDefPFFamily(PF_INET6);
                        break;
                case 'A':
                        send_to_all++;
                        break;
                case 'a':
			fprintf(stderr, "rsyslogd: error -a is no longer supported, use module imuxsock instead");
                        break;
		case 'S':		/* Source IP for local client to be used on multihomed host */
			if(glbl.GetSourceIPofLocalClient() != NULL) {
				fprintf (stderr, "rsyslogd: Only one -S argument allowed, the first one is taken.\n");
			} else {
				glbl.SetSourceIPofLocalClient((uchar*)arg);
			}
			break;
		case 'f':		/* configuration file */
			ConfFile = (uchar*) arg;
			break;
		case 'g':		/* enable tcp gssapi logging */
			fprintf(stderr,	"rsyslogd: -g option no longer supported - ignored\n");
		case 'h':
			fprintf(stderr, "rsyslogd: error -h is no longer supported - ignored");
			break;
		case 'i':		/* pid file name */
			PidFile = arg;
			break;
		case 'l':
			if(glbl.GetLocalHosts() != NULL) {
				fprintf (stderr, "rsyslogd: Only one -l argument allowed, the first one is taken.\n");
			} else {
				glbl.SetLocalHosts(crunch_list(arg));
			}
			break;
		case 'm':		/* mark interval */
			fprintf(stderr, "rsyslogd: error -m is no longer supported - use immark instead");
			break;
		case 'n':		/* don't fork */
			doFork = 0;
			break;
		case 'N':		/* enable config verify mode */
			iConfigVerify = atoi(arg);
			break;
                case 'o':
			fprintf(stderr, "error -o is no longer supported, use module imuxsock instead");
                        break;
                case 'p':
			fprintf(stderr, "error -p is no longer supported, use module imuxsock instead");
			break;
		case 'q':               /* add hostname if DNS resolving has failed */
		        *(net.pACLAddHostnameOnFail) = 1;
		        break;
		case 'Q':               /* dont resolve hostnames in ACL to IPs */
		        *(net.pACLDontResolve) = 1;
		        break;
		case 'r':		/* accept remote messages */
			fprintf(stderr, "rsyslogd: error option -r is no longer supported - ignored");
			break;
		case 's':
			if(glbl.GetStripDomains() != NULL) {
				fprintf (stderr, "rsyslogd: Only one -s argument allowed, the first one is taken.\n");
			} else {
				glbl.SetStripDomains(crunch_list(arg));
			}
			break;
		case 't':		/* enable tcp logging */
			fprintf(stderr, "rsyslogd: error option -t is no longer supported - ignored");
			break;
		case 'T':/* chroot() immediately at program startup, but only for testing, NOT security yet */
			if(chroot(arg) != 0) {
				perror("chroot");
				exit(1);
			}
			break;
		case 'u':		/* misc user settings */
			iHelperUOpt = atoi(arg);
			if(iHelperUOpt & 0x01)
				glbl.SetParseHOSTNAMEandTAG(0);
			if(iHelperUOpt & 0x02)
				bChDirRoot = 0;
			break;
		case 'w':		/* disable disallowed host warnigs */
			glbl.SetOption_DisallowWarning(0);
			break;
		case 'x':		/* disable dns for remote messages */
			glbl.SetDisableDNS(1);
			break;
               case '?':
		default:
			rsyslogd_usage();
		}
	}

	if(iRet != RS_RET_END_OF_LINKEDLIST)
		FINALIZE;

	if(iConfigVerify) {
		fprintf(stderr, "rsyslogd: version %s, config validation run (level %d), master config %s\n",
			VERSION, iConfigVerify, ConfFile);
	}

	localRet = rsconf.Load(&ourConf, ConfFile);
	/* oxpa <iippolitov@gmail.com> contribution, need to check ASL 2.0 */
	queryLocalHostname();	/* need to re-query to pick up a changed hostname due to config */
	/* end oxpa */

	if(localRet == RS_RET_NONFATAL_CONFIG_ERR) {
		if(loadConf->globals.bAbortOnUncleanConfig) {
			fprintf(stderr, "rsyslogd: $AbortOnUncleanConfig is set, and config is not clean.\n"
					"Check error log for details, fix errors and restart. As a last\n"
					"resort, you may want to remove $AbortOnUncleanConfig to permit a\n"
					"startup with a dirty config.\n");
			exit(2);
		}
		if(iConfigVerify) {
			/* a bit dirty, but useful... */
			exit(1);
		}
		localRet = RS_RET_OK;
	}
	CHKiRet(localRet);
	
	CHKiRet(rsyslogd_InitStdRatelimiters());

	if(bChDirRoot) {
		if(chdir("/") != 0)
			fprintf(stderr, "Can not do 'cd /' - still trying to run\n");
	}

	/* process compatibility mode settings */
	if(bEOptionWasGiven) {
		errmsg.LogError(0, NO_ERRCODE, "WARNING: \"message repeated n times\" feature MUST be turned on in "
					    "rsyslog.conf - CURRENTLY EVERY MESSAGE WILL BE LOGGED. Visit "
					    "http://www.rsyslog.com/rptdmsgreduction to learn "
					    "more and cast your vote if you want us to keep this feature.");
	}

	if(!iConfigVerify)
		CHKiRet(doGlblProcessInit());

	/* Send a signal to the parent so it can terminate.  */
	if(glblGetOurPid() != ppid)
		kill(ppid, SIGTERM);

	CHKiRet(init()); /* LICENSE NOTE: contribution from Michael Terry */

	if(Debug && debugging_on) {
		dbgprintf("Debugging enabled, SIGUSR1 to turn off debugging.\n");
	}

	/* END OF INTIALIZATION */
	DBGPRINTF("initialization completed, transitioning to regular run mode\n");

	/* close stderr and stdout if they are kept open during a fork. Note that this
	 * may introduce subtle security issues: if we are in a jail, one may break out of
	 * it via these descriptors. But if I close them earlier, error messages will (once
	 * again) not be emitted to the user that starts the daemon. As root jail support
	 * is still in its infancy (and not really done), we currently accept this issue.
	 * rgerhards, 2009-06-29
	 */
	if(doFork) {
		close(1);
		close(2);
		ourConf->globals.bErrMsgToStderr = 0;
	}

	sd_notify(0, "READY=1");


finalize_it:
	if(iRet == RS_RET_VALIDATION_RUN) {
		fprintf(stderr, "rsyslogd: End of config validation run. Bye.\n");
	} else if(iRet != RS_RET_OK) {
		fprintf(stderr, "rsyslogd: run failed with error %d (see rsyslog.h "
				"or try http://www.rsyslog.com/e/%d to learn what that number means)\n", iRet, iRet*-1);
		exit(1);
	}

	ENDfunc
}
