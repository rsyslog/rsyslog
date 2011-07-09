/* rainerscript.c - routines to support RainerScript config language
 *
 * Module begun 2011-07-01 by Rainer Gerhards
 *
 * Copyright 2011 Rainer Gerhards and Adiscon GmbH.
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

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <glob.h>
#include <errno.h>
#include <sys/stat.h>
#include <libestr.h>
#include "rainerscript.h"
#include "parserif.h"
#include "grammar.h"

void
readConfFile(FILE *fp, es_str_t **str)
{
	char ln[10240];
	char buf[512];
	int lenBuf;
	int bWriteLineno = 0;
	int len, i;
	int start;	/* start index of to be submitted text */
	int bContLine = 0;
	int lineno = 0;

	*str = es_newStr(4096);

	while(fgets(ln, sizeof(ln), fp) != NULL) {
		++lineno;
		if(bWriteLineno) {
			bWriteLineno = 0;
			lenBuf = sprintf(buf, "PreprocFileLineNumber(%d)\n", lineno);
			es_addBuf(str, buf, lenBuf);
		}
		len = strlen(ln);
		/* if we are continuation line, we need to drop leading WS */
		if(bContLine) {
			for(start = 0 ; start < len && isspace(ln[start]) ; ++start)
				/* JUST SCAN */;
		} else {
			start = 0;
		}
		for(i = len - 1 ; i >= start && isspace(ln[i]) ; --i)
			/* JUST SCAN */;
		if(i >= 0) {
			if(ln[i] == '\\') {
				--i;
				bContLine = 1;
			} else {
				if(bContLine) /* write line number if we had cont line */
					bWriteLineno = 1;
				bContLine = 0;
			}
			/* add relevant data to buffer */
			es_addBuf(str, ln+start, i+1 - start);
		}
		if(!bContLine)
			es_addChar(str, '\n');
	}
	/* indicate end of buffer to flex */
	es_addChar(str, '\0');
	es_addChar(str, '\0');
}

struct nvlst*
nvlstNew(es_str_t *name, es_str_t *value)
{
	struct nvlst *lst;

	if((lst = malloc(sizeof(struct nvlst))) != NULL) {
		lst->next = NULL;
		lst->name = name;
		lst->value = value;
	}

	return lst;
}

void
nvlstDestruct(struct nvlst *lst)
{
	struct nvlst *toDel;

	while(lst != NULL) {
		toDel = lst;
		lst = lst->next;
		es_deleteStr(toDel->name);
		es_deleteStr(toDel->value);
		free(toDel);
	}
}

void
nvlstPrint(struct nvlst *lst)
{
	char *name, *value;
	dbgprintf("nvlst %p:\n", lst);
	while(lst != NULL) {
		name = es_str2cstr(lst->name, NULL);
		value = es_str2cstr(lst->value, NULL);
		dbgprintf("\tname: '%s', value '%s'\n", name, value);
		free(name);
		free(value);
		lst = lst->next;
	}
}

struct cnfobj*
cnfobjNew(enum cnfobjType objType, struct nvlst *lst)
{
	struct cnfobj *o;

	if((o = malloc(sizeof(struct nvlst))) != NULL) {
		o->objType = objType;
		o->nvlst = lst;
	}

	return o;
}

void
cnfobjDestruct(struct cnfobj *o)
{
	if(o != NULL) {
		nvlstDestruct(o->nvlst);
		free(o);
	}
}

void
cnfobjPrint(struct cnfobj *o)
{
	dbgprintf("obj: '%s'\n", cnfobjType2str(o->objType));
	nvlstPrint(o->nvlst);
}


struct cnfactlst*
cnfactlstNew(enum cnfactType actType, struct nvlst *lst, char *actLine)
{
	struct cnfactlst *actlst;

	if((actlst = malloc(sizeof(struct cnfactlst))) != NULL) {
		actlst->next = NULL;
		actlst->syslines = NULL;
		actlst->actType = actType;
		if(actType == CNFACT_V2)
			actlst->data.lst = lst;
		else
			actlst->data.legActLine = actLine;
	}
	return actlst;
}

struct cnfactlst*
cnfactlstAddSysline(struct cnfactlst* actlst, char *line)
{
	struct cnfcfsyslinelst *cflst;

	if((cflst = malloc(sizeof(struct cnfcfsyslinelst))) != NULL)   {
		cflst->next = NULL;
		cflst->line = line;
		if(actlst->syslines == NULL) {
			actlst->syslines = cflst;
		} else {
			cflst->next = actlst->syslines;
			actlst->syslines = cflst;
		}
	}
	return actlst;
}

void
cnfactlstDestruct(struct cnfactlst *actlst)
{
	struct cnfactlst *toDel;

	while(actlst != NULL) {
		toDel = actlst;
		actlst = actlst->next;
		if(toDel->actType == CNFACT_V2)
			nvlstDestruct(toDel->data.lst);
		else
			free(toDel->data.legActLine);
		free(toDel);
	}
	
}

static inline struct cnfcfsyslinelst*
cnfcfsyslinelstReverse(struct cnfcfsyslinelst *lst)
{
	struct cnfcfsyslinelst *curr, *prev;
	if(lst == NULL)
		return NULL;
	prev = NULL;
	while(lst != NULL) {
		curr = lst;
		lst = lst->next;
		curr->next = prev;
		prev = curr;
	}
	return prev;
}

struct cnfactlst*
cnfactlstReverse(struct cnfactlst *actlst)
{
	struct cnfactlst *curr, *prev;

	prev = NULL;
	while(actlst != NULL) {
		//dbgprintf("reversing: %s\n", actlst->data.legActLine);
		curr = actlst;
		actlst = actlst->next;
		curr->syslines = cnfcfsyslinelstReverse(curr->syslines);
		curr->next = prev;
		prev = curr;
	}
	return prev;
}

void
cnfactlstPrint(struct cnfactlst *actlst)
{
	struct cnfcfsyslinelst *cflst;

	while(actlst != NULL) {
		dbgprintf("aclst %p: ", actlst);
		if(actlst->actType == CNFACT_V2) {
			dbgprintf("V2 action type: ");
			nvlstPrint(actlst->data.lst);
		} else {
			dbgprintf("legacy action line: '%s'\n",
				actlst->data.legActLine);
		}
		for(  cflst = actlst->syslines
		    ; cflst != NULL ; cflst = cflst->next) {
			dbgprintf("action:cfsysline: '%s'\n", cflst->line);
		}
		actlst = actlst->next;
	}
}

struct cnfexpr*
cnfexprNew(unsigned nodetype, struct cnfexpr *l, struct cnfexpr *r)
{
	struct cnfexpr *expr;

	/* optimize some constructs during parsing */
	if(nodetype == 'M' && r->nodetype == 'N') {
		((struct cnfnumval*)r)->val *= -1;
		expr = r;
		goto done;
	}

	if((expr = malloc(sizeof(struct cnfexpr))) != NULL) {
		expr->nodetype = nodetype;
		expr->l = l;
		expr->r = r;
	}
done:
	return expr;
}


/* ensure that retval is a number; if string is no number,
 * try to convert it to one. The semantics from es_str2num()
 * are used (bSuccess tells if the conversion went well or not).
 */
static inline long long
exprret2Number(struct exprret *r, int *bSuccess)
{
	long long n;
	if(r->datatype == 'S') {
		n = es_str2num(r->d.estr, bSuccess);
	} else {
		*bSuccess = 1;
	}
	return r->d.n;
}

/* ensure that retval is a string; if string is no number,
 * emit error message and set number to 0.
 */
static inline es_str_t *
exprret2String(struct exprret *r, int *bMustFree)
{
	if(r->datatype == 'N') {
		*bMustFree = 1;
		return es_newStrFromNumber(r->d.n);
	}
	*bMustFree = 0;
	return r->d.estr;
}

/* Perform a function call. This has been moved out of cnfExprEval in order
 * to keep the code small and easier to maintain.
 */
static inline void
doFuncCall(struct cnffunc *func, struct exprret *ret, void* usrptr)
{
	char *fname;
	char *envvar;
	int bMustFree;
	es_str_t *estr;
	char *str;
	struct exprret r[CNFFUNC_MAX_ARGS];

	dbgprintf("rainerscript: executing function id %d\n", func->fID);
	switch(func->fID) {
	case CNFFUNC_STRLEN:
		if(func->expr[0]->nodetype == 'S') {
			/* if we already have a string, we do not need to
			 * do one more recursive call.
			 */
			ret->d.n = es_strlen(((struct cnfstringval*) func->expr[0])->estr);
		} else {
			cnfexprEval(func->expr[0], &r[0], usrptr);
			estr = exprret2String(&r[0], &bMustFree);
			ret->d.n = es_strlen(estr);
			if(bMustFree) es_deleteStr(estr);
		}
		ret->datatype = 'N';
		break;
	case CNFFUNC_GETENV:
		/* note: the optimizer shall have replaced calls to getenv()
		 * with a constant argument to a single string (once obtained via
		 * getenv()). So we do NOT need to check if there is just a
		 * string following.
		 */
		cnfexprEval(func->expr[0], &r[0], usrptr);
		estr = exprret2String(&r[0], &bMustFree);
		str = (char*) es_str2cstr(estr, NULL);
		envvar = getenv(str);
		ret->datatype = 'S';
		ret->d.estr = es_newStrFromCStr(envvar, strlen(envvar));
		if(bMustFree) es_deleteStr(estr);
		if(r[0].datatype == 'S') es_deleteStr(r[0].d.estr);
		free(str);
		break;
	case CNFFUNC_TOLOWER:
		cnfexprEval(func->expr[0], &r[0], usrptr);
		estr = exprret2String(&r[0], &bMustFree);
		if(!bMustFree) /* let caller handle that M) */
			estr = es_strdup(estr);
		es_tolower(estr);
		ret->datatype = 'S';
		ret->d.estr = estr;
		break;
	case CNFFUNC_CSTR:
		cnfexprEval(func->expr[0], &r[0], usrptr);
		estr = exprret2String(&r[0], &bMustFree);
		if(!bMustFree) /* let caller handle that M) */
			estr = es_strdup(estr);
		ret->datatype = 'S';
		ret->d.estr = estr;
		break;
	case CNFFUNC_CNUM:
		if(func->expr[0]->nodetype == 'N') {
			ret->d.n = ((struct cnfnumval*)func->expr[0])->val;
		} else if(func->expr[0]->nodetype == 'S') {
			ret->d.n = es_str2num(((struct cnfstringval*) func->expr[0])->estr,
					      NULL);
		} else {
			cnfexprEval(func->expr[0], &r[0], usrptr);
			ret->d.n = exprret2Number(&r[0], NULL);
			if(r[0].datatype == 'S') es_deleteStr(r[0].d.estr);
		}
		ret->datatype = 'N';
		break;
	default:
		if(Debug) {
			fname = es_str2cstr(func->fname, NULL);
			dbgprintf("rainerscript: invalid function id %u (name '%s')\n",
				  (unsigned) func->fID, fname);
			free(fname);
		}
		ret->datatype = 'N';
		ret->d.n = 0;
	}
}

#define FREE_BOTH_RET \
		if(r.datatype == 'S') es_deleteStr(r.d.estr); \
		if(l.datatype == 'S') es_deleteStr(l.d.estr)

#define COMP_NUM_BINOP(x) \
	cnfexprEval(expr->l, &l, usrptr); \
	cnfexprEval(expr->r, &r, usrptr); \
	ret->datatype = 'N'; \
	ret->d.n = exprret2Number(&l, &convok_l) x exprret2Number(&r, &convok_r); \
	FREE_BOTH_RET

#define PREP_TWO_STRINGS \
		cnfexprEval(expr->l, &l, usrptr); \
		estr_l = exprret2String(&l, &bMustFree2); \
		cnfexprEval(expr->r, &r, usrptr); \
		estr_r = exprret2String(&r, &bMustFree)

#define FREE_TWO_STRINGS \
		if(bMustFree) es_deleteStr(estr_r); \
		if(bMustFree2) es_deleteStr(estr_l); \
		FREE_BOTH_RET

/* evaluate an expression.
 * Note that we try to avoid malloc whenever possible (because of
 * the large overhead it has, especially on highly threaded programs).
 * As such, the each caller level must provide buffer space for the
 * result on its stack during recursion. This permits the callee to store
 * the return value without malloc. As the value is a somewhat larger
 * struct, we could otherwise not return it without malloc.
 * Note that we implement boolean shortcut operations. For our needs, there
 * simply is no case where full evaluation would make any sense at all.
 */
void
cnfexprEval(struct cnfexpr *expr, struct exprret *ret, void* usrptr)
{
	struct exprret r, l; /* memory for subexpression results */
	es_str_t *estr_r, *estr_l;
	int convok_r, convok_l;
	int bMustFree, bMustFree2;
	long long n_r, n_l;

	//dbgprintf("eval expr %p, type '%c'(%u)\n", expr, expr->nodetype, expr->nodetype);
	switch(expr->nodetype) {
	/* note: comparison operations are extremely similar. The code can be copyied, only
	 * places flagged with "CMP" need to be changed.
	 */
	case CMP_EQ:
		cnfexprEval(expr->l, &l, usrptr);
		cnfexprEval(expr->r, &r, usrptr);
		ret->datatype = 'N';
		if(l.datatype == 'S') {
			if(r.datatype == 'S') {
				ret->d.n = !es_strcmp(l.d.estr, r.d.estr); /*CMP*/
			} else {
				n_l = exprret2Number(&l, &convok_l);
				if(convok_l) {
					ret->d.n = (n_l == r.d.n); /*CMP*/
				} else {
					estr_r = exprret2String(&r, &bMustFree);
					ret->d.n = !es_strcmp(l.d.estr, estr_r); /*CMP*/
					if(bMustFree) es_deleteStr(estr_r);
				}
			}
		} else {
			if(r.datatype == 'S') {
				n_r = exprret2Number(&r, &convok_r);
				if(convok_r) {
					ret->d.n = (l.d.n == n_r); /*CMP*/
				} else {
					estr_l = exprret2String(&l, &bMustFree);
					ret->d.n = !es_strcmp(r.d.estr, estr_l); /*CMP*/
					if(bMustFree) es_deleteStr(estr_l);
				}
			} else {
				ret->d.n = (l.d.n == r.d.n); /*CMP*/
			}
		}
		FREE_BOTH_RET;
		break;
	case CMP_NE:
		cnfexprEval(expr->l, &l, usrptr);
		cnfexprEval(expr->r, &r, usrptr);
		ret->datatype = 'N';
		if(l.datatype == 'S') {
			if(r.datatype == 'S') {
				ret->d.n = es_strcmp(l.d.estr, r.d.estr); /*CMP*/
			} else {
				n_l = exprret2Number(&l, &convok_l);
				if(convok_l) {
					ret->d.n = (n_l != r.d.n); /*CMP*/
				} else {
					estr_r = exprret2String(&r, &bMustFree);
					ret->d.n = es_strcmp(l.d.estr, estr_r); /*CMP*/
					if(bMustFree) es_deleteStr(estr_r);
				}
			}
		} else {
			if(r.datatype == 'S') {
				n_r = exprret2Number(&r, &convok_r);
				if(convok_r) {
					ret->d.n = (l.d.n != n_r); /*CMP*/
				} else {
					estr_l = exprret2String(&l, &bMustFree);
					ret->d.n = es_strcmp(r.d.estr, estr_l); /*CMP*/
					if(bMustFree) es_deleteStr(estr_l);
				}
			} else {
				ret->d.n = (l.d.n != r.d.n); /*CMP*/
			}
		}
		FREE_BOTH_RET;
		break;
	case CMP_LE:
		cnfexprEval(expr->l, &l, usrptr);
		cnfexprEval(expr->r, &r, usrptr);
		ret->datatype = 'N';
		if(l.datatype == 'S') {
			if(r.datatype == 'S') {
				ret->d.n = es_strcmp(l.d.estr, r.d.estr) <= 0; /*CMP*/
			} else {
				n_l = exprret2Number(&l, &convok_l);
				if(convok_l) {
					ret->d.n = (n_l <= r.d.n); /*CMP*/
				} else {
					estr_r = exprret2String(&r, &bMustFree);
					ret->d.n = es_strcmp(l.d.estr, estr_r) <= 0; /*CMP*/
					if(bMustFree) es_deleteStr(estr_r);
				}
			}
		} else {
			if(r.datatype == 'S') {
				n_r = exprret2Number(&r, &convok_r);
				if(convok_r) {
					ret->d.n = (l.d.n <= n_r); /*CMP*/
				} else {
					estr_l = exprret2String(&l, &bMustFree);
					ret->d.n = es_strcmp(r.d.estr, estr_l) <= 0; /*CMP*/
					if(bMustFree) es_deleteStr(estr_l);
				}
			} else {
				ret->d.n = (l.d.n <= r.d.n); /*CMP*/
			}
		}
		FREE_BOTH_RET;
		break;
	case CMP_GE:
		cnfexprEval(expr->l, &l, usrptr);
		cnfexprEval(expr->r, &r, usrptr);
		ret->datatype = 'N';
		if(l.datatype == 'S') {
			if(r.datatype == 'S') {
				ret->d.n = es_strcmp(l.d.estr, r.d.estr) >= 0; /*CMP*/
			} else {
				n_l = exprret2Number(&l, &convok_l);
				if(convok_l) {
					ret->d.n = (n_l >= r.d.n); /*CMP*/
				} else {
					estr_r = exprret2String(&r, &bMustFree);
					ret->d.n = es_strcmp(l.d.estr, estr_r) >= 0; /*CMP*/
					if(bMustFree) es_deleteStr(estr_r);
				}
			}
		} else {
			if(r.datatype == 'S') {
				n_r = exprret2Number(&r, &convok_r);
				if(convok_r) {
					ret->d.n = (l.d.n >= n_r); /*CMP*/
				} else {
					estr_l = exprret2String(&l, &bMustFree);
					ret->d.n = es_strcmp(r.d.estr, estr_l) >= 0; /*CMP*/
					if(bMustFree) es_deleteStr(estr_l);
				}
			} else {
				ret->d.n = (l.d.n >= r.d.n); /*CMP*/
			}
		}
		FREE_BOTH_RET;
		break;
	case CMP_LT:
		cnfexprEval(expr->l, &l, usrptr);
		cnfexprEval(expr->r, &r, usrptr);
		ret->datatype = 'N';
		if(l.datatype == 'S') {
			if(r.datatype == 'S') {
				ret->d.n = es_strcmp(l.d.estr, r.d.estr) < 0; /*CMP*/
			} else {
				n_l = exprret2Number(&l, &convok_l);
				if(convok_l) {
					ret->d.n = (n_l < r.d.n); /*CMP*/
				} else {
					estr_r = exprret2String(&r, &bMustFree);
					ret->d.n = es_strcmp(l.d.estr, estr_r) < 0; /*CMP*/
					if(bMustFree) es_deleteStr(estr_r);
				}
			}
		} else {
			if(r.datatype == 'S') {
				n_r = exprret2Number(&r, &convok_r);
				if(convok_r) {
					ret->d.n = (l.d.n < n_r); /*CMP*/
				} else {
					estr_l = exprret2String(&l, &bMustFree);
					ret->d.n = es_strcmp(r.d.estr, estr_l) < 0; /*CMP*/
					if(bMustFree) es_deleteStr(estr_l);
				}
			} else {
				ret->d.n = (l.d.n < r.d.n); /*CMP*/
			}
		}
		FREE_BOTH_RET;
		break;
	case CMP_GT:
		cnfexprEval(expr->l, &l, usrptr);
		cnfexprEval(expr->r, &r, usrptr);
		ret->datatype = 'N';
		if(l.datatype == 'S') {
			if(r.datatype == 'S') {
				ret->d.n = es_strcmp(l.d.estr, r.d.estr) > 0; /*CMP*/
			} else {
				n_l = exprret2Number(&l, &convok_l);
				if(convok_l) {
					ret->d.n = (n_l > r.d.n); /*CMP*/
				} else {
					estr_r = exprret2String(&r, &bMustFree);
					ret->d.n = es_strcmp(l.d.estr, estr_r) > 0; /*CMP*/
					if(bMustFree) es_deleteStr(estr_r);
				}
			}
		} else {
			if(r.datatype == 'S') {
				n_r = exprret2Number(&r, &convok_r);
				if(convok_r) {
					ret->d.n = (l.d.n > n_r); /*CMP*/
				} else {
					estr_l = exprret2String(&l, &bMustFree);
					ret->d.n = es_strcmp(r.d.estr, estr_l) > 0; /*CMP*/
					if(bMustFree) es_deleteStr(estr_l);
				}
			} else {
				ret->d.n = (l.d.n > r.d.n); /*CMP*/
			}
		}
		FREE_BOTH_RET;
		break;
	case CMP_STARTSWITH:
		PREP_TWO_STRINGS;
		ret->datatype = 'N';
		ret->d.n = es_strncmp(estr_l, estr_r, estr_r->lenStr) == 0;
		FREE_TWO_STRINGS;
		break;
	case CMP_STARTSWITHI:
		PREP_TWO_STRINGS;
		ret->datatype = 'N';
		ret->d.n = es_strncasecmp(estr_l, estr_r, estr_r->lenStr) == 0;
		FREE_TWO_STRINGS;
		break;
	case CMP_CONTAINS:
		PREP_TWO_STRINGS;
		ret->datatype = 'N';
		ret->d.n = es_strContains(estr_l, estr_r) != -1;
		FREE_TWO_STRINGS;
		break;
	case CMP_CONTAINSI:
		PREP_TWO_STRINGS;
		ret->datatype = 'N';
		ret->d.n = es_strCaseContains(estr_l, estr_r) != -1;
		FREE_TWO_STRINGS;
		break;
	case OR:
		cnfexprEval(expr->l, &l, usrptr);
		ret->datatype = 'N';
		if(exprret2Number(&l, &convok_l)) {
			ret->d.n = 1ll;
		} else {
			cnfexprEval(expr->r, &r, usrptr);
			if(exprret2Number(&r, &convok_r))
				ret->d.n = 1ll;
			else 
				ret->d.n = 0ll;
		}
		FREE_BOTH_RET;
		break;
	case AND:
		cnfexprEval(expr->l, &l, usrptr);
		ret->datatype = 'N';
		if(exprret2Number(&l, &convok_l)) {
			cnfexprEval(expr->r, &r, usrptr);
			if(exprret2Number(&r, &convok_r))
				ret->d.n = 1ll;
			else 
				ret->d.n = 0ll;
		} else {
			ret->d.n = 0ll;
		}
		FREE_BOTH_RET;
		break;
	case NOT:
		cnfexprEval(expr->r, &r, usrptr);
		ret->datatype = 'N';
		ret->d.n = !exprret2Number(&r, &convok_r);
		if(r.datatype == 'S') es_deleteStr(r.d.estr);
		break;
	case 'N':
		ret->datatype = 'N';
		ret->d.n = ((struct cnfnumval*)expr)->val;
		break;
	case 'S':
		ret->datatype = 'S';
		ret->d.estr = es_strdup(((struct cnfstringval*)expr)->estr);
		break;
	case 'V':
		ret->datatype = 'S';
		ret->d.estr = cnfGetVar(((struct cnfvar*)expr)->name, usrptr);
		break;
	case '+':
		COMP_NUM_BINOP(+);
		break;
	case '-':
		COMP_NUM_BINOP(-);
		break;
	case '*':
		COMP_NUM_BINOP(*);
		break;
	case '/':
		COMP_NUM_BINOP(/);
		break;
	case '%':
		COMP_NUM_BINOP(%);
		break;
	case 'M':
		cnfexprEval(expr->r, &r, usrptr);
		ret->datatype = 'N';
		ret->d.n = -exprret2Number(&r, &convok_r);
		if(r.datatype == 'S') es_deleteStr(r.d.estr);
		break;
	case 'F':
		doFuncCall((struct cnffunc*) expr, ret, usrptr);
		break;
	default:
		ret->datatype = 'N';
		ret->d.n = 0ll;
		dbgprintf("eval error: unknown nodetype %u['%c']\n",
			(unsigned) expr->nodetype, (char) expr->nodetype);
		break;
	}
}

/* Evaluate an expression as a bool. This is added because expressions are
 * mostly used inside filters, and so this function is quite common and
 * important.
 */
int
cnfexprEvalBool(struct cnfexpr *expr, void *usrptr)
{
	int convok;
	struct exprret ret;
	cnfexprEval(expr, &ret, usrptr);
	return exprret2Number(&ret, &convok);
}

inline static void
doIndent(int indent)
{
	int i;
	for(i = 0 ; i < indent ; ++i)
		dbgprintf("  ");
}
void
cnfexprPrint(struct cnfexpr *expr, int indent)
{
	struct cnffunc *func;
	int i;

	//dbgprintf("expr %p, indent %d, type '%c'\n", expr, indent, expr->nodetype);
	switch(expr->nodetype) {
	case CMP_EQ:
		cnfexprPrint(expr->l, indent+1);
		doIndent(indent);
		dbgprintf("==\n");
		cnfexprPrint(expr->r, indent+1);
		break;
	case CMP_NE:
		cnfexprPrint(expr->l, indent+1);
		doIndent(indent);
		dbgprintf("!=\n");
		cnfexprPrint(expr->r, indent+1);
		break;
	case CMP_LE:
		cnfexprPrint(expr->l, indent+1);
		doIndent(indent);
		dbgprintf("<=\n");
		cnfexprPrint(expr->r, indent+1);
		break;
	case CMP_GE:
		cnfexprPrint(expr->l, indent+1);
		doIndent(indent);
		dbgprintf(">=\n");
		cnfexprPrint(expr->r, indent+1);
		break;
	case CMP_LT:
		cnfexprPrint(expr->l, indent+1);
		doIndent(indent);
		dbgprintf("<\n");
		cnfexprPrint(expr->r, indent+1);
		break;
	case CMP_GT:
		cnfexprPrint(expr->l, indent+1);
		doIndent(indent);
		dbgprintf(">\n");
		cnfexprPrint(expr->r, indent+1);
		break;
	case CMP_CONTAINS:
		cnfexprPrint(expr->l, indent+1);
		doIndent(indent);
		dbgprintf("CONTAINS\n");
		cnfexprPrint(expr->r, indent+1);
		break;
	case CMP_CONTAINSI:
		cnfexprPrint(expr->l, indent+1);
		doIndent(indent);
		dbgprintf("CONTAINS_I\n");
		cnfexprPrint(expr->r, indent+1);
		break;
	case CMP_STARTSWITH:
		cnfexprPrint(expr->l, indent+1);
		doIndent(indent);
		dbgprintf("STARTSWITH\n");
		cnfexprPrint(expr->r, indent+1);
		break;
	case CMP_STARTSWITHI:
		cnfexprPrint(expr->l, indent+1);
		doIndent(indent);
		dbgprintf("STARTSWITH_I\n");
		cnfexprPrint(expr->r, indent+1);
		break;
	case OR:
		cnfexprPrint(expr->l, indent+1);
		doIndent(indent);
		dbgprintf("OR\n");
		cnfexprPrint(expr->r, indent+1);
		break;
	case AND:
		cnfexprPrint(expr->l, indent+1);
		doIndent(indent);
		dbgprintf("AND\n");
		cnfexprPrint(expr->r, indent+1);
		break;
	case NOT:
		doIndent(indent);
		dbgprintf("NOT\n");
		cnfexprPrint(expr->r, indent+1);
		break;
	case 'S':
		doIndent(indent);
		cstrPrint("string '", ((struct cnfstringval*)expr)->estr);
		dbgprintf("'\n");
		break;
	case 'N':
		doIndent(indent);
		dbgprintf("%lld\n", ((struct cnfnumval*)expr)->val);
		break;
	case 'V':
		doIndent(indent);
		dbgprintf("var '%s'\n", ((struct cnfvar*)expr)->name);
		break;
	case 'F':
		doIndent(indent);
		func = (struct cnffunc*) expr;
		cstrPrint("function '", func->fname);
		dbgprintf("' (id:%d, params:%hu)\n", func->fID, func->nParams);
		for(i = 0 ; i < func->nParams ; ++i) {
			cnfexprPrint(func->expr[i], indent+1);
		}
		break;
	case '+':
	case '-':
	case '*':
	case '/':
	case '%':
	case 'M':
		if(expr->l != NULL)
			cnfexprPrint(expr->l, indent+1);
		doIndent(indent);
		dbgprintf("%c\n", (char) expr->nodetype);
		cnfexprPrint(expr->r, indent+1);
		break;
	default:
		dbgprintf("error: unknown nodetype %u\n",
			(unsigned) expr->nodetype);
		break;
	}
}

struct cnfnumval*
cnfnumvalNew(long long val)
{
	struct cnfnumval *numval;
	if((numval = malloc(sizeof(struct cnfnumval))) != NULL) {
		numval->nodetype = 'N';
		numval->val = val;
	}
	return numval;
}

struct cnfstringval*
cnfstringvalNew(es_str_t *estr)
{
	struct cnfstringval *strval;
	if((strval = malloc(sizeof(struct cnfstringval))) != NULL) {
		strval->nodetype = 'S';
		strval->estr = estr;
	}
	return strval;
}

struct cnfvar*
cnfvarNew(char *name)
{
	struct cnfvar *var;
	if((var = malloc(sizeof(struct cnfvar))) != NULL) {
		var->nodetype = 'V';
		var->name = name;
	}
	return var;
}

struct cnfrule *
cnfruleNew(enum cnfFiltType filttype, struct cnfactlst *actlst)
{
	struct cnfrule* cnfrule;
	if((cnfrule = malloc(sizeof(struct cnfrule))) != NULL) {
		cnfrule->nodetype = 'R';
		cnfrule->filttype = filttype;
		cnfrule->actlst = cnfactlstReverse(actlst);
	}
	return cnfrule;
}

void
cnfrulePrint(struct cnfrule *rule)
{
	dbgprintf("------ start rule %p:\n", rule);
	dbgprintf("%s: ", cnfFiltType2str(rule->filttype));
	switch(rule->filttype) {
	case CNFFILT_NONE:
		break;
	case CNFFILT_PRI:
	case CNFFILT_PROP:
		dbgprintf("%s\n", rule->filt.s);
		break;
	case CNFFILT_SCRIPT:
		dbgprintf("\n");
		cnfexprPrint(rule->filt.expr, 0);
		break;
	}
	cnfactlstPrint(rule->actlst);
	dbgprintf("------ end rule %p\n", rule);
}

struct cnffparamlst *
cnffparamlstNew(struct cnfexpr *expr, struct cnffparamlst *next)
{
	struct cnffparamlst* lst;
	if((lst = malloc(sizeof(struct cnffparamlst))) != NULL) {
		lst->nodetype = 'P';
		lst->expr = expr;
		lst->next = next;
	}
	return lst;
}

/* Obtain function id from name AND number of params. Issues the
 * relevant error messages if errors are detected.
 */
static inline enum cnffuncid
funcName2ID(es_str_t *fname, unsigned short nParams)
{
	if(!es_strbufcmp(fname, (unsigned char*)"strlen", sizeof("strlen") - 1)) {
		if(nParams != 1) {
			parser_errmsg("number of parameters for strlen() must be one "
				      "but is %d.", nParams);
			return CNFFUNC_INVALID;
		}
		return CNFFUNC_STRLEN;
	} else if(!es_strbufcmp(fname, (unsigned char*)"getenv", sizeof("getenv") - 1)) {
		if(nParams != 1) {
			parser_errmsg("number of parameters for getenv() must be one "
				      "but is %d.", nParams);
			return CNFFUNC_INVALID;
		}
		return CNFFUNC_GETENV;
	} else if(!es_strbufcmp(fname, (unsigned char*)"tolower", sizeof("tolower") - 1)) {
		if(nParams != 1) {
			parser_errmsg("number of parameters for tolower() must be one "
				      "but is %d.", nParams);
			return CNFFUNC_INVALID;
		}
		return CNFFUNC_TOLOWER;
	} else if(!es_strbufcmp(fname, (unsigned char*)"cstr", sizeof("cstr") - 1)) {
		if(nParams != 1) {
			parser_errmsg("number of parameters for cstr() must be one "
				      "but is %d.", nParams);
			return CNFFUNC_INVALID;
		}
		return CNFFUNC_CSTR;
	} else if(!es_strbufcmp(fname, (unsigned char*)"cnum", sizeof("cnum") - 1)) {
		if(nParams != 1) {
			parser_errmsg("number of parameters for cnum() must be one "
				      "but is %d.", nParams);
			return CNFFUNC_INVALID;
		}
		return CNFFUNC_CNUM;
	} else {
		return CNFFUNC_INVALID;
	}
}

struct cnffunc *
cnffuncNew(es_str_t *fname, struct cnffparamlst* paramlst)
{
	struct cnffunc* func;
	struct cnffparamlst *param, *toDel;
	unsigned short i;
	unsigned short nParams;

	/* we first need to find out how many params we have */
	nParams = 0;
	for(param = paramlst ; param != NULL ; param = param->next)
		++nParams;
	if((func = malloc(sizeof(struct cnffunc) + (nParams * sizeof(struct cnfexp*))))
	   != NULL) {
		func->nodetype = 'F';
		func->fname = fname;
		func->nParams = nParams;
		func->fID = funcName2ID(fname, nParams);
		/* shuffle params over to array (access speed!) */
		param = paramlst;
		for(i = 0 ; i < nParams ; ++i) {
			func->expr[i] = param->expr;
			toDel = param;
			param = param->next;
			free(toDel);
		}
	}
	return func;
}

int
cnfDoInclude(char *name)
{
	char *cfgFile;
	unsigned i;
	int result;
	glob_t cfgFiles;
	struct stat fileInfo;

	/* Use GLOB_MARK to append a trailing slash for directories.
	 * Required by doIncludeDirectory().
	 */
	result = glob(name, GLOB_MARK, NULL, &cfgFiles);
	if(result == GLOB_NOSPACE || result == GLOB_ABORTED) {
#if 0
		char errStr[1024];
		rs_strerror_r(errno, errStr, sizeof(errStr));
		errmsg.LogError(0, RS_RET_FILE_NOT_FOUND, "error accessing config file or directory '%s': %s",
				pattern, errStr);
		ABORT_FINALIZE(RS_RET_FILE_NOT_FOUND);
#endif
		dbgprintf("includeconfig glob error %d\n", errno);
		return 1;
	}

	for(i = 0; i < cfgFiles.gl_pathc; i++) {
		cfgFile = cfgFiles.gl_pathv[i];

		if(stat(cfgFile, &fileInfo) != 0) 
			continue; /* continue with the next file if we can't stat() the file */

		if(S_ISREG(fileInfo.st_mode)) { /* config file */
			dbgprintf("requested to include config file '%s'\n", cfgFile);
			cnfSetLexFile(cfgFile);
		} else if(S_ISDIR(fileInfo.st_mode)) { /* config directory */
			if(strcmp(name, cfgFile)) {
				/* do not include ourselves! */
				dbgprintf("requested to include directory '%s'\n", cfgFile);
				cnfDoInclude(cfgFile);
			}
		} else {
			dbgprintf("warning: unable to process IncludeConfig directive '%s'\n", cfgFile);
		}
	}

	globfree(&cfgFiles);
	return 0;
}

void
cstrPrint(char *text, es_str_t *estr)
{
	char *str;
	str = es_str2cstr(estr, NULL);
	dbgprintf("%s%s", text, str);
	free(str);
}
