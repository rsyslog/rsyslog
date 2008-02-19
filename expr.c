/* expr.c - an expression class.
 * This module contains all code needed to represent expressions. Most
 * importantly, that means code to parse and execute them. Expressions
 * heavily depend on (loadable) functions, so it works in conjunction
 * with the function manager.
 *
 * Module begun 2007-11-30 by Rainer Gerhards
 *
 * Copyright 2007, 2008 Rainer Gerhards and Adiscon GmbH.
 *
 * This file is part of rsyslog.
 *
 * Rsyslog is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Rsyslog is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Rsyslog.  If not, see <http://www.gnu.org/licenses/>.
 *
 * A copy of the GPL can be found in the file "COPYING" in this distribution.
 */

#include "config.h"
#include <stdlib.h>
#include <assert.h>

#include "rsyslog.h"
#include "template.h"
#include "expr.h"

/* static data */
DEFobjStaticHelpers


/* Standard-Constructor
 */
BEGINobjConstruct(expr) /* be sure to specify the object type also in END macro! */
ENDobjConstruct(expr)


/* ConstructionFinalizer
 * rgerhards, 2008-01-09
 */
rsRetVal exprConstructFinalize(expr_t *pThis)
{
	DEFiRet;

	ISOBJ_TYPE_assert(pThis, expr);

finalize_it:
	RETiRet;
}


/* destructor for the strm object */
BEGINobjDestruct(expr) /* be sure to specify the object type also in END and CODESTART macros! */
CODESTARTobjDestruct(expr)
	/* ... then free resources */
ENDobjDestruct(expr)


/* evaluate an expression and store the result. pMsg is optional, but if
 * it is not given, no message-based variables can be accessed. The expression
 * must previously have been created.
 * rgerhards, 2008-02-09
 */
rsRetVal
exprEval(expr_t *pThis, msg_t *pMsg)
{
	DEFiRet;
	
	ISOBJ_TYPE_assert(pThis, expr);

	RETiRet;
}


/* returns the expression result as a string. The string is read-only and valid
 * only as long as the expression exists and is not newly evaluated. If the caller
 * needs to access it for an extended period of time, it must copy it. This is a
 * performance optimization, as most expression results are expected to be used
 * only for a brief period. In such cases, it saves us the need to copy the string.
 * Also, it is assumed that most callers will hold their private expression. If it 
 * is not shared, the caller can count on the string to remain stable as long as it
 * does not reevaluate the expression (via exprEval or other means) or destruct it.
 * rgerhards, 2008-02-09
 */
rsRetVal
exprGetStr(expr_t *pThis, rsCStrObj **ppStr)
{
	DEFiRet;
	
	ISOBJ_TYPE_assert(pThis, expr);
	ASSERT(ppStr != NULL);

	RETiRet;
}


/* parse an expression from a string. The string MUST include the full expression. The
 * expression object is only created if there is no error during parsing.
 * The string is a standard C string. It must NOT contain anything else but the
 * expression. Most importantly, it must not contain any comments after the
 * expression.
 * rgerhards, 2008-02-09 (a rainy tenerife return flight day ;))
 */
rsRetVal
exprParseStr(expr_t **ppThis, uchar *p)
{
	DEFiRet;
	expr_t *pThis = NULL;
	
	ASSERT(ppThis != NULL);
	ASSERT(p != NULL);

	CHKiRet(exprConstruct(&pThis));

	CHKiRet(rsCStrConstruct(&pThis->cstrConst));

	/* so far, we are a dummy - we just pull the first string and
	 * ignore the rest...
	 */
	while(*p && *p != '"')
		++p; /* find begin of string */
	if(*p == '"')
		++p;	/* eat it */

	/* we got it, now copy over everything up until the end of the string */
	while(*p && *p != '"') {
		CHKiRet(rsCStrAppendChar(pThis->cstrConst, *p));
		++p;
	}

	/* we are done with it... */
	CHKiRet(rsCStrFinish(pThis->cstrConst));

	CHKiRet(exprConstructFinalize(&pThis));

	/* we are successfully done, so store the result */
	*ppThis = pThis;

finalize_it:
	if(iRet != RS_RET_OK) {
		if(pThis != NULL)
			exprDestruct(pThis);
	}

	RETiRet;
}


/* Initialize the expr class. Must be called as the very first method
 * before anything else is called inside this class.
 * rgerhards, 2008-02-19
 */
BEGINObjClassInit(expr, 1) /* class, version */
	OBJSetMethodHandler(objMethod_CONSTRUCTION_FINALIZER, exprConstructFinalize);
ENDObjClassInit(expr)

/* vi:set ai:
 */
