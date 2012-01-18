/* The vmprg object.
 *
 * The program is made up of vmop_t's, one after another. When we support
 * branching (or user-defined functions) at some time, well do this via
 * special branch opcodes. They will then contain the actual memory
 * address of a logical program entry that we shall branch to. Other than
 * that, all execution is serial - that is one opcode is executed after
 * the other. This class implements a logical program store, modelled
 * after real main memory. A linked list of opcodes is used to implement it.
 * In the future, we may use linked lists of array's to enhance performance,
 * but for the time being we have taken the simplistic approach (which also
 * reduces risk of bugs during initial development). The necessary pointers
 * for this are already implemented in vmop. Though this is not the 100%
 * correct place, we have opted this time in favor of performance, which
 * made them go there.
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
#ifndef INCLUDED_VMPRG_H
#define INCLUDED_VMPRG_H

#include "vmop.h"
#include "stringbuf.h"

/* the vmprg object */
typedef struct vmprg_s {
	BEGINobjInstance;	/* Data to implement generic object - MUST be the first data element! */
	vmop_t *vmopRoot;	/* start of program */
	vmop_t *vmopLast;	/* last vmop of program (for adding new ones) */
} vmprg_t;


/* interfaces */
BEGINinterface(vmprg) /* name must also be changed in ENDinterface macro! */
	INTERFACEObjDebugPrint(vmprg);
	rsRetVal (*Construct)(vmprg_t **ppThis);
	rsRetVal (*ConstructFinalize)(vmprg_t __attribute__((unused)) *pThis);
	rsRetVal (*Destruct)(vmprg_t **ppThis);
	rsRetVal (*AddOperation)(vmprg_t *pThis, vmop_t *pOp);
	rsRetVal (*AddVarOperation)(vmprg_t *pThis, opcode_t opcode, var_t *pVar);
	rsRetVal (*Obj2Str)(vmprg_t *pThis, cstr_t *pstr);
	/* v2 (4.1.7) */
	rsRetVal (*AddCallOperation)(vmprg_t *pThis, cstr_t *pVar); /* added 2009-04-06 */
ENDinterface(vmprg)
#define vmprgCURR_IF_VERSION 2 /* increment whenever you change the interface structure! */


/* prototypes */
PROTOTYPEObj(vmprg);

#endif /* #ifndef INCLUDED_VMPRG_H */
