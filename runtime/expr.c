/* expr.c - an expression class.
 * This module contains all code needed to represent expressions. Most
 * importantly, that means code to parse and execute them. Expressions
 * heavily depend on (loadable) functions, so it works in conjunction
 * with the function manager.
 *
 * Module begun 2007-11-30 by Rainer Gerhards
 *
 * Copyright 2007-2012 Adiscon GmbH.
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

#include "config.h"
#include <stdlib.h>
#include <assert.h>

#include "rsyslog.h"
#include "template.h"
#include "expr.h"

/* static data */
DEFobjStaticHelpers
DEFobjCurrIf(vmprg)
DEFobjCurrIf(var)
DEFobjCurrIf(ctok_token)
DEFobjCurrIf(ctok)


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

/* forward definition - thanks to recursive ABNF, we can not avoid at least one ;) */
static rsRetVal expr(expr_t *pThis, ctok_t *tok);


static rsRetVal
function(expr_t *pThis, ctok_t *tok)
{
	DEFiRet;
	ctok_token_t *pToken = NULL;
	int iNumArgs = 0;
	var_t *pVar;

	ISOBJ_TYPE_assert(pThis, expr);
	ISOBJ_TYPE_assert(tok, ctok);

	CHKiRet(ctok.GetToken(tok, &pToken));
	/* note: pToken is destructed in finalize_it */

	if(pToken->tok == ctok_LPAREN) {
		CHKiRet(ctok_token.Destruct(&pToken)); /* token processed, "eat" it */
		CHKiRet(ctok.GetToken(tok, &pToken)); /* get next one */
	} else
		ABORT_FINALIZE(RS_RET_FUNC_NO_LPAREN);

	/* we first push all the params on the stack. Then we call the function */
	while(pToken->tok != ctok_RPAREN) { 
		++iNumArgs;
		CHKiRet(ctok.UngetToken(tok, pToken)); /* not for us, so let others process it */
		CHKiRet(expr(pThis, tok));
		CHKiRet(ctok.GetToken(tok, &pToken)); /* get next one, needed for while() check */
		if(pToken->tok == ctok_COMMA) {
			CHKiRet(ctok_token.Destruct(&pToken)); /* token processed, "eat" it */
			CHKiRet(ctok.GetToken(tok, &pToken)); /* get next one */
			if(pToken->tok == ctok_RPAREN) {
				ABORT_FINALIZE(RS_RET_FUNC_MISSING_EXPR);
			}
		}
	}


	/* now push number of arguments - this must be on top of the stack */
	CHKiRet(var.Construct(&pVar));
	CHKiRet(var.ConstructFinalize(pVar));
	CHKiRet(var.SetNumber(pVar, iNumArgs));
	CHKiRet(vmprg.AddVarOperation(pThis->pVmprg, opcode_PUSHCONSTANT, pVar)); /* add to program */


finalize_it:
	if(pToken != NULL) {
		ctok_token.Destruct(&pToken); /* "eat" processed token */
	}

	RETiRet;
}


static rsRetVal
terminal(expr_t *pThis, ctok_t *tok)
{
	DEFiRet;
	ctok_token_t *pToken = NULL;
	var_t *pVar;

	ISOBJ_TYPE_assert(pThis, expr);
	ISOBJ_TYPE_assert(tok, ctok);

	CHKiRet(ctok.GetToken(tok, &pToken));
	/* note: pToken is destructed in finalize_it */

	switch(pToken->tok) {
		case ctok_SIMPSTR:
			dbgoprint((obj_t*) pThis, "simpstr\n");
			CHKiRet(ctok_token.UnlinkVar(pToken, &pVar));
			CHKiRet(vmprg.AddVarOperation(pThis->pVmprg, opcode_PUSHCONSTANT, pVar)); /* add to program */
			break;
		case ctok_NUMBER:
			dbgoprint((obj_t*) pThis, "number\n");
			CHKiRet(ctok_token.UnlinkVar(pToken, &pVar));
			CHKiRet(vmprg.AddVarOperation(pThis->pVmprg, opcode_PUSHCONSTANT, pVar)); /* add to program */
			break;
		case ctok_FUNCTION:
			dbgoprint((obj_t*) pThis, "function\n");
			CHKiRet(function(pThis, tok)); /* this creates the stack call frame */
			/* ... but we place the call instruction onto the stack ourselfs (because
			 * we have all relevant information)
			 */
			CHKiRet(ctok_token.UnlinkVar(pToken, &pVar));
			CHKiRet(var.ConvToString(pVar)); /* make sure we have a string */
			CHKiRet(vmprg.AddCallOperation(pThis->pVmprg, pVar->val.pStr)); /* add to program */
			CHKiRet(var.Destruct(&pVar));
			break;
		case ctok_MSGVAR:
			dbgoprint((obj_t*) pThis, "MSGVAR\n");
			CHKiRet(ctok_token.UnlinkVar(pToken, &pVar));
			CHKiRet(vmprg.AddVarOperation(pThis->pVmprg, opcode_PUSHMSGVAR, pVar)); /* add to program */
			break;
		case ctok_CEEVAR:
			dbgoprint((obj_t*) pThis, "SYSVAR\n");
			CHKiRet(ctok_token.UnlinkVar(pToken, &pVar));
			CHKiRet(vmprg.AddVarOperation(pThis->pVmprg, opcode_PUSHCEEVAR, pVar)); /* add to program */
			break;
		case ctok_SYSVAR:
			dbgoprint((obj_t*) pThis, "SYSVAR\n");
			CHKiRet(ctok_token.UnlinkVar(pToken, &pVar));
			CHKiRet(vmprg.AddVarOperation(pThis->pVmprg, opcode_PUSHSYSVAR, pVar)); /* add to program */
			break;
		case ctok_LPAREN:
			dbgoprint((obj_t*) pThis, "expr\n");
			CHKiRet(ctok_token.Destruct(&pToken)); /* "eat" processed token */
			CHKiRet(expr(pThis, tok));
			CHKiRet(ctok.GetToken(tok, &pToken)); /* get next one */
			if(pToken->tok != ctok_RPAREN)
				ABORT_FINALIZE(RS_RET_SYNTAX_ERROR);
			break;
		default:
			dbgoprint((obj_t*) pThis, "invalid token %d\n", pToken->tok);
			ABORT_FINALIZE(RS_RET_SYNTAX_ERROR);
			break;
	}

finalize_it:
	if(pToken != NULL) {
		ctok_token.Destruct(&pToken); /* "eat" processed token */
	}

	RETiRet;
}

static rsRetVal
factor(expr_t *pThis, ctok_t *tok)
{
	DEFiRet;
	ctok_token_t *pToken;
	int bWasNot;
	int bWasUnaryMinus;

	ISOBJ_TYPE_assert(pThis, expr);
	ISOBJ_TYPE_assert(tok, ctok);

	CHKiRet(ctok.GetToken(tok, &pToken));
	if(pToken->tok == ctok_NOT) {
		dbgprintf("not\n");
		bWasNot = 1;
		CHKiRet(ctok_token.Destruct(&pToken)); /* no longer needed */
		CHKiRet(ctok.GetToken(tok, &pToken)); /* get new one for next check */
	} else {
		bWasNot = 0;
	}

	if(pToken->tok == ctok_MINUS) {
		dbgprintf("unary minus\n");
		bWasUnaryMinus = 1;
		CHKiRet(ctok_token.Destruct(&pToken)); /* no longer needed */
	} else {
		bWasUnaryMinus = 0;
		/* we could not process the token, so push it back */
		CHKiRet(ctok.UngetToken(tok, pToken));
	}

	CHKiRet(terminal(pThis, tok));

	/* warning: the order if the two following ifs is important. Do not change them, this
	 * would change the semantics of the expression!
	 */
	if(bWasUnaryMinus) {
		CHKiRet(vmprg.AddVarOperation(pThis->pVmprg, opcode_UNARY_MINUS, NULL)); /* add to program */
	}

	if(bWasNot == 1) {
		CHKiRet(vmprg.AddVarOperation(pThis->pVmprg, opcode_NOT, NULL)); /* add to program */
	}

finalize_it:
	RETiRet;
}


static rsRetVal
term(expr_t *pThis, ctok_t *tok)
{
	DEFiRet;
	ctok_token_t *pToken;

	ISOBJ_TYPE_assert(pThis, expr);
	ISOBJ_TYPE_assert(tok, ctok);

	CHKiRet(factor(pThis, tok));

 	/* *(("*" / "/" / "%") factor) part */
	CHKiRet(ctok.GetToken(tok, &pToken));
	while(pToken->tok == ctok_TIMES || pToken->tok == ctok_DIV || pToken->tok == ctok_MOD) {
		dbgoprint((obj_t*) pThis, "/,*,%%\n");
		CHKiRet(factor(pThis, tok));
		CHKiRet(vmprg.AddVarOperation(pThis->pVmprg, (opcode_t) pToken->tok, NULL)); /* add to program */
		CHKiRet(ctok_token.Destruct(&pToken)); /* no longer needed */
		CHKiRet(ctok.GetToken(tok, &pToken));
	}

	/* unget the token that made us exit the loop - it's obviously not one
	 * we can process.
	 */
	CHKiRet(ctok.UngetToken(tok, pToken)); 

finalize_it:
	RETiRet;
}

static rsRetVal
val(expr_t *pThis, ctok_t *tok)
{
	DEFiRet;
	ctok_token_t *pToken;

	ISOBJ_TYPE_assert(pThis, expr);
	ISOBJ_TYPE_assert(tok, ctok);

	CHKiRet(term(pThis, tok));

 	/* *(("+" / "-") term) part */
	CHKiRet(ctok.GetToken(tok, &pToken));
	while(pToken->tok == ctok_PLUS || pToken->tok == ctok_MINUS || pToken->tok == ctok_STRADD) {
		dbgoprint((obj_t*) pThis, "+/-/&\n");
		CHKiRet(term(pThis, tok));
		CHKiRet(vmprg.AddVarOperation(pThis->pVmprg, (opcode_t) pToken->tok, NULL)); /* add to program */
		CHKiRet(ctok_token.Destruct(&pToken)); /* no longer needed */
		CHKiRet(ctok.GetToken(tok, &pToken));
	}

	/* unget the token that made us exit the loop - it's obviously not one
	 * we can process.
	 */
	CHKiRet(ctok.UngetToken(tok, pToken)); 

finalize_it:
	RETiRet;
}


static rsRetVal
e_cmp(expr_t *pThis, ctok_t *tok)
{
	DEFiRet;
	ctok_token_t *pToken;

	ISOBJ_TYPE_assert(pThis, expr);
	ISOBJ_TYPE_assert(tok, ctok);

	CHKiRet(val(pThis, tok));

 	/* 0*1(cmp_op val) part */
	CHKiRet(ctok.GetToken(tok, &pToken));
	if(ctok_token.IsCmpOp(pToken)) {
		dbgoprint((obj_t*) pThis, "cmp\n");
		CHKiRet(val(pThis, tok));
		CHKiRet(vmprg.AddVarOperation(pThis->pVmprg, (opcode_t) pToken->tok, NULL)); /* add to program */
		CHKiRet(ctok_token.Destruct(&pToken)); /* no longer needed */
	} else {
		/* we could not process the token, so push it back */
		CHKiRet(ctok.UngetToken(tok, pToken));
	}


finalize_it:
	RETiRet;
}


static rsRetVal
e_and(expr_t *pThis, ctok_t *tok)
{
	DEFiRet;
	ctok_token_t *pToken;

	ISOBJ_TYPE_assert(pThis, expr);
	ISOBJ_TYPE_assert(tok, ctok);

	CHKiRet(e_cmp(pThis, tok));

 	/* *("and" e_cmp) part */
	CHKiRet(ctok.GetToken(tok, &pToken));
	while(pToken->tok == ctok_AND) {
		dbgoprint((obj_t*) pThis, "and\n");
		CHKiRet(e_cmp(pThis, tok));
		CHKiRet(vmprg.AddVarOperation(pThis->pVmprg, opcode_AND, NULL)); /* add to program */
		CHKiRet(ctok_token.Destruct(&pToken)); /* no longer needed */
		CHKiRet(ctok.GetToken(tok, &pToken));
	}

	/* unget the token that made us exit the loop - it's obviously not one
	 * we can process.
	 */
	CHKiRet(ctok.UngetToken(tok, pToken)); 

finalize_it:
	RETiRet;
}


static rsRetVal
expr(expr_t *pThis, ctok_t *tok)
{
	DEFiRet;
	ctok_token_t *pToken;

	ISOBJ_TYPE_assert(pThis, expr);
	ISOBJ_TYPE_assert(tok, ctok);

	CHKiRet(e_and(pThis, tok));

 	/* *("or" e_and) part */
	CHKiRet(ctok.GetToken(tok, &pToken));
	while(pToken->tok == ctok_OR) {
		dbgoprint((obj_t*) pThis, "found OR\n");
		CHKiRet(e_and(pThis, tok));
		CHKiRet(vmprg.AddVarOperation(pThis->pVmprg, opcode_OR, NULL)); /* add to program */
		CHKiRet(ctok_token.Destruct(&pToken)); /* no longer needed */
		CHKiRet(ctok.GetToken(tok, &pToken));
	}

	/* unget the token that made us exit the loop - it's obviously not one
	 * we can process.
	 */
	CHKiRet(ctok.UngetToken(tok, pToken)); 

finalize_it:
	RETiRet;
}


/* ------------------------------ end parser functions ------------------------------ */


/* ------------------------------ actual expr object functions ------------------------------ */

/* Standard-Constructor
 * rgerhards, 2008-02-09 (a rainy Tenerife return flight day ;))
 */
BEGINobjConstruct(expr) /* be sure to specify the object type also in END macro! */
ENDobjConstruct(expr)


/* ConstructionFinalizer
 * rgerhards, 2008-01-09
 */
rsRetVal exprConstructFinalize(expr_t __attribute__((unused)) *pThis)
{
	DEFiRet;

	ISOBJ_TYPE_assert(pThis, expr);

	RETiRet;
}


/* destructor for the expr object */
BEGINobjDestruct(expr) /* be sure to specify the object type also in END and CODESTART macros! */
CODESTARTobjDestruct(expr)
	if(pThis->pVmprg != NULL)
		vmprg.Destruct(&pThis->pVmprg);
ENDobjDestruct(expr)


/* parse an expression object based on a given tokenizer
 * rgerhards, 2008-02-19
 */
rsRetVal
exprParse(expr_t *pThis, ctok_t *tok)
{
	DEFiRet;

	ISOBJ_TYPE_assert(pThis, expr);
	ISOBJ_TYPE_assert(tok, ctok);

	/* first, we need to make sure we have a program where we can add to what we parse... */
	CHKiRet(vmprg.Construct(&pThis->pVmprg));
	CHKiRet(vmprg.ConstructFinalize(pThis->pVmprg));

	/* happy parsing... */
	CHKiRet(expr(pThis, tok));
	dbgoprint((obj_t*) pThis, "successfully parsed/created expression\n");

finalize_it:
	RETiRet;
}


/* queryInterface function
 * rgerhards, 2008-02-21
 */
BEGINobjQueryInterface(expr)
CODESTARTobjQueryInterface(expr)
	if(pIf->ifVersion != exprCURR_IF_VERSION) { /* check for current version, increment on each change */
		ABORT_FINALIZE(RS_RET_INTERFACE_NOT_SUPPORTED);
	}

	/* ok, we have the right interface, so let's fill it
	 * Please note that we may also do some backwards-compatibility
	 * work here (if we can support an older interface version - that,
	 * of course, also affects the "if" above).
	 */
	pIf->Construct = exprConstruct;
	pIf->ConstructFinalize = exprConstructFinalize;
	pIf->Destruct = exprDestruct;
	pIf->Parse = exprParse;
finalize_it:
ENDobjQueryInterface(expr)


/* Initialize the expr class. Must be called as the very first method
 * before anything else is called inside this class.
 * rgerhards, 2008-02-19
 */
BEGINObjClassInit(expr, 1, OBJ_IS_CORE_MODULE) /* class, version */
	/* request objects we use */
	CHKiRet(objUse(var, CORE_COMPONENT));
	CHKiRet(objUse(vmprg, CORE_COMPONENT));
	CHKiRet(objUse(var, CORE_COMPONENT));
	CHKiRet(objUse(ctok_token, CORE_COMPONENT));
	CHKiRet(objUse(ctok, CORE_COMPONENT));

	OBJSetMethodHandler(objMethod_CONSTRUCTION_FINALIZER, exprConstructFinalize);
ENDObjClassInit(expr)

/* vi:set ai:
 */
