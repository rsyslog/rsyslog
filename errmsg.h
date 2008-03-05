/* The errmsg object. It is used to emit error message inside rsyslog.
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
#ifndef INCLUDED_ERRMSG_H
#define INCLUDED_ERRMSG_H

#include "errmsg.h"

/* TODO: define error codes */
#define NO_ERRCODE -1

/* the errmsg object */
typedef struct errmsg_s {
} errmsg_t;


/* interfaces */
BEGINinterface(errmsg) /* name must also be changed in ENDinterface macro! */
	void  __attribute__((format(printf, 2, 3))) (*LogError)(int iErrCode, char *pszErrFmt, ... );
ENDinterface(errmsg)
#define errmsgCURR_IF_VERSION 1 /* increment whenever you change the interface structure! */


/* prototypes */
PROTOTYPEObj(errmsg);

#endif /* #ifndef INCLUDED_ERRMSG_H */
