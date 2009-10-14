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

/* the following prototypes should go away once we have an input
 * module interface -- rgerhards, 2007-12-12
 */
extern int NoHops;
extern int send_to_all;
extern int Debug;
#include "dirty.h"

#endif /* #ifndef SYSLOGD_H_INCLUDED */
