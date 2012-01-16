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
	/* v2 (4.1.7) */
	rsRetVal (*FindRSFunction)(cstr_t *pcsName, prsf_t *prsf); /* 2009-06-04 */
	rsRetVal (*FindRSFunctionName)(prsf_t rsf, cstr_t **ppcsName); /* 2009-06-04 */
ENDinterface(vm)
#define vmCURR_IF_VERSION 2 /* increment whenever you change the interface structure! */


/* prototypes */
PROTOTYPEObj(vm);

#endif /* #ifndef INCLUDED_VM_H */
