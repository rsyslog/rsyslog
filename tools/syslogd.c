/**
 * main rsyslog file with GPLv3 content.
 *
 * *********************** NOTE ************************
 * * Do no longer patch this file. If there is hard    *
 * * need to, talk to Rainer as to how we can make any *
 * * patch be licensed under ASL 2.0.                  *
 * * THIS FILE WILL GO AWAY. The new main file is      *
 * * rsyslogd.c.                                       *
 * *****************************************************
 *
 * Please visit the rsyslog project at
 * http://www.rsyslog.com
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
#include "parser.h"
#include "unicode-helper.h"
#include "net.h"
#include "dnscache.h"
#include "sd-daemon.h"
#include "ratelimit.h"

/* definitions for objects we access */
DEFobjCurrIf(obj)
DEFobjCurrIf(glbl)
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
rsRetVal rsyslogdInit(void);
void rsyslogdDebugSwitch();
void rsyslogdDoDie(int sig);


#ifndef _PATH_LOGPID
#	define _PATH_LOGPID "/var/run/rsyslogd.pid"
#endif

#ifndef _PATH_TTY
#	define _PATH_TTY	"/dev/tty"
#endif
char	*PidFile = _PATH_LOGPID; /* read-only after startup */

int bHadHUP = 0; 	/* did we have a HUP? */
int bFinished = 0;	/* used by termination signal handler, read-only except there
			 * is either 0 or the number of the signal that requested the
 			 * termination.
			 */
int iConfigVerify = 0;	/* is this just a config verify run? */
pid_t ppid;	/* This is a quick and dirty hack used for spliting main/startup thread */
int	doFork = 1; 	/* fork - run in daemon mode - read-only after startup */


/* up to the next comment, prototypes that should be removed by reordering */
/* Function prototypes. */
static void reapchild();


#define LIST_DELIMITER	':'		/* delimiter between two hosts */
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
char **syslogd_crunch_list(char *list)
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

/* function stems back to sysklogd */
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


/*
 * The following function is resposible for handling a SIGHUP signal.  Since
 * we are now doing mallocs/free as part of init we had better not being
 * doing this during a signal handler.  Instead this function simply sets
 * a flag variable which will tells the main loop to do "the right thing".
 */
void syslogd_sighup_handler()
{
	struct sigaction sigAct;

	bHadHUP = 1;

	memset(&sigAct, 0, sizeof (sigAct));
	sigemptyset(&sigAct.sa_mask);
	sigAct.sa_handler = syslogd_sighup_handler;
	sigaction(SIGHUP, &sigAct, NULL);
}

/* obtain ptrs to all clases we need.  */
rsRetVal
syslogd_obtainClassPointers(void)
{
	DEFiRet;
	char *pErrObj; /* tells us which object failed if that happens (useful for troubleshooting!) */

	CHKiRet(objGetObjInterface(&obj)); /* this provides the root pointer for all other queries */

	/* Now tell the system which classes we need ourselfs */
	pErrObj = "glbl";
	CHKiRet(objUse(glbl,     CORE_COMPONENT));

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

/* global initialization, to be done only once and before the mainloop is started.
 * rgerhards, 2008-07-28 (extracted from realMain())
 */
rsRetVal
syslogd_doGlblProcessInit(void)
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
		fprintf(stderr, "rsyslogd: pidfile '%s' and pid %d already exist.\n",
			PidFile, getpid());
		exit(1); /* exit during startup - questionable */
	}
	glblSetOurPid(getpid());

	memset(&sigAct, 0, sizeof (sigAct));
	sigemptyset(&sigAct.sa_mask);

	sigAct.sa_handler = sigsegvHdlr;
	sigaction(SIGSEGV, &sigAct, NULL);
	sigAct.sa_handler = sigsegvHdlr;
	sigaction(SIGABRT, &sigAct, NULL);
	sigAct.sa_handler = rsyslogdDoDie;
	sigaction(SIGTERM, &sigAct, NULL);
	sigAct.sa_handler = Debug ? rsyslogdDoDie : SIG_IGN;
	sigaction(SIGINT, &sigAct, NULL);
	sigaction(SIGQUIT, &sigAct, NULL);
	sigAct.sa_handler = reapchild;
	sigaction(SIGCHLD, &sigAct, NULL);
	sigAct.sa_handler = Debug ? rsyslogdDebugSwitch : SIG_IGN;
	sigaction(SIGUSR1, &sigAct, NULL);
	sigAct.sa_handler = rsyslogd_sigttin_handler;
	sigaction(SIGTTIN, &sigAct, NULL); /* (ab)used to interrupt input threads */
	sigAct.sa_handler = SIG_IGN;
	sigaction(SIGPIPE, &sigAct, NULL);
	sigaction(SIGXFSZ, &sigAct, NULL); /* do not abort if 2gig file limit is hit */

	RETiRet;
}


void
syslogdInit(void)
{
	/* oxpa <iippolitov@gmail.com> contribution, need to check ASL 2.0 */
	queryLocalHostname();	/* need to re-query to pick up a changed hostname due to config */
	/* end oxpa */
}
