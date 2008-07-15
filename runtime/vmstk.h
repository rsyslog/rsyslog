/* The vmstk object.
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
#ifndef INCLUDED_VMSTK_H
#define INCLUDED_VMSTK_H

/* The max size of the stack - TODO: make configurable */
#define VMSTK_SIZE 256

/* the vmstk object */
typedef struct vmstk_s {
	BEGINobjInstance;	/* Data to implement generic object - MUST be the first data element! */
	var_t *vStk[VMSTK_SIZE];/* the actual stack */
	int iStkPtr;		/* stack pointer, points to next free location, grows from 0 --> topend */
} vmstk_t;


/* interfaces */
BEGINinterface(vmstk) /* name must also be changed in ENDinterface macro! */
	INTERFACEObjDebugPrint(vmstk);
	rsRetVal (*Construct)(vmstk_t **ppThis);
	rsRetVal (*ConstructFinalize)(vmstk_t __attribute__((unused)) *pThis);
	rsRetVal (*Destruct)(vmstk_t **ppThis);
	rsRetVal (*Push)(vmstk_t *pThis, var_t *pVar);
	rsRetVal (*Pop)(vmstk_t *pThis, var_t **ppVar);
	rsRetVal (*PopBool)(vmstk_t *pThis, var_t **ppVar);
	rsRetVal (*PopNumber)(vmstk_t *pThis, var_t **ppVar);
	rsRetVal (*PopString)(vmstk_t *pThis, var_t **ppVar);
	rsRetVal (*Pop2CommOp)(vmstk_t *pThis, var_t **ppVar1, var_t **ppVar2);
ENDinterface(vmstk)
#define vmstkCURR_IF_VERSION 1 /* increment whenever you change the interface structure! */


/* prototypes */
PROTOTYPEObj(vmstk);

#endif /* #ifndef INCLUDED_VMSTK_H */
