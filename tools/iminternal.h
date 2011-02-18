/* Definition of the internal messages input module.
 *
 * Note: we currently do not have an input module spec, but
 * we will have one in the future. This module needs then to be
 * adapted.
 *
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

#ifndef IMINTERNAL_H_INCLUDED
#define IMINTERNAL_H_INCLUDED
#include "template.h"

/* this is a single entry for a parse routine. It describes exactly
 * one entry point/handler.
 * The short name is cslch (Configfile SysLine CommandHandler)
 */
struct iminternal_s { /* config file sysline parse entry */
	msg_t *pMsg;	/* the message (in all its glory) */
};
typedef struct iminternal_s iminternal_t;

/* prototypes */
rsRetVal modInitIminternal(void);
rsRetVal modExitIminternal(void);
rsRetVal iminternalAddMsg(msg_t *pMsg);
rsRetVal iminternalHaveMsgReady(int* pbHaveOne);
rsRetVal iminternalRemoveMsg(msg_t **ppMsg);

#endif /* #ifndef IMINTERNAL_H_INCLUDED */
