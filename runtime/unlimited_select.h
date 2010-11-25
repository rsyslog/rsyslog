/* unlimited_select.h
 * Tweak the macros for accessing fd_set so that the select() syscall
 * won't be limited to a particular number of file descriptors.
 *
 * Copyright 2009 Rainer Gerhards and Adiscon GmbH.
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

#ifndef	UNLIMITED_SELECT_H_INCLUDED
#define UNLIMITED_SELECT_H_INCLUDED

#include <string.h>
#include <stdlib.h>
#include <sys/select.h>
#include "glbl.h"

#ifdef USE_UNLIMITED_SELECT
# undef FD_ZERO
# define FD_ZERO(set) memset((set), 0, glbl.GetFdSetSize());
#endif

#ifdef USE_UNLIMITED_SELECT
void freeFdSet(fd_set *p) {
        free(p);
}
#else
#	define freeFdSet(x)
#endif

#endif /* #ifndef UNLIMITED_SELECT_H_INCLUDED */
