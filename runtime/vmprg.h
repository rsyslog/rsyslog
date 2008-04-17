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
#ifndef INCLUDED_VMPRG_H
#define INCLUDED_VMPRG_H

#include "vmop.h"


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
ENDinterface(vmprg)
#define vmprgCURR_IF_VERSION 1 /* increment whenever you change the interface structure! */


/* prototypes */
PROTOTYPEObj(vmprg);

#endif /* #ifndef INCLUDED_VMPRG_H */
