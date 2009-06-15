/* The apc object.
 *
 * See apc.c for more information.
 *
 * Copyright 2009 Rainer Gerhards and Adiscon GmbH.
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
#ifndef INCLUDED_APC_H
#define INCLUDED_APC_H

/* the apc object */
typedef struct apc_s {
	BEGINobjInstance;		/* Data to implement generic object - MUST be the first data element! */
	time_t ttExec;			/* when to call procedure (so far seconds...) */
	void (*pProc)(void*, void*);	/* which procedure to call */
	void *param1;			/* user-supplied parameters */
	void *param2;			/* user-supplied parameters */
} apc_t;

typedef unsigned long apc_id_t;		/* monotonically incrementing apc ID */

/* interfaces */
BEGINinterface(apc) /* name must also be changed in ENDinterface macro! */
	INTERFACEObjDebugPrint(apc);
	rsRetVal (*Construct)(apc_t **ppThis);
	rsRetVal (*ConstructFinalize)(apc_t *pThis, apc_id_t *);
	rsRetVal (*Destruct)(apc_t **ppThis);
	rsRetVal (*SetProcedure)(apc_t *pThis, void (*pProc)(void*, void*));
	rsRetVal (*SetParam1)(apc_t *pThis, void *);
	rsRetVal (*SetParam2)(apc_t *pThis, void *);
	rsRetVal (*CancelApc)(apc_id_t);
ENDinterface(apc)
#define apcCURR_IF_VERSION 1 /* increment whenever you change the interface structure! */


/* prototypes */
PROTOTYPEObj(apc);

#endif /* #ifndef INCLUDED_APC_H */
