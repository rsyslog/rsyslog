/* Definition of globally-accessible data items.
 *
 * This module provides access methods to items of global scope. Most often,
 * these globals serve as defaults to initialize local settings. Currently,
 * many of them are either constants or global variable references. However,
 * this module provides the necessary hooks to change that at any time.
 *
 * Please note that there currently is no glbl.c file as we do not yet
 * have any implementations.
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

#ifndef GLBL_H_INCLUDED
#define GLBL_H_INCLUDED

#define glblGetIOBufSize() 4096 /* size of the IO buffer, e.g. for strm class */

extern uchar *glblModPath; /* module load path */

/* the glbl object 
 * Note: this must be defined to satisfy the interface. We do not 
 * actually have instance data.*/
typedef struct glbl_s {
	BEGINobjInstance;	/* Data to implement generic object - MUST be the first data element! */
} glbl_t;


/* interfaces */
BEGINinterface(glbl) /* name must also be changed in ENDinterface macro! */
	uchar* (*GetWorkDir)(void);
ENDinterface(glbl)
#define glblCURR_IF_VERSION 1 /* increment whenever you change the interface structure! */

/* the remaining prototypes */
PROTOTYPEObj(glbl);

#endif /* #ifndef GLBL_H_INCLUDED */
