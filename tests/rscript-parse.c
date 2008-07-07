/* This test checks runtime initialization and exit. Other than that, it
 * also serves as the most simplistic sample of how a test can be coded.
 *
 * Part of the testbench for rsyslog.
 * Copyright 2008 Rainer Gerhards and Adiscon GmbH.
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
#include <stdio.h>

#include "rsyslog.h"
#include "testbench.h"
#include "ctok.h"
#include "expr.h"

MODULE_TYPE_TESTBENCH
/* define addtional objects we need for our tests */
DEFobjCurrIf(expr)
DEFobjCurrIf(ctok)
DEFobjCurrIf(ctok_token)
DEFobjCurrIf(vmprg)

BEGINInit
CODESTARTInit
	pErrObj = "expr"; CHKiRet(objUse(expr, CORE_COMPONENT));
	pErrObj = "ctok"; CHKiRet(objUse(ctok, CORE_COMPONENT));
	pErrObj = "ctok_token"; CHKiRet(objUse(ctok_token, CORE_COMPONENT));
	pErrObj = "vmprg"; CHKiRet(objUse(vmprg, CORE_COMPONENT));
ENDInit

BEGINExit
CODESTARTExit
ENDExit

BEGINTest
	cstr_t *pstrPrg;
	ctok_t *tok;
	ctok_token_t *pToken;
	expr_t *pExpr;
	/* the string below is an expression as defined up to 3.19.x - note that the
	 * then and the space after it MUST be present!
	 */
	//uchar szExpr[] = " $msg contains 'test' then ";
	uchar szExpr[] = "'test 1' <> $var or /* some comment */($SEVERITY == -4 +5 -(3 * - 2) and $fromhost == '127.0.0.1') then ";
	/*uchar szSynErr[] = "$msg == 1 and syntaxerror ";*/
CODESTARTTest
	/* we first need a tokenizer... */
	CHKiRet(ctok.Construct(&tok));
	CHKiRet(ctok.Setpp(tok, szExpr));
	CHKiRet(ctok.ConstructFinalize(tok));

	/* now construct our expression */
	CHKiRet(expr.Construct(&pExpr));
	CHKiRet(expr.ConstructFinalize(pExpr));

	/* ready to go... */
	CHKiRet(expr.Parse(pExpr, tok));

	/* we now need to parse off the "then" - and note an error if it is
	 * missing...
	 *
	 * rgerhards, 2008-07-01: we disable the check below, because I can not
	 * find the cause of the misalignment. The problem is that pToken structure has
	 * a different member alignment inside the runtime library then inside of
	 * this program. I checked compiler options, but could not find the cause.
	 * Should anyone have any insight, I'd really appreciate if you drop me 
	 * a line.
	 */
	CHKiRet(ctok.GetToken(tok, &pToken));
	if(pToken->tok != ctok_THEN) {
		ctok_token.Destruct(&pToken);
		ABORT_FINALIZE(RS_RET_SYNTAX_ERROR);
	}

	ctok_token.Destruct(&pToken); /* no longer needed */

	CHKiRet(rsCStrConstruct(&pstrPrg));
	CHKiRet(vmprg.Obj2Str(pExpr->pVmprg, pstrPrg));

	printf("string returned: '%s'\n", rsCStrGetSzStr(pstrPrg));
	rsCStrDestruct(&pstrPrg);

	/* we are done, so we now need to restore things */
	CHKiRet(ctok.Destruct(&tok));
finalize_it:
	/* here we may do custom error reporting */
	if(iRet != RS_RET_OK) {
		uchar *pp;
		ctok.Getpp(tok, &pp);
		printf("error on or before '%s'\n", pp);
	}
ENDTest
