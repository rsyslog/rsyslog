/* The errmsg object. It is used to emit error message inside rsyslog.
 *
 * Copyright 2008 Rainer Gerhards and Adiscon GmbH.
 *
 * This file is part of the rsyslog runtime library.
 *
 * The rsyslog runtime library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * The rsyslog runtime library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with the rsyslog runtime library.  If not, see <http://www.gnu.org/licenses/>.
 *
 * A copy of the GPL can be found in the file "COPYING" in this distribution.
 * A copy of the LGPL can be found in the file "COPYING.LESSER" in this distribution.
 */
#ifndef INCLUDED_ERRMSG_H
#define INCLUDED_ERRMSG_H

#include "errmsg.h"

/* TODO: define error codes */
#define NO_ERRCODE -1

/* the errmsg object */
typedef struct errmsg_s {
    	char dummy;
} errmsg_t;


/* interfaces */
BEGINinterface(errmsg) /* name must also be changed in ENDinterface macro! */
	void  __attribute__((format(printf, 3, 4))) (*LogError)(int iErrno, int iErrCode, char *pszErrFmt, ... );
ENDinterface(errmsg)
#define errmsgCURR_IF_VERSION 1 /* increment whenever you change the interface structure! */


/* prototypes */
PROTOTYPEObj(errmsg);

#endif /* #ifndef INCLUDED_ERRMSG_H */
