/* The expr object.
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
#ifndef INCLUDED_EXPR_H
#define INCLUDED_EXPR_H

#include "obj.h"
#include "ctok.h"
#include "vmprg.h"
#include "stringbuf.h"

/* a node inside an expression tree */
typedef struct exprNode_s {
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
