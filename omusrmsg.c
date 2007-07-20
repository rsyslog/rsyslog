/* omusrmsg.c
 * This is the implementation of the build-in output module for sending
 * user messages.
 *
 * File begun on 2007-07-20 by RGerhards (extracted from syslogd.c)
 * This file is under development and has not yet arrived at being fully
 * self-contained and a real object. So far, it is mostly an excerpt
 * of the "old" message code without any modifications. However, it
 * helps to have things at the right place one we go to the meat of it.
 *
 * Copyright 2007 Rainer Gerhards and Adiscon GmbH.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * A copy of the GPL can be found in the file "COPYING" in this distribution.
 */
#include "config.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <signal.h>
#include <sys/param.h>
#include <utmp.h>
#include <unistd.h>
#include <sys/uio.h>
#include <sys/stat.h>
#include <setjmp.h>
#if HAVE_FCNTL_H
#include <fcntl.h>
#else
#include <sys/msgbuf.h>
#endif
#include "rsyslog.h"
#include "syslogd.h"
#include "syslogd-types.h"
#include "srUtils.h"
#include "omusrmsg.h"


jmp_buf ttybuf;

static void endtty()
{
	longjmp(ttybuf, 1);
}

/**
 * BSD setutent/getutent() replacement routines
 * The following routines emulate setutent() and getutent() under
 * BSD because they are not available there. We only emulate what we actually
 * need! rgerhards 2005-03-18
 */
#ifdef BSD
static FILE *BSD_uf = NULL;
void setutent(void)
{
	assert(BSD_uf == NULL);
	if ((BSD_uf = fopen(_PATH_UTMP, "r")) == NULL) {
		logerror(_PATH_UTMP);
		return;
	}
}

struct utmp* getutent(void)
{
	static struct utmp st_utmp;

	if(fread((char *)&st_utmp, sizeof(st_utmp), 1, BSD_uf) != 1)
		return NULL;

	return(&st_utmp);
}

void endutent(void)
{
	fclose(BSD_uf);
	BSD_uf = NULL;
}
#endif


/*
 *  WALLMSG -- Write a message to the world at large
 *
 *	Write the specified message to either the entire
 *	world, or a list of approved users.
 *
 * rgerhards, 2005-10-19: applying the following sysklogd patch:
 * Tue May  4 16:52:01 CEST 2004: Solar Designer <solar@openwall.com>
 *	Adjust the size of a variable to prevent a buffer overflow
 *	should _PATH_DEV ever contain something different than "/dev/".
 */
static void wallmsg(register selector_t *f)
{
  
	char p[sizeof(_PATH_DEV) + UNAMESZ];
	register int i;
	int ttyf;
	static int reenter = 0;
	struct utmp ut;
	struct utmp *uptr;

	assert(f != NULL);

	if (reenter++)
		return;

	iovCreate(f);	/* init the iovec */

	/* open the user login file */
	setutent();

	/*
	 * Might as well fork instead of using nonblocking I/O
	 * and doing notty().
	 */
	if (fork() == 0) {
		signal(SIGTERM, SIG_DFL);
		alarm(0);
#		ifdef		SIGTTOU
		signal(SIGTTOU, SIG_IGN);
#		endif
		sigsetmask(0);
	/* TODO: find a way to limit the max size of the message. hint: this
	 * should go into the template!
	 */
	
	/* rgerhards 2005-10-24: HINT: this code might be run in a seperate thread
	 * instead of a seperate process once we have multithreading...
	 */

		/* scan the user login file */
		while ((uptr = getutent())) {
			memcpy(&ut, uptr, sizeof(ut));
			/* is this slot used? */
			if (ut.ut_name[0] == '\0')
				continue;
#ifndef BSD
			if (ut.ut_type != USER_PROCESS)
			        continue;
#endif
			if (!(strncmp (ut.ut_name,"LOGIN", 6))) /* paranoia */
			        continue;

			/* should we send the message to this user? */
			if (f->f_type == F_USERS) {
				for (i = 0; i < MAXUNAMES; i++) {
					if (!f->f_un.f_uname[i][0]) {
						i = MAXUNAMES;
						break;
					}
					if (strncmp(f->f_un.f_uname[i],
					    ut.ut_name, UNAMESZ) == 0)
						break;
				}
				if (i >= MAXUNAMES)
					continue;
			}

			/* compute the device name */
			strcpy(p, _PATH_DEV);
			strncat(p, ut.ut_line, UNAMESZ);

			if (setjmp(ttybuf) == 0) {
				(void) signal(SIGALRM, endtty);
				(void) alarm(15);
				/* open the terminal */
				ttyf = open(p, O_WRONLY|O_NOCTTY);
				if (ttyf >= 0) {
					struct stat statb;

					if (fstat(ttyf, &statb) == 0 &&
					    (statb.st_mode & S_IWRITE))
						(void) writev(ttyf, f->f_iov, f->f_iIovUsed);
					close(ttyf);
					ttyf = -1;
				}
			}
			(void) alarm(0);
		}
		exit(0); /* "good" exit - this terminates the child forked just for message delivery */
	}
	/* close the user login file */
	endutent();
	reenter = 0;
}


/* call the shell action
 * returns 0 if it succeeds, something else otherwise
 */
int doActionUsrMsg(selector_t *f, time_t now)
{
	assert(f != NULL);

	f->f_time = now;
	dprintf("\n");
	wallmsg(f);
	return 0;
}

/*
 * vi:set ai:
 */
