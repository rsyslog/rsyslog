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


/* ------------------------------ parser functions ------------------------------ */
/* the following functions implement the parser. They are all static. For
 * simplicity, the function names match their ABNF definition. The ABNF is defined
 * in the doc set. See file expression.html for details. I do *not* reproduce it
 * here in an effort to keep both files in sync.
 * 
 * All functions receive the current expression object as parameter as well as the
 * current tokenizer.
 *
 * rgerhards, 2008-02-19
 */

#if 0
static rsRetVal
template(expr_t *pThis, ctok_t *ctok)
{
	DEFiRet;
RUNLOG_STR("");

	ISOBJ_TYPE_assert(pThis, expr);
	ISOBJ_TYPE_assert(ctok, ctok);


finalize_it:
	RETiRet;
}
#endif



static rsRetVal
terminal(expr_t *pThis, ctok_t *ctok)
{
	DEFiRet;
	ctok_token_t token;
RUNLOG_STR("terminal");

	ISOBJ_TYPE_assert(pThis, expr);
	ISOBJ_TYPE_assert(ctok, ctok);

	CHKiRet(ctokGetNextToken(ctok, &token));

	switch(token.tok) {
		case ctok_SIMPSTR:
			break;
		default:
			ABORT_FINALIZE(RS_RET_SYNTAX_ERROR);
			break;
	}

finalize_it:
	RETiRet;
}

static rsRetVal
factor(expr_t *pThis, ctok_t *ctok)
{
	DEFiRet;
RUNLOG_STR("factor");

	ISOBJ_TYPE_assert(pThis, expr);
	ISOBJ_TYPE_assert(ctok, ctok);

	CHKiRet(terminal(pThis, ctok));

finalize_it:
	RETiRet;
}


static rsRetVal
term(expr_t *pThis, ctok_t *ctok)
{
	DEFiRet;
RUNLOG_STR("term");

	ISOBJ_TYPE_assert(pThis, expr);
	ISOBJ_TYPE_assert(ctok, ctok);

	CHKiRet(factor(pThis, ctok));

finalize_it:
	RETiRet;
}

static rsRetVal
val(expr_t *pThis, ctok_t *ctok)
{
	DEFiRet;
RUNLOG_STR("val");

	ISOBJ_TYPE_assert(pThis, expr);
	ISOBJ_TYPE_assert(ctok, ctok);

	CHKiRet(term(pThis, ctok));

finalize_it:
	RETiRet;
}


static rsRetVal
e_cmp(expr_t *pThis, ctok_t *ctok)
{
	DEFiRet;
RUNLOG_STR("e_cmp");

	ISOBJ_TYPE_assert(pThis, expr);
	ISOBJ_TYPE_assert(ctok, ctok);

	CHKiRet(val(pThis, ctok));

finalize_it:
	RETiRet;
}


static rsRetVal
e_and(expr_t *pThis, ctok_t *ctok)
{
	DEFiRet;
RUNLOG_STR("e_and");

	ISOBJ_TYPE_assert(pThis, expr);
	ISOBJ_TYPE_assert(ctok, ctok);

	CHKiRet(e_cmp(pThis, ctok));

finalize_it:
	RETiRet;
}


static rsRetVal
expr(expr_t *pThis, ctok_t *ctok)
{
	DEFiRet;
RUNLOG_STR("expr");

	ISOBJ_TYPE_assert(pThis, expr);
	ISOBJ_TYPE_assert(ctok, ctok);

	CHKiRet(e_and(pThis, ctok));

finalize_it:
	RETiRet;
}


/* ------------------------------ end parser functions ------------------------------ */


/* ------------------------------ actual expr object functions ------------------------------ */

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


/* destructor for the expr object */
BEGINobjDestruct(expr) /* be sure to specify the object type also in END and CODESTART macros! */
CODESTARTobjDestruct(expr)
	/* ... then free resources */
ENDobjDestruct(expr)


/* evaluate an expression and store the result. pMsg is optional, but if
 * it is not given, no message-based variables can be accessed. The expression
 * must previously have been created.
 * rgerhards, 2008-02-09 (a rainy tenerife return flight day ;))
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
 * rgerhards, 2008-02-09 (a rainy tenerife return flight day ;))
 */
rsRetVal
exprGetStr(expr_t *pThis, rsCStrObj **ppStr)
{
	DEFiRet;
	
	ISOBJ_TYPE_assert(pThis, expr);
	ASSERT(ppStr != NULL);

	RETiRet;
}


/* parse an expression object based on a given tokenizer
 * rgerhards, 2008-02-19
 */
rsRetVal
exprParse(expr_t *pThis, ctok_t *ctok)
{
	DEFiRet;

	ISOBJ_TYPE_assert(pThis, expr);
	ISOBJ_TYPE_assert(ctok, ctok);

	CHKiRet(expr(pThis, ctok));

RUNLOG_STR("expr parser being called");
finalize_it:
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
