/* The vmstk object.
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
#ifndef INCLUDED_VMSTK_H
#define INCLUDED_VMSTK_H

/* The max size of the stack - TODO: make configurable */
#define VMSTK_SIZE 256

/* the vmstk object */
struct vmstk_s {
	BEGINobjInstance;	/* Data to implement generic object - MUST be the first data element! */
	var_t *vStk[VMSTK_SIZE];/* the actual stack */
	int iStkPtr;		/* stack pointer, points to next free location, grows from 0 --> topend */
};


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
