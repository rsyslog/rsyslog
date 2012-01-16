/* The errmsg object. It is used to emit error message inside rsyslog.
 *
 * Copyright 2008-2012 Rainer Gerhards and Adiscon GmbH.
 *
 * This file is part of the rsyslog runtime library.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *       http://www.apache.org/licenses/LICENSE-2.0
 *       -or-
 *       see COPYING.ASL20 in the source distribution
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
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
