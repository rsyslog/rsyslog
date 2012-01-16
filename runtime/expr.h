/* The expr object.
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
#ifndef INCLUDED_EXPR_H
#define INCLUDED_EXPR_H

#include "obj.h"
#include "ctok.h"
#include "vmprg.h"
#include "stringbuf.h"

/* a node inside an expression tree */
typedef struct exprNode_s {
    	char dummy;
} exprNode_t;


/* the expression object */
typedef struct expr_s {
	BEGINobjInstance;	/* Data to implement generic object - MUST be the first data element! */
	vmprg_t *pVmprg;	/* the expression in vmprg format - ready to execute */
} expr_t;


/* interfaces */
BEGINinterface(expr) /* name must also be changed in ENDinterface macro! */
	INTERFACEObjDebugPrint(expr);
	rsRetVal (*Construct)(expr_t **ppThis);
	rsRetVal (*ConstructFinalize)(expr_t __attribute__((unused)) *pThis);
	rsRetVal (*Destruct)(expr_t **ppThis);
	rsRetVal (*Parse)(expr_t *pThis, ctok_t *ctok);
ENDinterface(expr)
#define exprCURR_IF_VERSION 1 /* increment whenever you change the interface structure! */

/* prototypes */
PROTOTYPEObj(expr);

#endif /* #ifndef INCLUDED_EXPR_H */
