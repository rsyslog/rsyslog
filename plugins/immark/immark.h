/* immark.h
 * These are the definitions for the built-in mark message generation module. This
 * file may disappear when this has become a loadable module.
 *
 * File begun on 2007-12-12 by RGerhards (extracted from syslogd.c)
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
#ifndef	IMMARK_H_INCLUDED
#define	IMMARK_H_INCLUDED 1

/* prototypes */
rsRetVal immark_runInput(void);

#endif /* #ifndef IMMARK_H_INCLUDED */
/*
 * vi:set ai:
 */
