/* debug.c
 *
 * This file proides debug and run time error analysis support. Some of the
 * settings are very performance intense and my be turned off during a release
 * build.
 * 
 * File begun on 2008-01-22 by RGerhards
 *
 * There is some in-depth documentation available in doc/dev_queue.html
 * (and in the web doc set on http://www.rsyslog.com/doc). Be sure to read it
 * if you are getting aquainted to the object.
 *
 * Copyright 2008 Rainer Gerhards and Adiscon GmbH.
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


#include "config.h" /* autotools! */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <pthread.h>

#include "rsyslog.h"
#include "debug.h"

/* static data (some time to be replaced) */
int	Debug;		/* debug flag  - read-only after startup */
int debugging_on = 0;	 /* read-only, except on sig USR1 */

/* handler for SIGSEGV - MUST terminiate the app, but does so in a somewhat 
 * more meaningful way.
 * rgerhards, 2008-01-22
 */
void
sigsegvHdlr(int signum)
{
	struct sigaction sigAct;
	char *signame;

	if(signum == SIGSEGV) {
		signame = " (SIGSEGV)";
	} else {
		signame = "";
	}

	fprintf(stderr, "Signal %d%s occured, execution must be terminated %d.\n", signum, signame, SIGSEGV);
	fflush(stderr);

	/* re-instantiate original handler ... */
	memset(&sigAct, 0, sizeof (sigAct));
	sigemptyset(&sigAct.sa_mask);
	sigAct.sa_handler = SIG_DFL;
	sigaction(SIGSEGV, &sigAct, NULL);

	/* and call it */
	int i= raise(signum);
	printf("raise returns %d, errno %d: %s\n", i, errno, strerror(errno));

	/* we should never arrive here - but we provide some code just in case... */
	fprintf(stderr, "sigsegvHdlr: oops, returned from raise(), doing exit(), something really wrong...\n");
	exit(1);
}


/* print some debug output */
void
dbgprintf(char *fmt, ...)
{
	static pthread_t ptLastThrdID = 0;
	static int bWasNL = 0;
	va_list ap;

	if ( !(Debug && debugging_on) )
		return;
	
	/* The bWasNL handler does not really work. It works if no thread
	 * switching occurs during non-NL messages. Else, things are messed
	 * up. Anyhow, it works well enough to provide useful help during
	 * getting this up and running. It is questionable if the extra effort
	 * is worth fixing it, giving the limited appliability.
	 * rgerhards, 2005-10-25
	 * I have decided that it is not worth fixing it - especially as it works
	 * pretty well.
	 * rgerhards, 2007-06-15
	 */
	if(ptLastThrdID != pthread_self()) {
		if(!bWasNL) {
			fprintf(stdout, "\n");
			bWasNL = 1;
		}
		ptLastThrdID = pthread_self();
	}

	if(bWasNL) {
		fprintf(stdout, "%8.8x: ", (unsigned int) pthread_self());
		//fprintf(stderr, "%8.8x: ", (unsigned int) pthread_self());
	}
	bWasNL = (*(fmt + strlen(fmt) - 1) == '\n') ? 1 : 0;
	va_start(ap, fmt);
	vfprintf(stdout, fmt, ap);
	//vfprintf(stderr, fmt, ap);
	va_end(ap);

	//fflush(stderr);
	fflush(stdout);
	return;
}

/* */

/*
 * vi:set ai:
 */
