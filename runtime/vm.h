/* The vm object.
 *
 * This implements the rsyslog virtual machine. The initial implementation is
 * done to support complex user-defined expressions, but it may evolve into a
 * much more useful thing over time.
 *
 * The virtual machine uses rsyslog variables as its memory storage system.
 * All computation is done on a stack (vmstk). The vm supports a given
 * instruction set and executes programs of type vmprg, which consist of
 * single operations defined in vmop (which hold the instruction and the
 * data).
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
#ifndef INCLUDED_VM_H
#define INCLUDED_VM_H

#include "msg.h"
#include "vmstk.h"
#include "vmprg.h"

/* the vm object */
typedef struct vm_s {
	BEGINobjInstance;	/* Data to implement generic object - MUST be the first data element! */
	vmstk_t *pStk;		/* The stack */
	msg_t *pMsg;		/* the current message (or NULL, if we have none) */
} vm_t;


/* interfaces */
BEGINinterface(vm) /* name must also be changed in ENDinterface macro! */
	INTERFACEObjDebugPrint(vm);
	rsRetVal (*Construct)(vm_t **ppThis);
	rsRetVal (*ConstructFinalize)(vm_t __attribute__((unused)) *pThis);
	rsRetVal (*Destruct)(vm_t **ppThis);
	rsRetVal (*ExecProg)(vm_t *pThis, vmprg_t *pProg);
	rsRetVal (*PopBoolFromStack)(vm_t *pThis, var_t **ppVar); /* there are a few cases where we need this... */
	rsRetVal (*PopVarFromStack)(vm_t *pThis, var_t **ppVar); /* there are a few cases where we need this... */
	rsRetVal (*SetMsg)(vm_t *pThis, msg_t *pMsg); /* there are a few cases where we need this... */
ENDinterface(vm)
#define vmCURR_IF_VERSION 1 /* increment whenever you change the interface structure! */


/* prototypes */
PROTOTYPEObj(vm);

#endif /* #ifndef INCLUDED_VM_H */
