/* common header for syslogd
 * Copyright 2007 Rainer Gerhards and Adiscon GmbH.
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
#ifndef	SYSLOGD_H_INCLUDED
#define	SYSLOGD_H_INCLUDED 1

#include "syslogd-types.h"
#include "objomsr.h"
#include "modules.h"
#include "template.h"
#include "action.h"
#include "linkedlist.h"
#include "expr.h"


#ifndef _PATH_CONSOLE
#define _PATH_CONSOLE	"/dev/console"
#endif


/* This structure represents the files that will have log
 * copies printed.
 * RGerhards 2004-11-08: Each instance of the filed structure 
 * describes what I call an "output channel". This is important
 * to mention as we now allow database connections to be
 * present in the filed structure. If helps immensely, if we
 * think of it as the abstraction of an output channel.
 * rgerhards, 2005-10-26: The structure below provides ample
 * opportunity for non-thread-safety. Each of the variable
 * accesses must be carefully evaluated, many of them probably
 * be guarded by mutexes. But beware of deadlocks...
 * rgerhards, 2007-08-01: as you can see, the structure has shrunk pretty much. I will
 * remove some of the comments some time. It's still the structure that controls much
 * of the processing that goes on in syslogd, but it now has lots of helpers.
 */
struct filed {
	struct	filed *f_next;		/* next in linked list */
	/* filter properties */
	enum {
		FILTER_PRI = 0,		/* traditional PRI based filer */
		FILTER_PROP = 1,	/* extended filter, property based */
		FILTER_EXPR = 2		/* extended filter, expression based */
	} f_filter_type;
	EHostnameCmpMode eHostnameCmpMode;
	cstr_t *pCSHostnameComp;	/* hostname to check */
	cstr_t *pCSProgNameComp;	/* tag to check or NULL, if not to be checked */
	union {
		u_char	f_pmask[LOG_NFACILITIES+1];	/* priority mask */
		struct {
			cstr_t *pCSPropName;
			enum {
				FIOP_NOP = 0,		/* do not use - No Operation */
				FIOP_CONTAINS  = 1,	/* contains string? */
				FIOP_ISEQUAL  = 2,	/* is (exactly) equal? */
				FIOP_STARTSWITH = 3,	/* starts with a string? */
 				FIOP_REGEX = 4,		/* matches a (BRE) regular expression? */
 				FIOP_EREREGEX = 5	/* matches a ERE regular expression? */
			} operation;
			regex_t *regex_cache;		/* cache for compiled REs, if such are used */
			cstr_t *pCSCompValue;	/* value to "compare" against */
			char isNegated;			/* actually a boolean ;) */
		} prop;
		expr_t *f_expr;				/* expression object */
	} f_filterData;

	linkedList_t llActList;	/* list of configured actions */
};


#include "net.h" /* TODO: remove when you remoe isAllowedSender from here! */
void untty(void);
rsRetVal selectorConstruct(selector_t **ppThis);
rsRetVal selectorDestruct(void *pVal);
rsRetVal selectorAddList(selector_t *f);
/* the following prototypes should go away once we have an input
 * module interface -- rgerhards, 2007-12-12
 */
void logmsg(msg_t *pMsg, int flags);
extern int NoHops;
extern int send_to_all;
extern int Debug;
#include "dirty.h"

#endif /* #ifndef SYSLOGD_H_INCLUDED */
