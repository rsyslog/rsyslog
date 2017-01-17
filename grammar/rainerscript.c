/* rainerscript.c - routines to support RainerScript config language
 *
 * Module begun 2011-07-01 by Rainer Gerhards
 *
 * Copyright 2011-2016 Rainer Gerhards and Adiscon GmbH.
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
#include <pwd.h>
#include <grp.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <libestr.h>
#include "rsyslog.h"
#include "rainerscript.h"
#include "conf.h"
#include "parserif.h"
#include "parse.h"
#include "rsconf.h"
#include "grammar.h"
#include "queue.h"
#include "srUtils.h"
#include "regexp.h"
#include "obj.h"
#include "modules.h"
#include "ruleset.h"
#include "msg.h"
#include "wti.h"
#include "unicode-helper.h"

#if !defined(_AIX)
#pragma GCC diagnostic ignored "-Wswitch-enum"
#endif

DEFobjCurrIf(obj)
DEFobjCurrIf(regexp)

struct cnfexpr* cnfexprOptimize(struct cnfexpr *expr);
static void cnfstmtOptimizePRIFilt(struct cnfstmt *stmt);
static void cnfarrayPrint(struct cnfarray *ar, int indent);
struct cnffunc * cnffuncNew_prifilt(int fac);

/* debug support: convert token to a human-readable string. Note that
 * this function only supports a single thread due to a static buffer.
 * This is deemed a solid solution, as it is intended to be used during
 * startup, only.
 * NOTE: This function MUST be updated if new tokens are defined in the
 *       grammar.
 */
static const char *
tokenToString(const int token)
{
	const char *tokstr;
	static char tokbuf[512];

	switch(token) {
	case NAME: tokstr = "NAME"; break;
	case FUNC: tokstr = "FUNC"; break;
	case BEGINOBJ: tokstr ="BEGINOBJ"; break;
	case ENDOBJ: tokstr ="ENDOBJ"; break;
	case BEGIN_ACTION: tokstr ="BEGIN_ACTION"; break;
	case BEGIN_PROPERTY: tokstr ="BEGIN_PROPERTY"; break;
	case BEGIN_CONSTANT: tokstr ="BEGIN_CONSTANT"; break;
	case BEGIN_TPL: tokstr ="BEGIN_TPL"; break;
	case BEGIN_RULESET: tokstr ="BEGIN_RULESET"; break;
	case STOP: tokstr ="STOP"; break;
	case SET: tokstr ="SET"; break;
	case UNSET: tokstr ="UNSET"; break;
	case CONTINUE: tokstr ="CONTINUE"; break;
	case CALL: tokstr ="CALL"; break;
	case LEGACY_ACTION: tokstr ="LEGACY_ACTION"; break;
	case LEGACY_RULESET: tokstr ="LEGACY_RULESET"; break;
	case PRIFILT: tokstr ="PRIFILT"; break;
	case PROPFILT: tokstr ="PROPFILT"; break;
	case IF: tokstr ="IF"; break;
	case THEN: tokstr ="THEN"; break;
	case ELSE: tokstr ="ELSE"; break;
	case OR: tokstr ="OR"; break;
	case AND: tokstr ="AND"; break;
	case NOT: tokstr ="NOT"; break;
	case VAR: tokstr ="VAR"; break;
	case STRING: tokstr ="STRING"; break;
	case NUMBER: tokstr ="NUMBER"; break;
	case CMP_EQ: tokstr ="CMP_EQ"; break;
	case CMP_NE: tokstr ="CMP_NE"; break;
	case CMP_LE: tokstr ="CMP_LE"; break;
	case CMP_GE: tokstr ="CMP_GE"; break;
	case CMP_LT: tokstr ="CMP_LT"; break;
	case CMP_GT: tokstr ="CMP_GT"; break;
	case CMP_CONTAINS: tokstr ="CMP_CONTAINS"; break;
	case CMP_CONTAINSI: tokstr ="CMP_CONTAINSI"; break;
	case CMP_STARTSWITH: tokstr ="CMP_STARTSWITH"; break;
	case CMP_STARTSWITHI: tokstr ="CMP_STARTSWITHI"; break;
	case UMINUS: tokstr ="UMINUS"; break;
	case '&': tokstr ="&"; break;
	case '+': tokstr ="+"; break;
	case '-': tokstr ="-"; break;
	case '*': tokstr ="*"; break;
	case '/': tokstr ="/"; break;
	case '%': tokstr ="%"; break;
	case 'M': tokstr ="M"; break;
	case 'N': tokstr ="N"; break;
	case 'S': tokstr ="S"; break;
	case 'V': tokstr ="V"; break;
	case 'F': tokstr ="F"; break;
	case 'A': tokstr ="A"; break;
	default: snprintf(tokbuf, sizeof(tokbuf), "%c[%d]", token, token);
		 tokstr = tokbuf; break;
	}
	return tokstr;
}


const char*
getFIOPName(const unsigned iFIOP)
{
	const char *pRet;
	switch(iFIOP) {
		case FIOP_CONTAINS:
			pRet = "contains";
			break;
		case FIOP_ISEQUAL:
			pRet = "isequal";
			break;
		case FIOP_STARTSWITH:
			pRet = "startswith";
			break;
		case FIOP_REGEX:
			pRet = "regex";
			break;
		case FIOP_EREREGEX:
			pRet = "ereregex";
			break;
		case FIOP_ISEMPTY:
			pRet = "isempty";
			break;
		default:
			pRet = "NOP";
			break;
	}
	return pRet;
}

const char*
cnfFiltType2str(const enum cnfFiltType filttype)
{
	switch(filttype) {
	case CNFFILT_NONE:
		return("filter:none");
	case CNFFILT_PRI:
		return("filter:pri");
	case CNFFILT_PROP:
		return("filter:prop");
	case CNFFILT_SCRIPT:
		return("filter:script");
	default:
		return("error:invalid_filter_type");	/* should never be reached */
	}
}

const char*
cnfobjType2str(const enum cnfobjType ot)
{
	switch(ot) {
	case CNFOBJ_ACTION:
		return "action";
		break;
	case CNFOBJ_RULESET:
		return "ruleset";
		break;
	case CNFOBJ_GLOBAL:
		return "global";
		break;
	case CNFOBJ_INPUT:
		return "input";
		break;
	case CNFOBJ_MODULE:
		return "module";
		break;
	case CNFOBJ_TPL:
		return "template";
		break;
	case CNFOBJ_PROPERTY:
		return "property";
		break;
	case CNFOBJ_CONSTANT:
		return "constant";
		break;
	case CNFOBJ_MAINQ:
		return "main_queue";
	case CNFOBJ_LOOKUP_TABLE:
		return "lookup_table";
	case CNFOBJ_PARSER:
		return "parser";
		break;
	case CNFOBJ_TIMEZONE:
		return "timezone";
		break;
	case CNFOBJ_DYN_STATS:
		return "dyn_stats";
		break;
	default:return "error: invalid cnfobjType";
	}
}

/* This function takes the filter part of a property
 * based filter and decodes it. It processes the line up to the beginning
 * of the action part.
 */
static rsRetVal
DecodePropFilter(uchar *pline, struct cnfstmt *stmt)
{
	rsParsObj *pPars = NULL;
	cstr_t *pCSCompOp = NULL;
	cstr_t *pCSPropName = NULL;
	int iOffset; /* for compare operations */
	DEFiRet;

	ASSERT(pline != NULL);

	DBGPRINTF("Decoding property-based filter '%s'\n", pline);

	/* create parser object starting with line string without leading colon */
	if((iRet = rsParsConstructFromSz(&pPars, pline+1)) != RS_RET_OK) {
		parser_errmsg("error %d constructing parser object", iRet);
		FINALIZE;
	}

	/* read property */
	iRet = parsDelimCStr(pPars, &pCSPropName, ',', 1, 1, 1);
	if(iRet != RS_RET_OK) {
		parser_errmsg("error %d parsing filter property", iRet);
		rsParsDestruct(pPars);
		FINALIZE;
	}
	CHKiRet(msgPropDescrFill(&stmt->d.s_propfilt.prop, cstrGetSzStrNoNULL(pCSPropName),
		cstrLen(pCSPropName)));

	/* read operation */
	iRet = parsDelimCStr(pPars, &pCSCompOp, ',', 1, 1, 1);
	if(iRet != RS_RET_OK) {
		parser_errmsg("error %d compare operation property - ignoring selector", iRet);
		rsParsDestruct(pPars);
		FINALIZE;
	}

	/* we now first check if the condition is to be negated. To do so, we first
	 * must make sure we have at least one char in the param and then check the
	 * first one.
	 * rgerhards, 2005-09-26
	 */
	if(rsCStrLen(pCSCompOp) > 0) {
		if(*rsCStrGetBufBeg(pCSCompOp) == '!') {
			stmt->d.s_propfilt.isNegated = 1;
			iOffset = 1; /* ignore '!' */
		} else {
			stmt->d.s_propfilt.isNegated = 0;
			iOffset = 0;
		}
	} else {
		stmt->d.s_propfilt.isNegated = 0;
		iOffset = 0;
	}

	if(!rsCStrOffsetSzStrCmp(pCSCompOp, iOffset, (uchar*) "contains", 8)) {
		stmt->d.s_propfilt.operation = FIOP_CONTAINS;
	} else if(!rsCStrOffsetSzStrCmp(pCSCompOp, iOffset, (uchar*) "isequal", 7)) {
		stmt->d.s_propfilt.operation = FIOP_ISEQUAL;
	} else if(!rsCStrOffsetSzStrCmp(pCSCompOp, iOffset, (uchar*) "isempty", 7)) {
		stmt->d.s_propfilt.operation = FIOP_ISEMPTY;
	} else if(!rsCStrOffsetSzStrCmp(pCSCompOp, iOffset, (uchar*) "startswith", 10)) {
		stmt->d.s_propfilt.operation = FIOP_STARTSWITH;
	} else if(!rsCStrOffsetSzStrCmp(pCSCompOp, iOffset, (unsigned char*) "regex", 5)) {
		stmt->d.s_propfilt.operation = FIOP_REGEX;
	} else if(!rsCStrOffsetSzStrCmp(pCSCompOp, iOffset, (unsigned char*) "ereregex", 8)) {
		stmt->d.s_propfilt.operation = FIOP_EREREGEX;
	} else {
		parser_errmsg("error: invalid compare operation '%s'",
		           (char*) rsCStrGetSzStrNoNULL(pCSCompOp));
		ABORT_FINALIZE(RS_RET_ERR);
	}

	if(stmt->d.s_propfilt.operation != FIOP_ISEMPTY) {
		/* read compare value */
		iRet = parsQuotedCStr(pPars, &stmt->d.s_propfilt.pCSCompValue);
		if(iRet != RS_RET_OK) {
			parser_errmsg("error %d compare value property", iRet);
			rsParsDestruct(pPars);
			FINALIZE;
		}
	}

finalize_it:
	if(pPars != NULL)
		rsParsDestruct(pPars);
	if(pCSCompOp != NULL)
		rsCStrDestruct(&pCSCompOp);
	if(pCSPropName != NULL)
		cstrDestruct(&pCSPropName);
	RETiRet;
}

static void
prifiltInvert(struct funcData_prifilt *__restrict__ const prifilt)
{
	int i;
	for(i = 0 ; i < LOG_NFACILITIES+1 ; ++i) {
		prifilt->pmask[i] = ~prifilt->pmask[i];
	}
}

/* set prifilt so that it matches for some severities, sev is its numerical
 * value. Mode is one of the compop tokens CMP_EQ, CMP_LT, CMP_LE, CMP_GT,
 * CMP_GE, CMP_NE.
 */
static void
prifiltSetSeverity(struct funcData_prifilt *prifilt, int sev, int mode)
{
	static int lessthanmasks[] = { 0x00, 0x01, 0x03, 0x07, 0x0f, 0x1f, 0x3f, 0x7f, 0xff };
	int i;
	for(i = 0 ; i < LOG_NFACILITIES+1 ; ++i) {
		if(mode == CMP_EQ || mode == CMP_NE)
			prifilt->pmask[i] = 1 << sev;
		else if(mode == CMP_LT)
			prifilt->pmask[i] = lessthanmasks[sev];
		else if(mode == CMP_LE)
			prifilt->pmask[i] = lessthanmasks[sev+1];
		else if(mode == CMP_GT)
			prifilt->pmask[i] = ~lessthanmasks[sev+1];
		else if(mode == CMP_GE)
			prifilt->pmask[i] = ~lessthanmasks[sev];
		else
			DBGPRINTF("prifiltSetSeverity: program error, invalid mode %s\n",
				  tokenToString(mode));
	}
	if(mode == CMP_NE)
		prifiltInvert(prifilt);
}

/* set prifilt so that it matches for some facilities, fac is its numerical
 * value. Mode is one of the compop tokens CMP_EQ, CMP_LT, CMP_LE, CMP_GT,
 * CMP_GE, CMP_NE. For the given facilities, all severities are enabled.
 * NOTE: fac MUST be in the range 0..24 (not multiplied by 8)!
 */
static void
prifiltSetFacility(struct funcData_prifilt *__restrict__ const prifilt, const int fac, const int mode)
{
	int i;

	memset(prifilt->pmask, 0, sizeof(prifilt->pmask));
	switch(mode) {
	case CMP_EQ:
		prifilt->pmask[fac] = TABLE_ALLPRI;
		break;
	case CMP_NE:
		prifilt->pmask[fac] = TABLE_ALLPRI;
		prifiltInvert(prifilt);
		break;
	case CMP_LT:
		for(i = 0 ; i < fac ; ++i)
			prifilt->pmask[i] = TABLE_ALLPRI;
		break;
	case CMP_LE:
		for(i = 0 ; i < fac+1 ; ++i)
			prifilt->pmask[i] = TABLE_ALLPRI;
		break;
	case CMP_GE:
		for(i = fac ; i < LOG_NFACILITIES+1 ; ++i)
			prifilt->pmask[i] = TABLE_ALLPRI;
		break;
	case CMP_GT:
		for(i = fac+1 ; i < LOG_NFACILITIES+1 ; ++i)
			prifilt->pmask[i] = TABLE_ALLPRI;
		break;
	default:break;
	}
}

/* combine a prifilt with AND/OR (the respective token values are
 * used to keep things simple).
 */
static void
prifiltCombine(struct funcData_prifilt *__restrict__ const prifilt,
	       struct funcData_prifilt *__restrict__ const prifilt2,
	       const int mode)
{
	int i;
	for(i = 0 ; i < LOG_NFACILITIES+1 ; ++i) {
		if(mode == AND)
			prifilt->pmask[i] = prifilt->pmask[i] & prifilt2->pmask[i];
		else
			prifilt->pmask[i] = prifilt->pmask[i] | prifilt2->pmask[i];
	}
}


void
readConfFile(FILE * const fp, es_str_t **str)
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

/* comparison function for qsort() and bsearch() string array compare */
static int
qs_arrcmp(const void *s1, const void *s2)
{
	return es_strcmp(*((es_str_t**)s1), *((es_str_t**)s2));
}


struct objlst*
objlstNew(struct cnfobj *o)
{
	struct objlst *lst;

	if((lst = malloc(sizeof(struct objlst))) != NULL) {
		lst->next = NULL;
		lst->obj = o;
	}
cnfobjPrint(o);

	return lst;
}

/* add object to end of object list, always returns pointer to root object */
struct objlst*
objlstAdd(struct objlst *root, struct cnfobj *o)
{
	struct objlst *l;
	struct objlst *newl;
	
	newl = objlstNew(o);
	if(root == 0) {
		root = newl;
	} else { /* find last, linear search ok, as only during config phase */
		for(l = root ; l->next != NULL ; l = l->next)
			;
		l->next = newl;
	}
	return root;
}

/* add stmt to current script, always return root stmt pointer */
struct cnfstmt*
scriptAddStmt(struct cnfstmt *root, struct cnfstmt *s)
{
	struct cnfstmt *l;
	
	if(root == NULL) {
		root = s;
	} else { /* find last, linear search ok, as only during config phase */
		for(l = root ; l->next != NULL ; l = l->next)
			;
		l->next = s;
	}
	return root;
}

void
objlstDestruct(struct objlst *lst)
{
	struct objlst *toDel;

	while(lst != NULL) {
		toDel = lst;
		lst = lst->next;
		cnfobjDestruct(toDel->obj);
		free(toDel);
	}
}

void
objlstPrint(struct objlst *lst)
{
	dbgprintf("objlst %p:\n", lst);
	while(lst != NULL) {
		cnfobjPrint(lst->obj);
		lst = lst->next;
	}
}

struct nvlst*
nvlstNewStr(es_str_t *value)
{
	struct nvlst *lst;

	if((lst = malloc(sizeof(struct nvlst))) != NULL) {
		lst->next = NULL;
		lst->val.datatype = 'S';
		lst->val.d.estr = value;
		lst->bUsed = 0;
	}

	return lst;
}

struct nvlst*
nvlstNewArray(struct cnfarray *ar)
{
	struct nvlst *lst;

	if((lst = malloc(sizeof(struct nvlst))) != NULL) {
		lst->next = NULL;
		lst->val.datatype = 'A';
		lst->val.d.ar = ar;
		lst->bUsed = 0;
	}

	return lst;
}

struct nvlst*
nvlstSetName(struct nvlst *lst, es_str_t *name)
{
	lst->name = name;
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
		varDelete(&toDel->val);
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
		switch(lst->val.datatype) {
		case 'A':
			dbgprintf("\tname: '%s':\n", name);
			cnfarrayPrint(lst->val.d.ar, 5);
			break;
		case 'S':
			value = es_str2cstr(lst->val.d.estr, NULL);
			dbgprintf("\tname: '%s', value '%s'\n", name, value);
			free(value);
			break;
		default:dbgprintf("nvlstPrint: unknown type '%s'\n",
				tokenToString(lst->val.datatype));
			break;
		}
		free(name);
		lst = lst->next;
	}
}

/* find a name starting at node lst. Returns node with this
 * name or NULL, if none found.
 */
struct nvlst*
nvlstFindName(struct nvlst *lst, es_str_t *name)
{
	while(lst != NULL && es_strcmp(lst->name, name))
		lst = lst->next;
	return lst;
}


/* find a name starting at node lst. Same as nvlstFindName, but
 * for classical C strings. This is useful because the config system
 * uses C string constants.
 */
static struct nvlst*
nvlstFindNameCStr(struct nvlst *lst, const char *const __restrict__ name)
{
	es_size_t lenName = strlen(name);
	while(lst != NULL && es_strcasebufcmp(lst->name, (uchar*)name, lenName))
		lst = lst->next;
	return lst;
}


/* check if there are duplicate names inside a nvlst and emit
 * an error message, if so.
 */
static void
nvlstChkDupes(struct nvlst *lst)
{
	char *cstr;

	while(lst != NULL) {
		if(nvlstFindName(lst->next, lst->name) != NULL) {
			cstr = es_str2cstr(lst->name, NULL);
			parser_errmsg("duplicate parameter '%s' -- "
			  "interpretation is ambigious, one value "
			  "will be randomly selected. Fix this problem.",
			  cstr);
			free(cstr);
		}
		lst = lst->next;
	}
}


/* check for unused params and emit error message is found. This must
 * be called after all config params have been pulled from the object
 * (otherwise the flags are not correctly set).
 */
void
nvlstChkUnused(struct nvlst *lst)
{
	char *cstr;

	while(lst != NULL) {
		if(!lst->bUsed) {
			cstr = es_str2cstr(lst->name, NULL);
			parser_errmsg("parameter '%s' not known -- "
			  "typo in config file?", 
			  cstr);
			free(cstr);
		}
		lst = lst->next;
	}
}


static int
doGetSize(struct nvlst *valnode, struct cnfparamdescr *param,
	  struct cnfparamvals *val)
{
	unsigned char *c;
	es_size_t i;
	long long n;
	int r;
	c = es_getBufAddr(valnode->val.d.estr);
	n = 0;
	i = 0;
	while(i < es_strlen(valnode->val.d.estr) && isdigit(*c)) {
		n = 10 * n + *c - '0';
		++i;
		++c;
	}
	if(i < es_strlen(valnode->val.d.estr)) {
		++i;
		switch(*c) {
		/* traditional binary-based definitions */
		case 'k': n *= 1024; break;
		case 'm': n *= 1024 * 1024; break;
		case 'g': n *= 1024 * 1024 * 1024; break;
		case 't': n *= (int64) 1024 * 1024 * 1024 * 1024; break; /* tera */
		case 'p': n *= (int64) 1024 * 1024 * 1024 * 1024 * 1024; break; /* peta */
		case 'e': n *= (int64) 1024 * 1024 * 1024 * 1024 * 1024 * 1024; break; /* exa */
		/* and now the "new" 1000-based definitions */
		case 'K': n *= 1000; break;
	        case 'M': n *= 1000000; break;
                case 'G': n *= 1000000000; break;
			  /* we need to use the multiplication below because otherwise
			   * the compiler gets an error during constant parsing */
                case 'T': n *= (int64) 1000       * 1000000000; break; /* tera */
                case 'P': n *= (int64) 1000000    * 1000000000; break; /* peta */
                case 'E': n *= (int64) 1000000000 * 1000000000; break; /* exa */
		default: --i; break; /* indicates error */
		}
	}
	if(i == es_strlen(valnode->val.d.estr)) {
		val->val.datatype = 'N';
		val->val.d.n = n;
		r = 1;
	} else {
		parser_errmsg("parameter '%s' does not contain a valid size",
			      param->name);
		r = 0;
	}
	return r;
}


static int
doGetBinary(struct nvlst *valnode, struct cnfparamdescr *param,
	  struct cnfparamvals *val)
{
	int r = 1;
	val->val.datatype = 'N';
	if(!es_strbufcmp(valnode->val.d.estr, (unsigned char*) "on", 2)) {
		val->val.d.n = 1;
	} else if(!es_strbufcmp(valnode->val.d.estr, (unsigned char*) "off", 3)) {
		val->val.d.n = 0;
	} else {
		parser_errmsg("parameter '%s' must be \"on\" or \"off\" but "
		  "is neither. Results unpredictable.", param->name);
		val->val.d.n = 0;
		r = 0;
	}
	return r;
}

static int
doGetQueueType(struct nvlst *valnode, struct cnfparamdescr *param,
	  struct cnfparamvals *val)
{
	char *cstr;
	int r = 1;
	if(!es_strcasebufcmp(valnode->val.d.estr, (uchar*)"fixedarray", 10)) {
		val->val.d.n = QUEUETYPE_FIXED_ARRAY;
	} else if(!es_strcasebufcmp(valnode->val.d.estr, (uchar*)"linkedlist", 10)) {
		val->val.d.n = QUEUETYPE_LINKEDLIST;
	} else if(!es_strcasebufcmp(valnode->val.d.estr, (uchar*)"disk", 4)) {
		val->val.d.n = QUEUETYPE_DISK;
	} else if(!es_strcasebufcmp(valnode->val.d.estr, (uchar*)"direct", 6)) {
		val->val.d.n = QUEUETYPE_DIRECT;
	} else {
		cstr = es_str2cstr(valnode->val.d.estr, NULL);
		parser_errmsg("param '%s': unknown queue type: '%s'",
			      param->name, cstr);
		free(cstr);
		r = 0;
	}
	val->val.datatype = 'N';
	return r;
}


/* A file create-mode must be a four-digit octal number
 * starting with '0'.
 */
static int
doGetFileCreateMode(struct nvlst *valnode, struct cnfparamdescr *param,
	  struct cnfparamvals *val)
{
	int fmtOK = 0;
	char *cstr;
	uchar *c;

	if(es_strlen(valnode->val.d.estr) == 4) {
		c = es_getBufAddr(valnode->val.d.estr);
		if(    (c[0] == '0')
		    && (c[1] >= '0' && c[1] <= '7')
		    && (c[2] >= '0' && c[2] <= '7')
		    && (c[3] >= '0' && c[3] <= '7')  )  {
			fmtOK = 1;
		}
	}

	if(fmtOK) {
		val->val.datatype = 'N';
		val->val.d.n = (c[1]-'0') * 64 + (c[2]-'0') * 8 + (c[3]-'0');
	} else {
		cstr = es_str2cstr(valnode->val.d.estr, NULL);
		parser_errmsg("file modes need to be specified as "
		  "4-digit octal numbers starting with '0' -"
		  "parameter '%s=\"%s\"' is not a file mode",
		param->name, cstr);
		free(cstr);
	}
	return fmtOK;
}

static int
doGetGID(struct nvlst *valnode, struct cnfparamdescr *param,
	  struct cnfparamvals *val)
{
	char *cstr;
	int r;
	struct group *resultBuf;
	struct group wrkBuf;
	char stringBuf[2048]; /* 2048 has been proven to be large enough */

	cstr = es_str2cstr(valnode->val.d.estr, NULL);
	getgrnam_r(cstr, &wrkBuf, stringBuf, sizeof(stringBuf), &resultBuf);
	if(resultBuf == NULL) {
		parser_errmsg("parameter '%s': ID for group %s could not "
		  "be found", param->name, cstr);
		r = 0;
	} else {
		val->val.datatype = 'N';
		val->val.d.n = resultBuf->gr_gid;
		DBGPRINTF("param '%s': uid %d obtained for group '%s'\n",
		   param->name, (int) resultBuf->gr_gid, cstr);
		r = 1;
	}
	free(cstr);
	return r;
}

static int
doGetUID(struct nvlst *valnode, struct cnfparamdescr *param,
	  struct cnfparamvals *val)
{
	char *cstr;
	int r;
	struct passwd *resultBuf;
	struct passwd wrkBuf;
	char stringBuf[2048]; /* 2048 has been proven to be large enough */

	cstr = es_str2cstr(valnode->val.d.estr, NULL);
	getpwnam_r(cstr, &wrkBuf, stringBuf, sizeof(stringBuf), &resultBuf);
	if(resultBuf == NULL) {
		parser_errmsg("parameter '%s': ID for user %s could not "
		  "be found", param->name, cstr);
		r = 0;
	} else {
		val->val.datatype = 'N';
		val->val.d.n = resultBuf->pw_uid;
		DBGPRINTF("param '%s': uid %d obtained for user '%s'\n",
		   param->name, (int) resultBuf->pw_uid, cstr);
		r = 1;
	}
	free(cstr);
	return r;
}

/* note: we support all integer formats that es_str2num support,
 * so hex and octal representations are also valid.
 */
static int
doGetInt(struct nvlst *valnode, struct cnfparamdescr *param,
	  struct cnfparamvals *val)
{
	long long n;
	int bSuccess;

	n = es_str2num(valnode->val.d.estr, &bSuccess);
	if(!bSuccess) {
		parser_errmsg("parameter '%s' is not a proper number",
		  param->name);
	}
	val->val.datatype = 'N';
	val->val.d.n = n;
	return bSuccess;
}

static int
doGetNonNegInt(struct nvlst *valnode, struct cnfparamdescr *param,
	  struct cnfparamvals *val)
{
	int bSuccess;

	if((bSuccess = doGetInt(valnode, param, val))) {
		if(val->val.d.n < 0) {
			parser_errmsg("parameter '%s' cannot be less than zero (was %lld)",
			  param->name, val->val.d.n);
			bSuccess = 0;
		}
	}
	return bSuccess;
}

static int
doGetPositiveInt(struct nvlst *valnode, struct cnfparamdescr *param,
	  struct cnfparamvals *val)
{
	int bSuccess;

	if((bSuccess = doGetInt(valnode, param, val))) {
		if(val->val.d.n < 1) {
			parser_errmsg("parameter '%s' cannot be less than one (was %lld)",
			  param->name, val->val.d.n);
			bSuccess = 0;
		}
	}
	return bSuccess;
}

static int
doGetWord(struct nvlst *valnode, struct cnfparamdescr *param,
	  struct cnfparamvals *val)
{
	es_size_t i;
	int r = 1;
	unsigned char *c;

	val->val.datatype = 'S';
	val->val.d.estr = es_newStr(32);
	c = es_getBufAddr(valnode->val.d.estr);
	for(i = 0 ; i < es_strlen(valnode->val.d.estr) && !isspace(c[i]) ; ++i) {
		es_addChar(&val->val.d.estr, c[i]);
	}
	if(i != es_strlen(valnode->val.d.estr)) {
		parser_errmsg("parameter '%s' contains whitespace, which is not "
		  "permitted",
		  param->name);
		r = 0;
	}
	return r;
}

static int
doGetArray(struct nvlst *valnode, struct cnfparamdescr *param,
	  struct cnfparamvals *val)
{
	int r = 1;

	switch(valnode->val.datatype) {
	case 'S':
		/* a constant string is assumed to be a single-element array */
		val->val.datatype = 'A';
		val->val.d.ar = cnfarrayNew(es_strdup(valnode->val.d.estr));
		break;
	case 'A':
		val->val.datatype = 'A';
		val->val.d.ar = cnfarrayDup(valnode->val.d.ar);
		break;
	default:parser_errmsg("parameter '%s' must be an array, but is a "
			"different datatype", param->name);
		r = 0;
		break;
	}
	return r;
}

static int
doGetChar(struct nvlst *valnode, struct cnfparamdescr *param,
	  struct cnfparamvals *val)
{
	int r = 1;
	if(es_strlen(valnode->val.d.estr) != 1) {
		parser_errmsg("parameter '%s' must contain exactly one character "
		  "but contains %d - cannot be processed",
		  param->name, es_strlen(valnode->val.d.estr));
		r = 0;
	}
	val->val.datatype = 'S';
	val->val.d.estr = es_strdup(valnode->val.d.estr);
	return r;
}

/* get a single parameter according to its definition. Helper to
 * nvlstGetParams. returns 1 if success, 0 otherwise
 */
static int
nvlstGetParam(struct nvlst *valnode, struct cnfparamdescr *param,
	       struct cnfparamvals *val)
{
	uchar *cstr;
	int r;

	DBGPRINTF("nvlstGetParam: name '%s', type %d, valnode->bUsed %d\n",
		  param->name, (int) param->type, valnode->bUsed);
	if(valnode->val.datatype != 'S' && param->type != eCmdHdlrArray) {
		parser_errmsg("parameter '%s' is not a string, which is not "
		  "permitted",
		  param->name);
		r = 0;
		goto done;
	}
	valnode->bUsed = 1;
	val->bUsed = 1;
	switch(param->type) {
	case eCmdHdlrQueueType:
		r = doGetQueueType(valnode, param, val);
		break;
	case eCmdHdlrUID:
		r = doGetUID(valnode, param, val);
		break;
	case eCmdHdlrGID:
		r = doGetGID(valnode, param, val);
		break;
	case eCmdHdlrBinary:
		r = doGetBinary(valnode, param, val);
		break;
	case eCmdHdlrFileCreateMode:
		r = doGetFileCreateMode(valnode, param, val);
		break;
	case eCmdHdlrInt:
		r = doGetInt(valnode, param, val);
		break;
	case eCmdHdlrNonNegInt:
		r = doGetNonNegInt(valnode, param, val);
		break;
	case eCmdHdlrPositiveInt:
		r = doGetPositiveInt(valnode, param, val);
		break;
	case eCmdHdlrSize:
		r = doGetSize(valnode, param, val);
		break;
	case eCmdHdlrGetChar:
		r = doGetChar(valnode, param, val);
		break;
	case eCmdHdlrFacility:
		cstr = (uchar*) es_str2cstr(valnode->val.d.estr, NULL);
		val->val.datatype = 'N';
		val->val.d.n = decodeSyslogName(cstr, syslogFacNames);
		free(cstr);
		r = 1;
		break;
	case eCmdHdlrSeverity:
		cstr = (uchar*) es_str2cstr(valnode->val.d.estr, NULL);
		val->val.datatype = 'N';
		val->val.d.n = decodeSyslogName(cstr, syslogPriNames);
		free(cstr);
		r = 1;
		break;
	case eCmdHdlrGetWord:
		r = doGetWord(valnode, param, val);
		break;
	case eCmdHdlrString:
		val->val.datatype = 'S';
		val->val.d.estr = es_strdup(valnode->val.d.estr);
		r = 1;
		break;
	case eCmdHdlrArray:
		r = doGetArray(valnode, param, val);
		break;
	case eCmdHdlrGoneAway:
		parser_errmsg("parameter '%s' is no longer supported",
			      param->name);
		r = 1; /* this *is* valid! */
		break;
	default:
		DBGPRINTF("error: invalid param type\n");
		r = 0;
		break;
	}
done:	return r;
}


/* obtain conf params from an nvlst and emit error messages if
 * necessary. If an already-existing param value is passed, that is
 * used. If NULL is passed instead, a new one is allocated. In that case,
 * it is the caller's duty to free it when no longer needed.
 * NULL is returned on error, otherwise a pointer to the vals array.
 */
struct cnfparamvals*
nvlstGetParams(struct nvlst *lst, struct cnfparamblk *params,
	       struct cnfparamvals *vals)
{
	int i;
	int bValsWasNULL;
	int bInError = 0;
	struct nvlst *valnode;
	struct cnfparamdescr *param;

	if(params->version != CNFPARAMBLK_VERSION) {
		DBGPRINTF("nvlstGetParams: invalid param block version "
			  "%d, expected %d\n",
			  params->version, CNFPARAMBLK_VERSION);
		return NULL;
	}
	
	if(vals == NULL) {
		bValsWasNULL = 1;
		if((vals = calloc(params->nParams,
				  sizeof(struct cnfparamvals))) == NULL)
			return NULL;
	} else {
		bValsWasNULL = 0;
	}

	for(i = 0 ; i < params->nParams ; ++i) {
		param = params->descr + i;
		if((valnode = nvlstFindNameCStr(lst, param->name)) == NULL) {
			if(param->flags & CNFPARAM_REQUIRED) {
				parser_errmsg("parameter '%s' required but not specified - "
				  "fix config", param->name);
				bInError = 1;
			}
			continue;
		}
		if(param->flags & CNFPARAM_DEPRECATED) {
			parser_errmsg("parameter '%s' deprecated but accepted, consider "
			  "removing or replacing it", param->name);
		}
		if(vals[i].bUsed) {
			parser_errmsg("parameter '%s' specified more than once - "
			  "one instance is ignored. Fix config", param->name);
			continue;
		}
		if(!nvlstGetParam(valnode, param, vals + i)) {
			bInError = 1;
		}
	}

	if(bInError) {
		if(bValsWasNULL)
			cnfparamvalsDestruct(vals, params);
		vals = NULL;
	}

	return vals;
}


/* check if at least one cnfparamval is actually set 
 * returns 1 if so, 0 otherwise
 */
int
cnfparamvalsIsSet(struct cnfparamblk *params, struct cnfparamvals *vals)
{
	int i;

	if(vals == NULL)
		return 0;
	if(params->version != CNFPARAMBLK_VERSION) {
		DBGPRINTF("nvlstGetParams: invalid param block version "
			  "%d, expected %d\n",
			  params->version, CNFPARAMBLK_VERSION);
		return 0;
	}
	for(i = 0 ; i < params->nParams ; ++i) {
		if(vals[i].bUsed)
			return 1;
	}
	return 0;
}


void
cnfparamsPrint(const struct cnfparamblk *params, const struct cnfparamvals *vals)
{
	int i;
	char *cstr;

	if(!Debug)
		return;

	for(i = 0 ; i < params->nParams ; ++i) {
		dbgprintf("%s: ", params->descr[i].name);
		if(vals[i].bUsed) {
			// TODO: other types!
			switch(vals[i].val.datatype) {
			case 'S':
				cstr = es_str2cstr(vals[i].val.d.estr, NULL);
				dbgprintf(" '%s'", cstr);
				free(cstr);
				break;
			case 'A':
				cnfarrayPrint(vals[i].val.d.ar, 0);
				break;
			case 'N':
				dbgprintf("%lld", vals[i].val.d.n);
				break;
			default:
				dbgprintf("(unsupported datatype %c)",
					  vals[i].val.datatype);
			}
		} else {
			dbgprintf("(unset)");
		}
		dbgprintf("\n");
	}
}

struct cnfobj*
cnfobjNew(enum cnfobjType objType, struct nvlst *lst)
{
	struct cnfobj *o;

	if((o = malloc(sizeof(struct cnfobj))) != NULL) {
		nvlstChkDupes(lst);
		o->objType = objType;
		o->nvlst = lst;
		o->subobjs = NULL;
		o->script = NULL;
	}

	return o;
}

void
cnfobjDestruct(struct cnfobj *o)
{
	if(o != NULL) {
		nvlstDestruct(o->nvlst);
		objlstDestruct(o->subobjs);
		free(o);
	}
}

void
cnfobjPrint(struct cnfobj *o)
{
	dbgprintf("obj: '%s'\n", cnfobjType2str(o->objType));
	nvlstPrint(o->nvlst);
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


static int64_t
str2num(es_str_t *s, int *bSuccess)
{
	size_t i;
	int neg;
	int64_t num = 0;
	const uchar *const c = es_getBufAddr(s);

	if(c[0] == '-') {
		neg = -1;
		i = -1;
	} else {
		neg = 1;
		i = 0;
	}
	while(i < s->lenStr && isdigit(c[i])) {
		num = num * 10 + c[i] - '0';
		++i;
	}
	num *= neg;
	if(bSuccess != NULL)
		*bSuccess = (i == s->lenStr) ? 1 : 0;
	return num;
}

/* We support decimal integers. Unfortunately, previous versions
 * said they support oct and hex, but that wasn't really the case.
 * Everything based on JSON was just dec-converted. As this was/is
 * the norm, we fix that inconsistency. Luckly, oct and hex support
 * was never documented.
 * rgerhards, 2015-11-12
 */
static long long
var2Number(struct svar *r, int *bSuccess)
{
	long long n = 0;
	if(r->datatype == 'S') {
		n = str2num(r->d.estr, bSuccess);
	} else {
		if(r->datatype == 'J') {
			n = (r->d.json == NULL) ? 0 : json_object_get_int64(r->d.json);
		} else {
			n = r->d.n;
		}
		if(bSuccess != NULL)
			*bSuccess = 1;
	}
	return n;
}

/* ensure that retval is a string
 */
static es_str_t *
var2String(struct svar *__restrict__ const r, int *__restrict__ const bMustFree)
{
	es_str_t *estr;
	const char *cstr;
	rs_size_t lenstr;
	if(r->datatype == 'N') {
		*bMustFree = 1;
		estr = es_newStrFromNumber(r->d.n);
	} else if(r->datatype == 'J') {
		*bMustFree = 1;
		if(r->d.json == NULL) {
			cstr = "",
			lenstr = 0;
		} else {
			cstr = (char*)json_object_get_string(r->d.json);
			lenstr = strlen(cstr);
		}
		estr = es_newStrFromCStr(cstr, lenstr);
	} else {
		*bMustFree = 0;
		estr = r->d.estr;
	}
	return estr;
}

uchar*
var2CString(struct svar *__restrict__ const r, int *__restrict__ const bMustFree)
{
	uchar *cstr;
	es_str_t *estr;
	estr = var2String(r, bMustFree);
	cstr = (uchar*) es_str2cstr(estr, NULL);
	if(*bMustFree)
		es_deleteStr(estr);
	*bMustFree = 1;
	return cstr;
}

/* frees struct svar members, but not the struct itself. This is because
 * it usually is allocated on the stack. Callers why dynamically allocate
 * struct svar need to free the struct themselfes!
 */

int SKIP_NOTHING = 0x0;
int SKIP_STRING = 0x1;

static void
varFreeMembersSelectively(const struct svar *r, const int skipMask)
{
	if(r->datatype == 'J') {
		json_object_put(r->d.json);
	} else if( !(skipMask & SKIP_STRING) && (r->datatype == 'S')) {
		es_deleteStr(r->d.estr);
	}
}

static void
varFreeMembers(const struct svar *r)
{
	varFreeMembersSelectively(r, SKIP_NOTHING);
}


static rsRetVal
doExtractFieldByChar(uchar *str, uchar delim, const int matchnbr, uchar **resstr)
{
	int iCurrFld;
    int allocLen;
	int iLen;
	uchar *pBuf;
	uchar *pFld;
	uchar *pFldEnd;
	DEFiRet;

	/* first, skip to the field in question */
	iCurrFld = 1;
	pFld = str;
	while(*pFld && iCurrFld < matchnbr) {
		/* skip fields until the requested field or end of string is found */
		while(*pFld && (uchar) *pFld != delim)
			++pFld; /* skip to field terminator */
		if(*pFld == delim) {
			++pFld; /* eat it */
			++iCurrFld;
		}
	}
	DBGPRINTF("field() field requested %d, field found %d\n", matchnbr, iCurrFld);
	
	if(iCurrFld == matchnbr) {
		/* field found, now extract it */
		/* first of all, we need to find the end */
		pFldEnd = pFld;
		while(*pFldEnd && *pFldEnd != delim)
			++pFldEnd;
		--pFldEnd; /* we are already at the delimiter - so we need to
			    * step back a little not to copy it as part of the field. */
		/* we got our end pointer, now do the copy */
		iLen = pFldEnd - pFld + 1; /* the +1 is for an actual char, NOT \0! */
		allocLen = iLen + 1;
#		ifdef VALGRIND
		allocLen += (3 - (iLen % 4));
        	/*older versions of valgrind have a problem with strlen inspecting 4-bytes at a time*/
#		endif
		CHKmalloc(pBuf = MALLOC(allocLen));
		/* now copy */
		memcpy(pBuf, pFld, iLen);
		pBuf[iLen] = '\0'; /* terminate it */
		*resstr = pBuf;
	} else {
		ABORT_FINALIZE(RS_RET_FIELD_NOT_FOUND);
	}
finalize_it:
	RETiRet;
}


static rsRetVal
doExtractFieldByStr(uchar *str, char *delim, const rs_size_t lenDelim, const int matchnbr, uchar **resstr)
{
	int iCurrFld;
	int iLen;
	uchar *pBuf;
	uchar *pFld;
	uchar *pFldEnd;
	DEFiRet;

	if (str == NULL || delim == NULL)
		ABORT_FINALIZE(RS_RET_FIELD_NOT_FOUND);

	/* first, skip to the field in question */
	iCurrFld = 1;
	pFld = str;
	while(pFld != NULL && iCurrFld < matchnbr) {
		if((pFld = (uchar*) strstr((char*)pFld, delim)) != NULL) {
			pFld += lenDelim;
			++iCurrFld;
		}
	}
	DBGPRINTF("field() field requested %d, field found %d\n", matchnbr, iCurrFld);
	
	if(iCurrFld == matchnbr) {
		/* field found, now extract it */
		/* first of all, we need to find the end */
		pFldEnd = (uchar*) strstr((char*)pFld, delim);
		if(pFldEnd == NULL) {
			iLen = strlen((char*) pFld);
		} else { /* found delmiter!  Note that pFldEnd *is* already on 
			  * the first delmi char, we don't need that. */
			iLen = pFldEnd - pFld;
		}
		/* we got our end pointer, now do the copy */
		CHKmalloc(pBuf = MALLOC(iLen + 1));
		/* now copy */
		memcpy(pBuf, pFld, iLen);
		pBuf[iLen] = '\0'; /* terminate it */
		*resstr = pBuf;
	} else {
		ABORT_FINALIZE(RS_RET_FIELD_NOT_FOUND);
	}
finalize_it:
	RETiRet;
}

static void
doFunc_re_extract(struct cnffunc *func, struct svar *ret, void* usrptr)
{
	size_t submatchnbr;
	short matchnbr;
	regmatch_t pmatch[50];
	int bMustFree;
	es_str_t *estr = NULL; /* init just to keep compiler happy */
	char *str;
	struct svar r[CNFFUNC_MAX_ARGS];
	int iLenBuf;
	unsigned iOffs;
	short iTry = 0;
	uchar bFound = 0;
	iOffs = 0;
	sbool bHadNoMatch = 0;

	cnfexprEval(func->expr[0], &r[0], usrptr);
	/* search string is already part of the compiled regex, so we don't
	 * need it here!
	 */
	cnfexprEval(func->expr[2], &r[2], usrptr);
	cnfexprEval(func->expr[3], &r[3], usrptr);
	str = (char*) var2CString(&r[0], &bMustFree);
	matchnbr = (short) var2Number(&r[2], NULL);
	submatchnbr = (size_t) var2Number(&r[3], NULL);
	if(submatchnbr >= sizeof(pmatch)/sizeof(regmatch_t)) {
		DBGPRINTF("re_extract() submatch %zd is too large\n", submatchnbr);
		bHadNoMatch = 1;
		goto finalize_it;
	}

	/* first see if we find a match, iterating through the series of
	 * potential matches over the string.
	 */
	while(!bFound) {
		int iREstat;
		iREstat = regexp.regexec(func->funcdata, (char*)(str + iOffs),
					 submatchnbr+1, pmatch, 0);
		DBGPRINTF("re_extract: regexec return is %d\n", iREstat);
		if(iREstat == 0) {
			if(pmatch[0].rm_so == -1) {
				DBGPRINTF("oops ... start offset of successful regexec is -1\n");
				break;
			}
			if(iTry == matchnbr) {
				bFound = 1;
			} else {
				DBGPRINTF("re_extract: regex found at offset %d, new offset %d, tries %d\n",
					  iOffs, (int) (iOffs + pmatch[0].rm_eo), iTry);
				iOffs += pmatch[0].rm_eo;
				++iTry;
			}
		} else {
			break;
		}
	}
	DBGPRINTF("re_extract: regex: end search, found %d\n", bFound);
	if(!bFound) {
		bHadNoMatch = 1;
		goto finalize_it;
	} else {
		/* Match- but did it match the one we wanted? */
		/* we got no match! */
		if(pmatch[submatchnbr].rm_so == -1) {
			bHadNoMatch = 1;
			goto finalize_it;
		}
		/* OK, we have a usable match - we now need to malloc pB */
		iLenBuf = pmatch[submatchnbr].rm_eo - pmatch[submatchnbr].rm_so;
		estr = es_newStrFromBuf(str + iOffs + pmatch[submatchnbr].rm_so,
					iLenBuf);
	}

finalize_it:
	if(bMustFree) free(str);
	varFreeMembers(&r[0]);
	varFreeMembers(&r[2]);
	varFreeMembers(&r[3]);

	if(bHadNoMatch) {
		cnfexprEval(func->expr[4], &r[4], usrptr);
		estr = var2String(&r[4], &bMustFree);
		varFreeMembersSelectively(&r[4], SKIP_STRING);
		/* Note that we do NOT free the string that was returned/created
		 * for r[4]. We pass it to the caller, which in turn frees it.
		 * This saves us doing one unnecessary memory alloc & write.
		 */
	}
	ret->datatype = 'S';
	ret->d.estr = estr;
	return;
}


/* note that we do not need to evaluate any parameters, as the template pointer
 * is set during initialization().
 * TODO: think if we can keep our buffer; but that may not be trival thinking about
 *       multiple threads.
 */
static void
doFunc_exec_template(struct cnffunc *__restrict__ const func,
	struct svar *__restrict__ const ret,
	smsg_t *const pMsg)
{
	rsRetVal localRet;
	actWrkrIParams_t iparam;

	wtiInitIParam(&iparam);
	localRet = tplToString(func->funcdata, pMsg, &iparam, NULL);
	if(localRet == RS_RET_OK) {
		ret->d.estr = es_newStrFromCStr((char*)iparam.param, iparam.lenStr);
	} else {
		ret->d.estr = es_newStrFromCStr("", 0);
	}
	ret->datatype = 'S';
	free(iparam.param);

	return;
}

static es_str_t*
doFuncReplace(struct svar *__restrict__ const operandVal, struct svar *__restrict__ const findVal,
	struct svar *__restrict__ const replaceWithVal) {
    int freeOperand, freeFind, freeReplacement;
    es_str_t *str = var2String(operandVal, &freeOperand);
    es_str_t *findStr = var2String(findVal, &freeFind);
    es_str_t *replaceWithStr = var2String(replaceWithVal, &freeReplacement);
    uchar *find = es_getBufAddr(findStr);
    uchar *replaceWith = es_getBufAddr(replaceWithStr);
    uint lfind = es_strlen(findStr);
    uint lReplaceWith = es_strlen(replaceWithStr);
    uint size = 0;
    uchar* src_buff = es_getBufAddr(str);
    uint i, j;
    for(i = j = 0; i <= es_strlen(str); i++, size++) {
        if (j == lfind) {
            size = size - lfind + lReplaceWith;
            j = 0;
        }
		if (i == es_strlen(str)) break;
		if (src_buff[i] == find[j]) {
			j++;
		} else if (j > 0) {
			i -= (j - 1);
			size -= (j - 1);
			j = 0;
		}
    }
    es_str_t *res = es_newStr(size);
    unsigned char* dest = es_getBufAddr(res);
    uint k, s;
    for(i = j = k = s = 0; i <= es_strlen(str); i++, s++) {
        if (j == lfind) {
            for (k = 0; k < lReplaceWith; k++) {
                dest[s - j + k] = replaceWith[k];
            }
            s = s - j + lReplaceWith;
            j = 0;
        }
		if (i == es_strlen(str)) break;
		if (src_buff[i] == find[j]) {
			j++;
		} else {
			if (j > 0) {
				i -= j;
				s -= j;
				j = 0;
			}
			dest[s] = src_buff[i];
		}
    }
    res->lenStr = size;
    if(freeOperand) es_deleteStr(str);
    if(freeFind) es_deleteStr(findStr);
    if(freeReplacement) es_deleteStr(replaceWithStr);
    return res;
}

static es_str_t*
doFuncWrap(struct svar *__restrict__ const sourceVal, struct svar *__restrict__ const wrapperVal,
	struct svar *__restrict__ const escaperVal) {
    int freeSource, freeWrapper;
    es_str_t *sourceStr;
    if (escaperVal) {
        sourceStr = doFuncReplace(sourceVal, wrapperVal, escaperVal);
        freeSource = 1;
    } else {
        sourceStr = var2String(sourceVal, &freeSource);
    }
    es_str_t *wrapperStr = var2String(wrapperVal, &freeWrapper);
    uchar *src = es_getBufAddr(sourceStr);
    uchar *wrapper = es_getBufAddr(wrapperStr);
    uint lWrapper = es_strlen(wrapperStr);
    uint lSrc = es_strlen(sourceStr);
    uint totalLen = lSrc + 2 * lWrapper;
    es_str_t *res = es_newStr(totalLen);
    uchar* resBuf = es_getBufAddr(res);
    memcpy(resBuf, wrapper, lWrapper);
    memcpy(resBuf + lWrapper, src, lSrc);
    memcpy(resBuf + lSrc + lWrapper, wrapper, lWrapper);
    res->lenStr = totalLen;
    if (freeSource) es_deleteStr(sourceStr);
    if (freeWrapper) es_deleteStr(wrapperStr);
    return res;
}

static long long
doRandomGen(struct svar *__restrict__ const sourceVal) {
	int success = 0;
	long long max = var2Number(sourceVal, &success);
	if (! success) {
		DBGPRINTF("rainerscript: random(max) didn't get a valid 'max' limit, defaulting random-number value to 0");
		return 0;
	}
	if(max == 0) {
		DBGPRINTF("rainerscript: random(max) invalid, 'max' is zero, , defaulting random-number value to 0");
		return 0;
	}
	long int x = randomNumber();
	if (max > MAX_RANDOM_NUMBER) {
		DBGPRINTF("rainerscript: desired random-number range [0 - %lld] "
			"is wider than supported limit of [0 - %d)",
			max, MAX_RANDOM_NUMBER);
	}
	return x % max;
}

/* Perform a function call. This has been moved out of cnfExprEval in order
 * to keep the code small and easier to maintain.
 */
static void
doFuncCall(struct cnffunc *__restrict__ const func, struct svar *__restrict__ const ret,
	   void *__restrict__ const usrptr)
{
	char *envvar;
	int bMustFree;
	es_str_t *estr;
	char *str;
	uchar *resStr;
	int retval;
	struct svar r[CNFFUNC_MAX_ARGS];
	int delim;
	int matchnbr;
	struct funcData_prifilt *pPrifilt;
	rsRetVal localRet;
	lookup_key_t key;
	uint8_t lookup_key_type;
	lookup_t *lookup_table;

	DBGPRINTF("rainerscript: executing function id %d\n", func->fID);
	switch(func->fID) {
	case CNFFUNC_STRLEN:
		if(func->expr[0]->nodetype == 'S') {
			/* if we already have a string, we do not need to
			 * do one more recursive call.
			 */
			ret->d.n = es_strlen(((struct cnfstringval*) func->expr[0])->estr);
		} else {
			cnfexprEval(func->expr[0], &r[0], usrptr);
			estr = var2String(&r[0], &bMustFree);
			ret->d.n = es_strlen(estr);
			if(bMustFree) es_deleteStr(estr);
			varFreeMembers(&r[0]);
		}
		ret->datatype = 'N';
		break;
	case CNFFUNC_REPLACE:
		cnfexprEval(func->expr[0], &r[0], usrptr);
		cnfexprEval(func->expr[1], &r[1], usrptr);
		cnfexprEval(func->expr[2], &r[2], usrptr);
		ret->d.estr = doFuncReplace(&r[0], &r[1], &r[2]);
		ret->datatype = 'S';
		varFreeMembers(&r[0]);
		varFreeMembers(&r[1]);
		varFreeMembers(&r[2]);
		break;
	case CNFFUNC_WRAP:
		cnfexprEval(func->expr[0], &r[0], usrptr);
		cnfexprEval(func->expr[1], &r[1], usrptr);
		if(func->nParams == 3) cnfexprEval(func->expr[2], &r[2], usrptr);
		ret->d.estr = doFuncWrap(&r[0], &r[1], func->nParams > 2 ? &r[2] : NULL);
		ret->datatype = 'S';
		varFreeMembers(&r[0]);
		varFreeMembers(&r[1]);
		if(func->nParams == 3) varFreeMembers(&r[2]);
		break;
	case CNFFUNC_RANDOM:
		cnfexprEval(func->expr[0], &r[0], usrptr);
		ret->d.n = doRandomGen(&r[0]);
		ret->datatype = 'N';
		varFreeMembers(&r[0]);
		break;
	case CNFFUNC_GETENV:
		/* note: the optimizer shall have replaced calls to getenv()
		 * with a constant argument to a single string (once obtained via
		 * getenv()). So we do NOT need to check if there is just a
		 * string following.
		 */
		cnfexprEval(func->expr[0], &r[0], usrptr);
		estr = var2String(&r[0], &bMustFree);
		str = (char*) es_str2cstr(estr, NULL);
		envvar = getenv(str);
		if(envvar == NULL) {
			ret->d.estr = es_newStr(0);
		} else {
			ret->d.estr = es_newStrFromCStr(envvar, strlen(envvar));
		}
		ret->datatype = 'S';
		if(bMustFree) es_deleteStr(estr);
		varFreeMembers(&r[0]);
		free(str);
		break;
	case CNFFUNC_TOLOWER:
		cnfexprEval(func->expr[0], &r[0], usrptr);
		estr = var2String(&r[0], &bMustFree);
		if(!bMustFree) /* let caller handle that M) */
			estr = es_strdup(estr);
		es_tolower(estr);
		ret->datatype = 'S';
		ret->d.estr = estr;
		varFreeMembers(&r[0]);
		break;
	case CNFFUNC_CSTR:
		cnfexprEval(func->expr[0], &r[0], usrptr);
		estr = var2String(&r[0], &bMustFree);
		if(!bMustFree) /* let caller handle that M) */
			estr = es_strdup(estr);
		ret->datatype = 'S';
		ret->d.estr = estr;
		varFreeMembers(&r[0]);
		break;
	case CNFFUNC_CNUM:
		if(func->expr[0]->nodetype == 'N') {
			ret->d.n = ((struct cnfnumval*)func->expr[0])->val;
		} else if(func->expr[0]->nodetype == 'S') {
			ret->d.n = es_str2num(((struct cnfstringval*) func->expr[0])->estr,
					      NULL);
		} else {
			cnfexprEval(func->expr[0], &r[0], usrptr);
			ret->d.n = var2Number(&r[0], NULL);
			varFreeMembers(&r[0]);
		}
		ret->datatype = 'N';
		DBGPRINTF("JSONorString: cnum node type %c result %d\n", func->expr[0]->nodetype, (int) ret->d.n);
		break;
	case CNFFUNC_RE_MATCH:
		cnfexprEval(func->expr[0], &r[0], usrptr);
		str = (char*) var2CString(&r[0], &bMustFree);
		retval = regexp.regexec(func->funcdata, str, 0, NULL, 0);
		if(retval == 0)
			ret->d.n = 1;
		else {
			ret->d.n = 0;
			if(retval != REG_NOMATCH) {
				DBGPRINTF("re_match: regexec returned error %d\n", retval);
			}
		}
		ret->datatype = 'N';
		if(bMustFree) free(str);
		varFreeMembers(&r[0]);
		break;
	case CNFFUNC_RE_EXTRACT:
		doFunc_re_extract(func, ret, usrptr);
		break;
	case CNFFUNC_EXEC_TEMPLATE:
		doFunc_exec_template(func, ret, (smsg_t*) usrptr);
		break;
	case CNFFUNC_FIELD:
		cnfexprEval(func->expr[0], &r[0], usrptr);
		cnfexprEval(func->expr[1], &r[1], usrptr);
		cnfexprEval(func->expr[2], &r[2], usrptr);
		str = (char*) var2CString(&r[0], &bMustFree);
		matchnbr = var2Number(&r[2], NULL);
		if(r[1].datatype == 'S') {
			char *delimstr;
			delimstr = (char*) es_str2cstr(r[1].d.estr, NULL);
			localRet = doExtractFieldByStr((uchar*)str, delimstr, es_strlen(r[1].d.estr),
							matchnbr, &resStr);
			free(delimstr);
		} else {
			delim = var2Number(&r[1], NULL);
			localRet = doExtractFieldByChar((uchar*)str, (char) delim, matchnbr, &resStr);
		}
		if(localRet == RS_RET_OK) {
			ret->d.estr = es_newStrFromCStr((char*)resStr, strlen((char*)resStr));
			free(resStr);
		} else if(localRet == RS_RET_FIELD_NOT_FOUND) {
			ret->d.estr = es_newStrFromCStr("***FIELD NOT FOUND***",
					sizeof("***FIELD NOT FOUND***")-1);
		} else {
			ret->d.estr = es_newStrFromCStr("***ERROR in field() FUNCTION***",
					sizeof("***ERROR in field() FUNCTION***")-1);
		}
		ret->datatype = 'S';
		if(bMustFree) free(str);
		varFreeMembers(&r[0]);
		varFreeMembers(&r[1]);
		varFreeMembers(&r[2]);
		break;
	case CNFFUNC_PRIFILT:
		pPrifilt = (struct funcData_prifilt*) func->funcdata;
		if( (pPrifilt->pmask[((smsg_t*)usrptr)->iFacility] == TABLE_NOPRI) ||
		   ((pPrifilt->pmask[((smsg_t*)usrptr)->iFacility]
			    & (1<<((smsg_t*)usrptr)->iSeverity)) == 0) )
			ret->d.n = 0;
		else
			ret->d.n = 1;
		ret->datatype = 'N';
		break;
	case CNFFUNC_LOOKUP:
		ret->datatype = 'S';
		if(func->funcdata == NULL) {
			ret->d.estr = es_newStrFromCStr("TABLE-NOT-FOUND", sizeof("TABLE-NOT-FOUND")-1);
			break;
		}
		cnfexprEval(func->expr[1], &r[1], usrptr);
		lookup_table = ((lookup_ref_t*)func->funcdata)->self;
		if (lookup_table != NULL) {
			lookup_key_type = lookup_table->key_type;
			bMustFree = 0;
			if (lookup_key_type == LOOKUP_KEY_TYPE_STRING) {
				key.k_str = (uchar*) var2CString(&r[1], &bMustFree);
			} else if (lookup_key_type == LOOKUP_KEY_TYPE_UINT) {
				key.k_uint = var2Number(&r[1], NULL);
			} else {
				DBGPRINTF("program error in %s:%d: lookup_key_type unknown\n",
					__FILE__, __LINE__);
				key.k_uint = 0;
			}
			ret->d.estr = lookupKey((lookup_ref_t*)func->funcdata, key);
			if(bMustFree) free(key.k_str);
		} else {
			ret->d.estr = es_newStrFromCStr("", 1);
		}
		varFreeMembers(&r[1]);
		break;
	case CNFFUNC_DYN_INC:
		ret->datatype = 'N';
		if(func->funcdata == NULL) {
			ret->d.n = -1;
			break;
		}
		cnfexprEval(func->expr[1], &r[1], usrptr);
		str = (char*) var2CString(&r[1], &bMustFree);
		ret->d.n = dynstats_inc(func->funcdata, (uchar*)str);
		if(bMustFree) free(str);
		varFreeMembers(&r[1]);
		break;
	default:
		if(Debug) {
			char *fname = es_str2cstr(func->fname, NULL);
			dbgprintf("rainerscript: invalid function id %u (name '%s')\n",
				  (unsigned) func->fID, fname);
			free(fname);
		}
		ret->datatype = 'N';
		ret->d.n = 0;
	}
}

static void
evalVar(struct cnfvar *__restrict__ const var, void *__restrict__ const usrptr,
	struct svar *__restrict__ const ret)
{
	rs_size_t propLen;
	uchar *pszProp = NULL;
	unsigned short bMustBeFreed = 0;
	rsRetVal localRet;
	struct json_object *json;
	uchar *cstr;

	if(var->prop.id == PROP_CEE        ||
	   var->prop.id == PROP_LOCAL_VAR  ||
	   var->prop.id == PROP_GLOBAL_VAR   ) {
		localRet = msgGetJSONPropJSONorString((smsg_t*)usrptr, &var->prop, &json, &cstr);
		if(json != NULL) {
			ret->datatype = 'J';
			ret->d.json = (localRet == RS_RET_OK) ? json : NULL;
			DBGPRINTF("rainerscript: (json) var %d:%s: '%s'\n",
				var->prop.id, var->prop.name,
			  (ret->d.json == NULL) ? "" : json_object_get_string(ret->d.json));
		} else { /* we have a string */
			ret->datatype = 'S';
			ret->d.estr = (localRet == RS_RET_OK) ?
					  es_newStrFromCStr((char*) cstr, strlen((char*) cstr))
					: es_newStr(1);
			DBGPRINTF("rainerscript: (json/string) var %d: '%s'\n", var->prop.id, cstr);
			free(cstr);
		}
	} else {
		ret->datatype = 'S';
		pszProp = (uchar*) MsgGetProp((smsg_t*)usrptr, NULL, &var->prop, &propLen, &bMustBeFreed, NULL);
		ret->d.estr = es_newStrFromCStr((char*)pszProp, propLen);
		DBGPRINTF("rainerscript: (string) var %d: '%s'\n", var->prop.id, pszProp);
		if(bMustBeFreed)
			free(pszProp);
	}

}

/* perform a string comparision operation against a while array. Semantic is
 * that one one comparison is true, the whole construct is true.
 * TODO: we can obviously optimize this process. One idea is to
 * compile a regex, which should work faster than serial comparison.
 * Note: compiling a regex does NOT work at all. I experimented with that
 * and it was generally 5 to 10 times SLOWER than what we do here...
 */
static int
evalStrArrayCmp(es_str_t *const estr_l,
		const struct cnfarray *__restrict__ const ar,
		const int cmpop)
{
	int i;
	int r = 0;
	es_str_t **res;
	if(cmpop == CMP_EQ) {
		res = bsearch(&estr_l, ar->arr, ar->nmemb, sizeof(es_str_t*), qs_arrcmp);
		r = res != NULL;
	} else if(cmpop == CMP_NE) {
		res = bsearch(&estr_l, ar->arr, ar->nmemb, sizeof(es_str_t*), qs_arrcmp);
		r = res == NULL;
	} else {
		for(i = 0 ; (r == 0) && (i < ar->nmemb) ; ++i) {
			switch(cmpop) {
			case CMP_STARTSWITH:
				r = es_strncmp(estr_l, ar->arr[i], es_strlen(ar->arr[i])) == 0;
				break;
			case CMP_STARTSWITHI:
				r = es_strncasecmp(estr_l, ar->arr[i], es_strlen(ar->arr[i])) == 0;
				break;
			case CMP_CONTAINS:
				r = es_strContains(estr_l, ar->arr[i]) != -1;
				break;
			case CMP_CONTAINSI:
				r = es_strCaseContains(estr_l, ar->arr[i]) != -1;
				break;
			}
		}
	}
	return r;
}

#define FREE_BOTH_RET \
		varFreeMembers(&r); \
		varFreeMembers(&l)

#define COMP_NUM_BINOP(x) \
	cnfexprEval(expr->l, &l, usrptr); \
	cnfexprEval(expr->r, &r, usrptr); \
	ret->datatype = 'N'; \
	ret->d.n = var2Number(&l, &convok_l) x var2Number(&r, &convok_r); \
	FREE_BOTH_RET

#define COMP_NUM_BINOP_DIV(x) \
	cnfexprEval(expr->l, &l, usrptr); \
	cnfexprEval(expr->r, &r, usrptr); \
	ret->datatype = 'N'; \
	if((ret->d.n = var2Number(&r, &convok_r)) == 0) { \
		/* division by zero */ \
	} else { \
		ret->d.n = var2Number(&l, &convok_l) x ret->d.n; \
	} \
	FREE_BOTH_RET

/* NOTE: array as right-hand argument MUST be handled by user */
#define PREP_TWO_STRINGS \
		cnfexprEval(expr->l, &l, usrptr); \
		estr_l = var2String(&l, &bMustFree2); \
		if(expr->r->nodetype == 'S') { \
			estr_r = ((struct cnfstringval*)expr->r)->estr;\
			bMustFree = 0; \
		} else if(expr->r->nodetype != 'A') { \
			cnfexprEval(expr->r, &r, usrptr); \
			estr_r = var2String(&r, &bMustFree); \
		} else { \
			/* Note: this is not really necessary, but if we do not */ \
			/* do it, we get a very irritating compiler warning... */ \
			estr_r = NULL; \
		}

#define FREE_TWO_STRINGS \
		if(bMustFree) es_deleteStr(estr_r);  \
		if(expr->r->nodetype != 'S' && expr->r->nodetype != 'A') varFreeMembers(&r); \
		if(bMustFree2) es_deleteStr(estr_l);  \
		varFreeMembers(&l)

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
cnfexprEval(const struct cnfexpr *__restrict__ const expr, struct svar *__restrict__ const ret,
	    void *__restrict__ const usrptr)
{
	struct svar r, l; /* memory for subexpression results */
	es_str_t *__restrict__ estr_r, *__restrict__ estr_l;
	int convok_r, convok_l;
	int bMustFree, bMustFree2;
	long long n_r, n_l;

	DBGPRINTF("eval expr %p, type '%s'\n", expr, tokenToString(expr->nodetype));
	switch(expr->nodetype) {
	/* note: comparison operations are extremely similar. The code can be copyied, only
	 * places flagged with "CMP" need to be changed.
	 */
	case CMP_EQ:
		/* this is optimized in regard to right param as a PoC for all compOps
		 * So this is a NOT yet the copy template!
		 */
		cnfexprEval(expr->l, &l, usrptr);
		ret->datatype = 'N';
		if(l.datatype == 'S') {
			if(expr->r->nodetype == 'S') {
				ret->d.n = !es_strcmp(l.d.estr, ((struct cnfstringval*)expr->r)->estr); /*CMP*/
			} else if(expr->r->nodetype == 'A') {
				ret->d.n = evalStrArrayCmp(l.d.estr,  (struct cnfarray*) expr->r, CMP_EQ);
			} else {
				cnfexprEval(expr->r, &r, usrptr);
				if(r.datatype == 'S') {
					ret->d.n = !es_strcmp(l.d.estr, r.d.estr); /*CMP*/
				} else {
					n_l = var2Number(&l, &convok_l);
					if(convok_l) {
						ret->d.n = (n_l == r.d.n); /*CMP*/
					} else {
						estr_r = var2String(&r, &bMustFree);
						ret->d.n = !es_strcmp(l.d.estr, estr_r); /*CMP*/
						if(bMustFree) es_deleteStr(estr_r);
					}
				}
				varFreeMembers(&r);
			}
		} else if(l.datatype == 'J') {
			estr_l = var2String(&l, &bMustFree);
			if(expr->r->nodetype == 'S') {
				ret->d.n = !es_strcmp(estr_l, ((struct cnfstringval*)expr->r)->estr); /*CMP*/
			} else if(expr->r->nodetype == 'A') {
				ret->d.n = evalStrArrayCmp(estr_l,  (struct cnfarray*) expr->r, CMP_EQ);
			} else {
				cnfexprEval(expr->r, &r, usrptr);
				if(r.datatype == 'S') {
					ret->d.n = !es_strcmp(estr_l, r.d.estr); /*CMP*/
				} else {
					n_l = var2Number(&l, &convok_l);
					if(convok_l) {
						ret->d.n = (n_l == r.d.n); /*CMP*/
					} else {
						estr_r = var2String(&r, &bMustFree2);
						ret->d.n = !es_strcmp(estr_l, estr_r); /*CMP*/
						if(bMustFree2) es_deleteStr(estr_r);
					}
				}
				varFreeMembers(&r);
			}
			if(bMustFree) es_deleteStr(estr_l);
		} else {
			cnfexprEval(expr->r, &r, usrptr);
			if(r.datatype == 'S') {
				n_r = var2Number(&r, &convok_r);
				if(convok_r) {
					ret->d.n = (l.d.n == n_r); /*CMP*/
				} else {
					estr_l = var2String(&l, &bMustFree);
					ret->d.n = !es_strcmp(r.d.estr, estr_l); /*CMP*/
					if(bMustFree) es_deleteStr(estr_l);
				}
			} else {
				ret->d.n = (l.d.n == r.d.n); /*CMP*/
			}
			varFreeMembers(&r);
		}
		varFreeMembers(&l);
		break;
	case CMP_NE:
		cnfexprEval(expr->l, &l, usrptr);
		cnfexprEval(expr->r, &r, usrptr);
		ret->datatype = 'N';
		if(l.datatype == 'S') {
			if(expr->r->nodetype == 'S') {
				ret->d.n = es_strcmp(l.d.estr, ((struct cnfstringval*)expr->r)->estr); /*CMP*/
			} else if(expr->r->nodetype == 'A') {
				ret->d.n = evalStrArrayCmp(l.d.estr,  (struct cnfarray*) expr->r, CMP_NE);
			} else {
				if(r.datatype == 'S') {
					ret->d.n = es_strcmp(l.d.estr, r.d.estr); /*CMP*/
				} else {
					n_l = var2Number(&l, &convok_l);
					if(convok_l) {
						ret->d.n = (n_l != r.d.n); /*CMP*/
					} else {
						estr_r = var2String(&r, &bMustFree);
						ret->d.n = es_strcmp(l.d.estr, estr_r); /*CMP*/
						if(bMustFree) es_deleteStr(estr_r);
					}
				}
			}
		} else if(l.datatype == 'J') {
			estr_l = var2String(&l, &bMustFree);
			if(r.datatype == 'S') {
				ret->d.n = es_strcmp(estr_l, r.d.estr); /*CMP*/
			} else {
				n_l = var2Number(&l, &convok_l);
				if(convok_l) {
					ret->d.n = (n_l != r.d.n); /*CMP*/
				} else {
					estr_r = var2String(&r, &bMustFree2);
					ret->d.n = es_strcmp(estr_l, estr_r); /*CMP*/
					if(bMustFree2) es_deleteStr(estr_r);
				}
			}
			if(bMustFree) es_deleteStr(estr_l);
		} else {
			if(r.datatype == 'S') {
				n_r = var2Number(&r, &convok_r);
				if(convok_r) {
					ret->d.n = (l.d.n != n_r); /*CMP*/
				} else {
					estr_l = var2String(&l, &bMustFree);
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
				n_l = var2Number(&l, &convok_l);
				if(convok_l) {
					ret->d.n = (n_l <= r.d.n); /*CMP*/
				} else {
					estr_r = var2String(&r, &bMustFree);
					ret->d.n = es_strcmp(l.d.estr, estr_r) <= 0; /*CMP*/
					if(bMustFree) es_deleteStr(estr_r);
				}
			}
		} else if(l.datatype == 'J') {
			estr_l = var2String(&l, &bMustFree);
			if(r.datatype == 'S') {
				ret->d.n = es_strcmp(estr_l, r.d.estr) <= 0; /*CMP*/
			} else {
				n_l = var2Number(&l, &convok_l);
				if(convok_l) {
					ret->d.n = (n_l <= r.d.n); /*CMP*/
				} else {
					estr_r = var2String(&r, &bMustFree2);
					ret->d.n = es_strcmp(estr_l, estr_r) <= 0; /*CMP*/
					if(bMustFree2) es_deleteStr(estr_r);
				}
			}
			if(bMustFree) es_deleteStr(estr_l);
		} else {
			if(r.datatype == 'S') {
				n_r = var2Number(&r, &convok_r);
				if(convok_r) {
					ret->d.n = (l.d.n <= n_r); /*CMP*/
				} else {
					estr_l = var2String(&l, &bMustFree);
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
				n_l = var2Number(&l, &convok_l);
				if(convok_l) {
					ret->d.n = (n_l >= r.d.n); /*CMP*/
				} else {
					estr_r = var2String(&r, &bMustFree);
					ret->d.n = es_strcmp(l.d.estr, estr_r) >= 0; /*CMP*/
					if(bMustFree) es_deleteStr(estr_r);
				}
			}
		} else if(l.datatype == 'J') {
			estr_l = var2String(&l, &bMustFree);
			if(r.datatype == 'S') {
				ret->d.n = es_strcmp(estr_l, r.d.estr) >= 0; /*CMP*/
			} else {
				n_l = var2Number(&l, &convok_l);
				if(convok_l) {
					ret->d.n = (n_l >= r.d.n); /*CMP*/
				} else {
					estr_r = var2String(&r, &bMustFree2);
					ret->d.n = es_strcmp(estr_l, estr_r) >= 0; /*CMP*/
					if(bMustFree2) es_deleteStr(estr_r);
				}
			}
			if(bMustFree) es_deleteStr(estr_l);
		} else {
			if(r.datatype == 'S') {
				n_r = var2Number(&r, &convok_r);
				if(convok_r) {
					ret->d.n = (l.d.n >= n_r); /*CMP*/
				} else {
					estr_l = var2String(&l, &bMustFree);
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
				n_l = var2Number(&l, &convok_l);
				if(convok_l) {
					ret->d.n = (n_l < r.d.n); /*CMP*/
				} else {
					estr_r = var2String(&r, &bMustFree);
					ret->d.n = es_strcmp(l.d.estr, estr_r) < 0; /*CMP*/
					if(bMustFree) es_deleteStr(estr_r);
				}
			}
		} else if(l.datatype == 'J') {
			estr_l = var2String(&l, &bMustFree);
			if(r.datatype == 'S') {
				ret->d.n = es_strcmp(estr_l, r.d.estr) < 0; /*CMP*/
			} else {
				n_l = var2Number(&l, &convok_l);
				if(convok_l) {
					ret->d.n = (n_l < r.d.n); /*CMP*/
				} else {
					estr_r = var2String(&r, &bMustFree2);
					ret->d.n = es_strcmp(estr_l, estr_r) < 0; /*CMP*/
					if(bMustFree2) es_deleteStr(estr_r);
				}
			}
			if(bMustFree) es_deleteStr(estr_l);
		} else {
			if(r.datatype == 'S') {
				n_r = var2Number(&r, &convok_r);
				if(convok_r) {
					ret->d.n = (l.d.n < n_r); /*CMP*/
				} else {
					estr_l = var2String(&l, &bMustFree);
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
				n_l = var2Number(&l, &convok_l);
				if(convok_l) {
					ret->d.n = (n_l > r.d.n); /*CMP*/
				} else {
					estr_r = var2String(&r, &bMustFree);
					ret->d.n = es_strcmp(l.d.estr, estr_r) > 0; /*CMP*/
					if(bMustFree) es_deleteStr(estr_r);
				}
			}
		} else if(l.datatype == 'J') {
			estr_l = var2String(&l, &bMustFree);
			if(r.datatype == 'S') {
				ret->d.n = es_strcmp(estr_l, r.d.estr) > 0; /*CMP*/
			} else {
				n_l = var2Number(&l, &convok_l);
				if(convok_l) {
					ret->d.n = (n_l > r.d.n); /*CMP*/
				} else {
					estr_r = var2String(&r, &bMustFree2);
					ret->d.n = es_strcmp(estr_l, estr_r) > 0; /*CMP*/
					if(bMustFree2) es_deleteStr(estr_r);
				}
			}
			if(bMustFree) es_deleteStr(estr_l);
		} else {
			if(r.datatype == 'S') {
				n_r = var2Number(&r, &convok_r);
				if(convok_r) {
					ret->d.n = (l.d.n > n_r); /*CMP*/
				} else {
					estr_l = var2String(&l, &bMustFree);
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
		if(expr->r->nodetype == 'A') {
			ret->d.n = evalStrArrayCmp(estr_l,  (struct cnfarray*) expr->r, CMP_STARTSWITH);
			bMustFree = 0;
		} else {
			ret->d.n = es_strncmp(estr_l, estr_r, estr_r->lenStr) == 0;
		}
		FREE_TWO_STRINGS;
		break;
	case CMP_STARTSWITHI:
		PREP_TWO_STRINGS;
		ret->datatype = 'N';
		if(expr->r->nodetype == 'A') {
			ret->d.n = evalStrArrayCmp(estr_l,  (struct cnfarray*) expr->r, CMP_STARTSWITHI);
			bMustFree = 0;
		} else {
			ret->d.n = es_strncasecmp(estr_l, estr_r, estr_r->lenStr) == 0;
		}
		FREE_TWO_STRINGS;
		break;
	case CMP_CONTAINS:
		PREP_TWO_STRINGS;
		ret->datatype = 'N';
		if(expr->r->nodetype == 'A') {
			ret->d.n = evalStrArrayCmp(estr_l,  (struct cnfarray*) expr->r, CMP_CONTAINS);
			bMustFree = 0;
		} else {
			ret->d.n = es_strContains(estr_l, estr_r) != -1;
		}
		FREE_TWO_STRINGS;
		break;
	case CMP_CONTAINSI:
		PREP_TWO_STRINGS;
		ret->datatype = 'N';
		if(expr->r->nodetype == 'A') {
			ret->d.n = evalStrArrayCmp(estr_l,  (struct cnfarray*) expr->r, CMP_CONTAINSI);
			bMustFree = 0;
		} else {
			ret->d.n = es_strCaseContains(estr_l, estr_r) != -1;
		}
		FREE_TWO_STRINGS;
		break;
	case OR:
		cnfexprEval(expr->l, &l, usrptr);
		ret->datatype = 'N';
		if(var2Number(&l, &convok_l)) {
			ret->d.n = 1ll;
		} else {
			cnfexprEval(expr->r, &r, usrptr);
			if(var2Number(&r, &convok_r))
				ret->d.n = 1ll;
			else 
				ret->d.n = 0ll;
			varFreeMembers(&r);
		}
		varFreeMembers(&l);
		break;
	case AND:
		cnfexprEval(expr->l, &l, usrptr);
		ret->datatype = 'N';
		if(var2Number(&l, &convok_l)) {
			cnfexprEval(expr->r, &r, usrptr);
			if(var2Number(&r, &convok_r))
				ret->d.n = 1ll;
			else 
				ret->d.n = 0ll;
			varFreeMembers(&r);
		} else {
			ret->d.n = 0ll;
		}
		varFreeMembers(&l);
		break;
	case NOT:
		cnfexprEval(expr->r, &r, usrptr);
		ret->datatype = 'N';
		ret->d.n = !var2Number(&r, &convok_r);
		varFreeMembers(&r);
		break;
	case 'N':
		ret->datatype = 'N';
		ret->d.n = ((struct cnfnumval*)expr)->val;
		break;
	case 'S':
		ret->datatype = 'S';
		ret->d.estr = es_strdup(((struct cnfstringval*)expr)->estr);
		break;
	case 'A':
		/* if an array is used with "normal" operations, it just evaluates
		 * to its first element.
		 */
		ret->datatype = 'S';
		ret->d.estr = es_strdup(((struct cnfarray*)expr)->arr[0]);
		break;
	case 'V':
		evalVar((struct cnfvar*)expr, usrptr, ret);
		break;
	case '&':
		/* TODO: think about optimization, should be possible ;) */
		PREP_TWO_STRINGS;
		if(expr->r->nodetype == 'A') {
			estr_r = ((struct cnfarray*)expr->r)->arr[0];
			bMustFree = 0;
		}
		ret->datatype = 'S';
		ret->d.estr = es_strdup(estr_l);
		es_addStr(&ret->d.estr, estr_r);
		FREE_TWO_STRINGS;
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
		COMP_NUM_BINOP_DIV(/);
		break;
	case '%':
		COMP_NUM_BINOP_DIV(%);
		break;
	case 'M':
		cnfexprEval(expr->r, &r, usrptr);
		ret->datatype = 'N';
		ret->d.n = -var2Number(&r, &convok_r);
		varFreeMembers(&r);
		break;
	case 'F':
		doFuncCall((struct cnffunc*) expr, ret, usrptr);
		break;
	default:
		ret->datatype = 'N';
		ret->d.n = 0ll;
		DBGPRINTF("eval error: unknown nodetype %u['%c']\n",
			(unsigned) expr->nodetype, (char) expr->nodetype);
		assert(0); /* abort on debug builds, this must not happen! */
		break;
	}
	DBGPRINTF("eval expr %p, return datatype '%c':%d\n", expr, ret->datatype,
		(ret->datatype == 'N') ? (int)ret->d.n: 0);
}

//---------------------------------------------------------

void
cnfarrayContentDestruct(struct cnfarray *ar)
{
	unsigned short i;
	for(i = 0 ; i < ar->nmemb ; ++i) {
		es_deleteStr(ar->arr[i]);
	}
	free(ar->arr);
}

static void
cnffuncDestruct(struct cnffunc *func)
{
	unsigned short i;

	for(i = 0 ; i < func->nParams ; ++i) {
		cnfexprDestruct(func->expr[i]);
	}
	/* some functions require special destruction */
	switch(func->fID) {
		case CNFFUNC_RE_MATCH:
		case CNFFUNC_RE_EXTRACT:
			if(func->funcdata != NULL)
				regexp.regfree(func->funcdata);
			break;
		default:break;
	}
	if(func->destructable_funcdata) {
		free(func->funcdata);
	}
	free(func->fname);
}

/* Destruct an expression and all sub-expressions contained in it.
 */
void
cnfexprDestruct(struct cnfexpr *__restrict__ const expr)
{

	if(expr == NULL) {
		/* this is valid and can happen during optimizer run! */
		DBGPRINTF("cnfexprDestruct got NULL ptr - valid, so doing nothing\n");
		return;
	}

	DBGPRINTF("cnfexprDestruct expr %p, type '%s'\n", expr, tokenToString(expr->nodetype));
	switch(expr->nodetype) {
	case CMP_NE:
	case CMP_EQ:
	case CMP_LE:
	case CMP_GE:
	case CMP_LT:
	case CMP_GT:
	case CMP_STARTSWITH:
	case CMP_STARTSWITHI:
	case CMP_CONTAINS:
	case CMP_CONTAINSI:
	case OR:
	case AND:
	case '&':
	case '+':
	case '-':
	case '*':
	case '/':
	case '%': /* binary */
		cnfexprDestruct(expr->l);
		cnfexprDestruct(expr->r);
		break;
	case NOT: 
	case 'M': /* unary */
		cnfexprDestruct(expr->r);
		break;
	case 'N':
		break;
	case 'S':
		es_deleteStr(((struct cnfstringval*)expr)->estr);
		break;
	case 'V':
		free(((struct cnfvar*)expr)->name);
		msgPropDescrDestruct(&(((struct cnfvar*)expr)->prop));
		break;
	case 'F':
		cnffuncDestruct((struct cnffunc*)expr);
		break;
	case 'A':
		cnfarrayContentDestruct((struct cnfarray*)expr);
		break;
	default:break;
	}
	free(expr);
}

//---- END


/* Evaluate an expression as a bool. This is added because expressions are
 * mostly used inside filters, and so this function is quite common and
 * important.
 */
int
cnfexprEvalBool(struct cnfexpr *__restrict__ const expr, void *__restrict__ const usrptr)
{
	int convok;
	struct svar ret;
	cnfexprEval(expr, &ret, usrptr);
	int retVal = var2Number(&ret, &convok);
	varFreeMembers(&ret);
	return retVal;
}

struct json_object*
cnfexprEvalCollection(struct cnfexpr *__restrict__ const expr, void *__restrict__ const usrptr)
{
	struct svar ret;
	void *retptr;
	cnfexprEval(expr, &ret, usrptr);
	if(ret.datatype == 'J') {
		retptr = ret.d.json; /*caller is supposed to free the returned json-object*/
	} else {
		retptr = NULL;
		varFreeMembers(&ret); /* we must free the element */
	}
	return retptr;
}

inline static void
doIndent(int indent)
{
	int i;
	for(i = 0 ; i < indent ; ++i)
		dbgprintf("  ");
}

static void
pmaskPrint(uchar *pmask, int indent)
{
	int i;
	doIndent(indent);
	dbgprintf("pmask: ");
	for (i = 0; i <= LOG_NFACILITIES; i++)
		if (pmask[i] == TABLE_NOPRI)
			dbgprintf(" X ");
		else
			dbgprintf("%2X ", pmask[i]);
	dbgprintf("\n");
}

static void
cnfarrayPrint(struct cnfarray *ar, int indent)
{
	int i;
	doIndent(indent); dbgprintf("ARRAY:\n");
	for(i = 0 ; i < ar->nmemb ; ++i) {
		doIndent(indent+1);
		cstrPrint("string '", ar->arr[i]);
		dbgprintf("'\n");
	}
}

void
cnfexprPrint(struct cnfexpr *expr, int indent)
{
	struct cnffunc *func;
	int i;

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
	case 'A':
		cnfarrayPrint((struct cnfarray*)expr, indent);
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
		if(func->fID == CNFFUNC_PRIFILT) {
			struct funcData_prifilt *pD;
			pD = (struct funcData_prifilt*) func->funcdata;
			pmaskPrint(pD->pmask, indent+1);
		}
		for(i = 0 ; i < func->nParams ; ++i) {
			cnfexprPrint(func->expr[i], indent+1);
		}
		break;
	case '&':
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
		dbgprintf("error: unknown nodetype %u['%c']\n",
			(unsigned) expr->nodetype, (char) expr->nodetype);
		assert(0); /* abort on debug builds, this must not happen! */
		break;
	}
}
/* print only the given stmt 
 * if "subtree" equals 1, the full statement subtree is printed, else
 * really only the statement.
 */
void
cnfstmtPrintOnly(struct cnfstmt *stmt, int indent, sbool subtree)
{
	char *cstr;
	switch(stmt->nodetype) {
	case S_NOP:
		doIndent(indent); dbgprintf("NOP\n");
		break;
	case S_STOP:
		doIndent(indent); dbgprintf("STOP\n");
		break;
	case S_CALL:
		cstr = es_str2cstr(stmt->d.s_call.name, NULL);
		doIndent(indent); dbgprintf("CALL [%s, queue:%d]\n", cstr,
			stmt->d.s_call.ruleset == NULL ? 0 : 1);
		free(cstr);
		break;
	case S_CALL_INDIRECT:
		doIndent(indent); dbgprintf("CALL_INDIRECT\n");
		cnfexprPrint(stmt->d.s_call_ind.expr, indent+1);
		break;
	case S_ACT:
		doIndent(indent); dbgprintf("ACTION %d [%s:%s]\n", stmt->d.act->iActionNbr,
			modGetName(stmt->d.act->pMod), stmt->printable);
		break;
	case S_IF:
		doIndent(indent); dbgprintf("IF\n");
		cnfexprPrint(stmt->d.s_if.expr, indent+1);
		if(subtree) {
			doIndent(indent); dbgprintf("THEN\n");
			cnfstmtPrint(stmt->d.s_if.t_then, indent+1);
			if(stmt->d.s_if.t_else != NULL) {
				doIndent(indent); dbgprintf("ELSE\n");
				cnfstmtPrint(stmt->d.s_if.t_else, indent+1);
			}
			doIndent(indent); dbgprintf("END IF\n");
		}
		break;
	case S_FOREACH:
		doIndent(indent); dbgprintf("FOREACH %s IN\n",
									stmt->d.s_foreach.iter->var);
		cnfexprPrint(stmt->d.s_foreach.iter->collection, indent+1);
		if(subtree) {
			doIndent(indent); dbgprintf("DO\n");
			cnfstmtPrint(stmt->d.s_foreach.body, indent+1);
			doIndent(indent); dbgprintf("END FOREACH\n");
		}
		break;
	case S_SET:
		doIndent(indent); dbgprintf("SET %s =\n",
				  stmt->d.s_set.varname);
		cnfexprPrint(stmt->d.s_set.expr, indent+1);
		doIndent(indent); dbgprintf("END SET\n");
		break;
	case S_UNSET:
		doIndent(indent); dbgprintf("UNSET %s\n",
				  stmt->d.s_unset.varname);
		break;
    case S_RELOAD_LOOKUP_TABLE:
		doIndent(indent); dbgprintf("RELOAD_LOOKUP_TABLE table(%s) (stub with '%s' on error)",
									stmt->d.s_reload_lookup_table.table_name,
									stmt->d.s_reload_lookup_table.stub_value);
		break;
	case S_PRIFILT:
		doIndent(indent); dbgprintf("PRIFILT '%s'\n", stmt->printable);
		pmaskPrint(stmt->d.s_prifilt.pmask, indent);
		if(subtree) {
			cnfstmtPrint(stmt->d.s_prifilt.t_then, indent+1);
			if(stmt->d.s_prifilt.t_else != NULL) {
				doIndent(indent); dbgprintf("ELSE\n");
				cnfstmtPrint(stmt->d.s_prifilt.t_else, indent+1);
			}
			doIndent(indent); dbgprintf("END PRIFILT\n");
		}
		break;
	case S_PROPFILT:
		doIndent(indent); dbgprintf("PROPFILT\n");
		doIndent(indent); dbgprintf("\tProperty.: '%s'\n",
			propIDToName(stmt->d.s_propfilt.prop.id));
		if(stmt->d.s_propfilt.prop.id == PROP_CEE ||
		   stmt->d.s_propfilt.prop.id == PROP_LOCAL_VAR ||
		   stmt->d.s_propfilt.prop.id == PROP_GLOBAL_VAR) {
			doIndent(indent);
			dbgprintf("\tCEE-Prop.: '%s'\n", stmt->d.s_propfilt.prop.name);
		}
		doIndent(indent); dbgprintf("\tOperation: ");
		if(stmt->d.s_propfilt.isNegated)
			dbgprintf("NOT ");
		dbgprintf("'%s'\n", getFIOPName(stmt->d.s_propfilt.operation));
		if(stmt->d.s_propfilt.pCSCompValue != NULL) {
			doIndent(indent); dbgprintf("\tValue....: '%s'\n",
			       rsCStrGetSzStrNoNULL(stmt->d.s_propfilt.pCSCompValue));
		}
		if(subtree) {
			doIndent(indent); dbgprintf("THEN\n");
			cnfstmtPrint(stmt->d.s_propfilt.t_then, indent+1);
			doIndent(indent); dbgprintf("END PROPFILT\n");
		}
		break;
	default:
		dbgprintf("error: unknown stmt type %u\n",
			(unsigned) stmt->nodetype);
		break;
	}
}
void
cnfstmtPrint(struct cnfstmt *root, int indent)
{
	struct cnfstmt *stmt;
	//dbgprintf("stmt %p, indent %d, type '%c'\n", expr, indent, expr->nodetype);
	for(stmt = root ; stmt != NULL ; stmt = stmt->next) {
		cnfstmtPrintOnly(stmt, indent, 1);
	}
}

struct cnfnumval*
cnfnumvalNew(const long long val)
{
	struct cnfnumval *numval;
	if((numval = malloc(sizeof(struct cnfnumval))) != NULL) {
		numval->nodetype = 'N';
		numval->val = val;
	}
	return numval;
}

struct cnfstringval*
cnfstringvalNew(es_str_t *const estr)
{
	struct cnfstringval *strval;
	if((strval = malloc(sizeof(struct cnfstringval))) != NULL) {
		strval->nodetype = 'S';
		strval->estr = estr;
	}
	return strval;
}

/* creates array AND adds first element to it */
struct cnfarray*
cnfarrayNew(es_str_t *val)
{
	struct cnfarray *ar;
	if((ar = malloc(sizeof(struct cnfarray))) != NULL) {
		ar->nodetype = 'A';
		ar->nmemb = 1;
		if((ar->arr = malloc(sizeof(es_str_t*))) == NULL) {
			free(ar);
			ar = NULL;
			goto done;
		}
		ar->arr[0] = val;
	}
done:	return ar;
}

struct cnfarray*
cnfarrayAdd(struct cnfarray *__restrict__ const ar, es_str_t *__restrict__ val)
{
	es_str_t **newptr;
	if((newptr = realloc(ar->arr, (ar->nmemb+1)*sizeof(es_str_t*))) == NULL) {
		DBGPRINTF("cnfarrayAdd: realloc failed, item ignored, ar->arr=%p\n", ar->arr);
		goto done;
	} else {
		ar->arr = newptr;
		ar->arr[ar->nmemb] = val;
		ar->nmemb++;
	}
done:	return ar;
}

/* duplicate an array (deep copy) */
struct cnfarray*
cnfarrayDup(struct cnfarray *old)
{
	int i;
	struct cnfarray *ar;
	ar = cnfarrayNew(es_strdup(old->arr[0]));
	for(i = 1 ; i < old->nmemb ; ++i) {
		cnfarrayAdd(ar, es_strdup(old->arr[i]));
	}
	return ar;
}

struct cnfvar*
cnfvarNew(char *name)
{
	struct cnfvar *var;
	if((var = malloc(sizeof(struct cnfvar))) != NULL) {
		var->nodetype = 'V';
		var->name = name;
		msgPropDescrFill(&var->prop, (uchar*)var->name, strlen(var->name));
	}
	return var;
}

struct cnfstmt *
cnfstmtNew(unsigned s_type)
{
	struct cnfstmt* cnfstmt;
	if((cnfstmt = malloc(sizeof(struct cnfstmt))) != NULL) {
		cnfstmt->nodetype = s_type;
		cnfstmt->printable = NULL;
		cnfstmt->next = NULL;
	}
	return cnfstmt;
}

void cnfstmtDestructLst(struct cnfstmt *root);

static void cnfIteratorDestruct(struct cnfitr *itr);

/* delete a single stmt */
static void
cnfstmtDestruct(struct cnfstmt *stmt)
{
	switch(stmt->nodetype) {
	case S_NOP:
	case S_STOP:
		break;
	case S_CALL:
		es_deleteStr(stmt->d.s_call.name);
		break;
	case S_CALL_INDIRECT:
		cnfexprDestruct(stmt->d.s_call_ind.expr);
		break;
	case S_ACT:
		actionDestruct(stmt->d.act);
		break;
	case S_IF:
		cnfexprDestruct(stmt->d.s_if.expr);
		if(stmt->d.s_if.t_then != NULL) {
			cnfstmtDestructLst(stmt->d.s_if.t_then);
		}
		if(stmt->d.s_if.t_else != NULL) {
			cnfstmtDestructLst(stmt->d.s_if.t_else);
		}
		break;
	case S_FOREACH:
		cnfIteratorDestruct(stmt->d.s_foreach.iter);
		cnfstmtDestructLst(stmt->d.s_foreach.body);
		break;
	case S_SET:
		free(stmt->d.s_set.varname);
		cnfexprDestruct(stmt->d.s_set.expr);
		break;
	case S_UNSET:
		free(stmt->d.s_set.varname);
		break;
	case S_PRIFILT:
		cnfstmtDestructLst(stmt->d.s_prifilt.t_then);
		cnfstmtDestructLst(stmt->d.s_prifilt.t_else);
		break;
	case S_PROPFILT:
		msgPropDescrDestruct(&stmt->d.s_propfilt.prop);
		if(stmt->d.s_propfilt.regex_cache != NULL)
			rsCStrRegexDestruct(&stmt->d.s_propfilt.regex_cache);
		if(stmt->d.s_propfilt.pCSCompValue != NULL)
			cstrDestruct(&stmt->d.s_propfilt.pCSCompValue);
		cnfstmtDestructLst(stmt->d.s_propfilt.t_then);
		break;
    case S_RELOAD_LOOKUP_TABLE:
        if (stmt->d.s_reload_lookup_table.table_name != NULL) {
			free(stmt->d.s_reload_lookup_table.table_name);
        }
        if (stmt->d.s_reload_lookup_table.stub_value != NULL) {
			free(stmt->d.s_reload_lookup_table.stub_value);
        }
	default:
		DBGPRINTF("error: unknown stmt type during destruct %u\n",
			(unsigned) stmt->nodetype);
		break;
	}
	free(stmt->printable);
	free(stmt);
}

/* delete a stmt and all others following it */
void
cnfstmtDestructLst(struct cnfstmt *root)
{
	struct cnfstmt *stmt, *todel;
	for(stmt = root ; stmt != NULL ; ) {
		todel = stmt;
		stmt = stmt->next;
		cnfstmtDestruct(todel);
	}
}

struct cnfitr *
cnfNewIterator(char *var, struct cnfexpr *collection)
{
	struct cnfitr* itr;
	if ((itr = malloc(sizeof(struct cnfitr))) != NULL) {
		itr->var = var;
		itr->collection = collection;
	}
	return itr;
}

static void
cnfIteratorDestruct(struct cnfitr *itr)
{
	free(itr->var);
	if(itr->collection != NULL)
		cnfexprDestruct(itr->collection);
	free(itr);
}

struct cnfstmt *
cnfstmtNewSet(char *var, struct cnfexpr *expr, int force_reset)
{
	struct cnfstmt* cnfstmt;
	if((cnfstmt = cnfstmtNew(S_SET)) != NULL) {
		cnfstmt->d.s_set.varname = (uchar*) var;
		cnfstmt->d.s_set.expr = expr;
		cnfstmt->d.s_set.force_reset = force_reset;
	}
	return cnfstmt;
}

struct cnfstmt *
cnfstmtNewCall(es_str_t *name)
{
	struct cnfstmt* cnfstmt;
	if((cnfstmt = cnfstmtNew(S_CALL)) != NULL) {
		cnfstmt->d.s_call.name = name;
	}
	return cnfstmt;
}

struct cnfstmt *
cnfstmtNewReloadLookupTable(struct cnffparamlst *fparams)
{
	int nParams;
	struct cnffparamlst *param, *nxt;
	struct cnfstmt* cnfstmt;
	uint8_t failed = 0;
	if((cnfstmt = cnfstmtNew(S_RELOAD_LOOKUP_TABLE)) != NULL) {
		nParams = 0;
		for(param = fparams ; param != NULL ; param = param->next) {
			++nParams;
		}
		cnfstmt->d.s_reload_lookup_table.table_name = cnfstmt->d.s_reload_lookup_table.stub_value = NULL;
		switch(nParams) {
		case 2:
			param = fparams->next;
			if (param->expr->nodetype != 'S') {
				parser_errmsg("statement ignored: reload_lookup_table(table_name, optional:stub_value_in_case_reload_fails) "
							  "expects a litteral string for second argument\n");
				failed = 1;
			}
			if ((cnfstmt->d.s_reload_lookup_table.stub_value =
			(uchar*) es_str2cstr(((struct cnfstringval*)param->expr)->estr, NULL)) == NULL) {
				parser_errmsg("statement ignored: reload_lookup_table statement "
				"failed to allocate memory for lookup-table stub-value\n");
				failed = 1;
			}
		case 1:
			param = fparams;
			if (param->expr->nodetype != 'S') {
				parser_errmsg("statement ignored: reload_lookup_table(table_name, optional:stub_value_in_case_reload_fails) "
							  "expects a litteral string for first argument\n");
				failed = 1;
			}
			if ((cnfstmt->d.s_reload_lookup_table.table_name =
			(uchar*) es_str2cstr(((struct cnfstringval*)param->expr)->estr, NULL)) == NULL) {
				parser_errmsg("statement ignored: reload_lookup_table statement "
				"failed to allocate memory for lookup-table name\n");
				failed = 1;
			}
			break;
		default:
			parser_errmsg("statement ignored: reload_lookup_table(table_name, optional:stub_value_in_case_reload_fails) "
						  "expected 1 or 2 arguments, but found '%d'\n", nParams);
			failed = 1;
		}
	}
	param = fparams;
	while(param != NULL) {
		nxt = param->next;
		if (param->expr != NULL) cnfexprDestruct(param->expr);
		free(param);
		param = nxt;
	}
	if (failed) {
		cnfstmt->nodetype = S_NOP;
		if (cnfstmt->d.s_reload_lookup_table.table_name != NULL) {
			free(cnfstmt->d.s_reload_lookup_table.table_name);
		}
		if (cnfstmt->d.s_reload_lookup_table.stub_value != NULL) {
			free(cnfstmt->d.s_reload_lookup_table.stub_value);
		}
	}
	return cnfstmt;
}

struct cnfstmt *
cnfstmtNewUnset(char *var)
{
	struct cnfstmt* cnfstmt;
	if((cnfstmt = cnfstmtNew(S_UNSET)) != NULL) {
		cnfstmt->d.s_unset.varname = (uchar*) var;
	}
	return cnfstmt;
}

struct cnfstmt *
cnfstmtNewContinue(void)
{
	return cnfstmtNew(S_NOP);
}

struct cnfstmt *
cnfstmtNewPRIFILT(char *prifilt, struct cnfstmt *t_then)
{
	struct cnfstmt* cnfstmt;
	if((cnfstmt = cnfstmtNew(S_PRIFILT)) != NULL) {
		cnfstmt->printable = (uchar*)prifilt;
		cnfstmt->d.s_prifilt.t_then = t_then;
		cnfstmt->d.s_prifilt.t_else = NULL;
		DecodePRIFilter((uchar*)prifilt, cnfstmt->d.s_prifilt.pmask);
	}
	return cnfstmt;
}

struct cnfstmt *
cnfstmtNewPROPFILT(char *propfilt, struct cnfstmt *t_then)
{
	struct cnfstmt* cnfstmt;
	if((cnfstmt = cnfstmtNew(S_PROPFILT)) != NULL) {
		cnfstmt->printable = (uchar*)propfilt;
		cnfstmt->d.s_propfilt.t_then = t_then;
		cnfstmt->d.s_propfilt.regex_cache = NULL;
		cnfstmt->d.s_propfilt.pCSCompValue = NULL;
		if(DecodePropFilter((uchar*)propfilt, cnfstmt) != RS_RET_OK) {
			cnfstmt->nodetype = S_NOP; /* disable action! */
			cnfstmtDestructLst(t_then); /* we do no longer need this */
		}
	}
	return cnfstmt;
}

struct cnfstmt *
cnfstmtNewAct(struct nvlst *lst)
{
	struct cnfstmt* cnfstmt;
	char namebuf[256];
	rsRetVal localRet;
	if((cnfstmt = cnfstmtNew(S_ACT)) == NULL) 
		goto done;
	localRet = actionNewInst(lst, &cnfstmt->d.act);
	if(localRet == RS_RET_OK_WARN) {
		parser_errmsg("warnings occured in file '%s' around line %d",
			      cnfcurrfn, yylineno);
	} else if(localRet != RS_RET_OK) {
		parser_errmsg("errors occured in file '%s' around line %d",
			      cnfcurrfn, yylineno);
		cnfstmt->nodetype = S_NOP; /* disable action! */
		goto done;
	}
	snprintf(namebuf, sizeof(namebuf)-1, "action(type=\"%s\" ...)",
		 modGetName(cnfstmt->d.act->pMod));
	namebuf[255] = '\0'; /* be on safe side */
	cnfstmt->printable = (uchar*)strdup(namebuf);
	nvlstChkUnused(lst);
	nvlstDestruct(lst);
done:	return cnfstmt;
}

struct cnfstmt *
cnfstmtNewLegaAct(char *actline)
{
	struct cnfstmt* cnfstmt;
	rsRetVal localRet;
	if((cnfstmt = cnfstmtNew(S_ACT)) == NULL) 
		goto done;
	cnfstmt->printable = (uchar*)strdup((char*)actline);
	localRet = cflineDoAction(loadConf, (uchar**)&actline, &cnfstmt->d.act);
	if(localRet != RS_RET_OK) {
		parser_errmsg("%s occured in file '%s' around line %d",
			      (localRet == RS_RET_OK_WARN) ? "warnings" : "errors",
			      cnfcurrfn, yylineno);
		if(localRet != RS_RET_OK_WARN) {
			cnfstmt->nodetype = S_NOP; /* disable action! */
			goto done;
		}
	}
done:	return cnfstmt;
}


/* returns 1 if the two expressions are constants, 0 otherwise
 * if both are constants, the expression subtrees are destructed
 * (this is an aid for constant folding optimizing) 
 */
static int
getConstNumber(struct cnfexpr *expr, long long *l, long long *r)
{
	int ret = 0;
	cnfexprOptimize(expr->l);
	cnfexprOptimize(expr->r);
	if(expr->l->nodetype == 'N') {
		if(expr->r->nodetype == 'N') {
			ret = 1;
			*l = ((struct cnfnumval*)expr->l)->val;
			*r = ((struct cnfnumval*)expr->r)->val;
			cnfexprDestruct(expr->l);
			cnfexprDestruct(expr->r);
		} else if(expr->r->nodetype == 'S') {
			ret = 1;
			*l = ((struct cnfnumval*)expr->l)->val;
			*r = es_str2num(((struct cnfstringval*)expr->r)->estr, NULL);
			cnfexprDestruct(expr->l);
			cnfexprDestruct(expr->r);
		}
	} else if(expr->l->nodetype == 'S') {
		if(expr->r->nodetype == 'N') {
			ret = 1;
			*l = es_str2num(((struct cnfstringval*)expr->l)->estr, NULL);
			*r = ((struct cnfnumval*)expr->r)->val;
			cnfexprDestruct(expr->l);
			cnfexprDestruct(expr->r);
		} else if(expr->r->nodetype == 'S') {
			ret = 1;
			*l = es_str2num(((struct cnfstringval*)expr->l)->estr, NULL);
			*r = es_str2num(((struct cnfstringval*)expr->r)->estr, NULL);
			cnfexprDestruct(expr->l);
			cnfexprDestruct(expr->r);
		}
	}
	return ret;
}


/* constant folding for string concatenation */
static void
constFoldConcat(struct cnfexpr *expr)
{
	es_str_t *estr;
	cnfexprOptimize(expr->l);
	cnfexprOptimize(expr->r);
	if(expr->l->nodetype == 'S') {
		if(expr->r->nodetype == 'S') {
			estr = ((struct cnfstringval*)expr->l)->estr;
			((struct cnfstringval*)expr->l)->estr = NULL;
			es_addStr(&estr, ((struct cnfstringval*)expr->r)->estr);
			cnfexprDestruct(expr->l);
			cnfexprDestruct(expr->r);
			expr->nodetype = 'S';
			((struct cnfstringval*)expr)->estr = estr;
		} else if(expr->r->nodetype == 'N') {
			es_str_t *numstr;
			estr = ((struct cnfstringval*)expr->l)->estr;
			((struct cnfstringval*)expr->l)->estr = NULL;
			numstr = es_newStrFromNumber(((struct cnfnumval*)expr->r)->val);
			es_addStr(&estr, numstr);
			es_deleteStr(numstr);
			cnfexprDestruct(expr->l);
			cnfexprDestruct(expr->r);
			expr->nodetype = 'S';
			((struct cnfstringval*)expr)->estr = estr;
		}
	} else if(expr->l->nodetype == 'N') {
		if(expr->r->nodetype == 'S') {
			estr = es_newStrFromNumber(((struct cnfnumval*)expr->l)->val);
			es_addStr(&estr, ((struct cnfstringval*)expr->r)->estr);
			cnfexprDestruct(expr->l);
			cnfexprDestruct(expr->r);
			expr->nodetype = 'S';
			((struct cnfstringval*)expr)->estr = estr;
		} else if(expr->r->nodetype == 'S') {
			es_str_t *numstr;
			estr = es_newStrFromNumber(((struct cnfnumval*)expr->l)->val);
			numstr = es_newStrFromNumber(((struct cnfnumval*)expr->r)->val);
			es_addStr(&estr, numstr);
			es_deleteStr(numstr);
			cnfexprDestruct(expr->l);
			cnfexprDestruct(expr->r);
			expr->nodetype = 'S';
			((struct cnfstringval*)expr)->estr = estr;
		}
	}
}


/* optimize comparisons with syslog severity/facility. This is a special
 * handler as the numerical values also support GT, LT, etc ops.
 */
static struct cnfexpr*
cnfexprOptimize_CMP_severity_facility(struct cnfexpr *expr)
{
	struct cnffunc *func;

	if(expr->l->nodetype != 'V')
		FINALIZE;

	if(!strcmp("syslogseverity", ((struct cnfvar*)expr->l)->name)) {
		if(expr->r->nodetype == 'N') {
			int sev = (int) ((struct cnfnumval*)expr->r)->val;
			if(sev >= 0 && sev <= 7) {
				DBGPRINTF("optimizer: change comparison OP to FUNC prifilt()\n");
				func = cnffuncNew_prifilt(0); /* fac is irrelevant, set below... */
				prifiltSetSeverity(func->funcdata, sev, expr->nodetype);
				cnfexprDestruct(expr);
				expr = (struct cnfexpr*) func;
			} else {
				parser_errmsg("invalid syslogseverity %d, expression will always "
					      "evaluate to FALSE", sev);
			}
		}
	} else if(!strcmp("syslogfacility", ((struct cnfvar*)expr->l)->name)) {
		if(expr->r->nodetype == 'N') {
			int fac = (int) ((struct cnfnumval*)expr->r)->val;
			if(fac >= 0 && fac <= 24) {
				DBGPRINTF("optimizer: change comparison OP to FUNC prifilt()\n");
				func = cnffuncNew_prifilt(0); /* fac is irrelevant, set below... */
				prifiltSetFacility(func->funcdata, fac, expr->nodetype);
				cnfexprDestruct(expr);
				expr = (struct cnfexpr*) func;
			} else {
				parser_errmsg("invalid syslogfacility %d, expression will always "
					      "evaluate to FALSE", fac);
			}
		}
	}
finalize_it:
	return expr;
}

/* optimize a comparison with a variable as left-hand operand
 * NOTE: Currently support CMP_EQ, CMP_NE only and code NEEDS 
 *       TO BE CHANGED fgr other comparisons!
 */
static struct cnfexpr*
cnfexprOptimize_CMP_var(struct cnfexpr *expr)
{
	struct cnffunc *func;

	if(!strcmp("syslogfacility-text", ((struct cnfvar*)expr->l)->name)) {
		if(expr->r->nodetype == 'S') {
			char *cstr = es_str2cstr(((struct cnfstringval*)expr->r)->estr, NULL);
			int fac = decodeSyslogName((uchar*)cstr, syslogFacNames);
			if(fac == -1) {
				parser_errmsg("invalid facility '%s', expression will always "
					      "evaluate to FALSE", cstr);
			} else {
				/* we can actually optimize! */
				DBGPRINTF("optimizer: change comparison OP to FUNC prifilt()\n");
				func = cnffuncNew_prifilt(fac);
				if(expr->nodetype == CMP_NE)
					prifiltInvert(func->funcdata);
				cnfexprDestruct(expr);
				expr = (struct cnfexpr*) func;
			}
			free(cstr);
		}
	} else if(!strcmp("syslogseverity-text", ((struct cnfvar*)expr->l)->name)) {
		if(expr->r->nodetype == 'S') {
			char *cstr = es_str2cstr(((struct cnfstringval*)expr->r)->estr, NULL);
			int sev = decodeSyslogName((uchar*)cstr, syslogPriNames);
			if(sev == -1) {
				parser_errmsg("invalid syslogseverity '%s', expression will always "
					      "evaluate to FALSE", cstr);
			} else {
				/* we can acutally optimize! */
				DBGPRINTF("optimizer: change comparison OP to FUNC prifilt()\n");
				func = cnffuncNew_prifilt(0);
				prifiltSetSeverity(func->funcdata, sev, expr->nodetype);
				cnfexprDestruct(expr);
				expr = (struct cnfexpr*) func;
			}
			free(cstr);
		}
	} else {
		expr = cnfexprOptimize_CMP_severity_facility(expr);
	}
	return expr;
}

static struct cnfexpr*
cnfexprOptimize_NOT(struct cnfexpr *expr)
{
	struct cnffunc *func;

	if(expr->r->nodetype == 'F') {
		func = (struct cnffunc *)expr->r;
		if(func->fID == CNFFUNC_PRIFILT) {
			DBGPRINTF("optimize NOT prifilt() to inverted prifilt()\n");
			expr->r = NULL;
			cnfexprDestruct(expr);
			prifiltInvert(func->funcdata);
			expr = (struct cnfexpr*) func;
		}
	}
	return expr;
}

static struct cnfexpr*
cnfexprOptimize_AND_OR(struct cnfexpr *expr)
{
	struct cnffunc *funcl, *funcr;

	if(expr->l->nodetype == 'F') {
		if(expr->r->nodetype == 'F') {
			funcl = (struct cnffunc *)expr->l;
			funcr = (struct cnffunc *)expr->r;
			if(funcl->fID == CNFFUNC_PRIFILT && funcr->fID == CNFFUNC_PRIFILT) {
				DBGPRINTF("optimize combine AND/OR prifilt()\n");
				expr->l = NULL;
				prifiltCombine(funcl->funcdata, funcr->funcdata, expr->nodetype);
				cnfexprDestruct(expr);
				expr = (struct cnfexpr*) funcl;
			}
		}
	}
	return expr;
}


/* optimize array for EQ/NEQ comparisons. We sort the array in
 * this case so that we can apply binary search later on.
 */
static inline void
cnfexprOptimize_CMPEQ_arr(struct cnfarray *arr)
{
	DBGPRINTF("optimizer: sorting array of %d members for CMP_EQ/NEQ comparison\n", arr->nmemb);
	qsort(arr->arr, arr->nmemb, sizeof(es_str_t*), qs_arrcmp);
}


/* (recursively) optimize an expression */
struct cnfexpr*
cnfexprOptimize(struct cnfexpr *expr)
{
	long long ln, rn;
	struct cnfexpr *exprswap;

	DBGPRINTF("optimize expr %p, type '%s'\n", expr, tokenToString(expr->nodetype));
	switch(expr->nodetype) {
	case '&':
		constFoldConcat(expr);
		break;
	case '+':
		if(getConstNumber(expr, &ln, &rn))  {
			expr->nodetype = 'N';
			((struct cnfnumval*)expr)->val = ln + rn;
		}
		break;
	case '-':
		if(getConstNumber(expr, &ln, &rn))  {
			expr->nodetype = 'N';
			((struct cnfnumval*)expr)->val = ln - rn;
		}
		break;
	case '*':
		if(getConstNumber(expr, &ln, &rn))  {
			expr->nodetype = 'N';
			((struct cnfnumval*)expr)->val = ln * rn;
		}
		break;
	case '/':
		if(getConstNumber(expr, &ln, &rn))  {
			expr->nodetype = 'N';
			if(rn == 0) {
				/* division by zero */
				((struct cnfnumval*)expr)->val = 0;
			} else {
				((struct cnfnumval*)expr)->val = ln / rn;
			}
		}
		break;
	case '%':
		if(getConstNumber(expr, &ln, &rn))  {
			expr->nodetype = 'N';
			if(rn == 0) {
				/* division by zero */
				((struct cnfnumval*)expr)->val = 0;
			} else {
				((struct cnfnumval*)expr)->val = ln % rn;
			}
		}
		break;
	case CMP_NE:
	case CMP_EQ:
		expr->l = cnfexprOptimize(expr->l);
		expr->r = cnfexprOptimize(expr->r);
		if(expr->l->nodetype == 'A') {
			if(expr->r->nodetype == 'A') {
				parser_errmsg("warning: '==' or '<>' "
				  "comparison of two constant string "
				  "arrays makes no sense");
			} else { /* swap for simpler execution step */
				exprswap = expr->l;
				expr->l = expr->r;
				expr->r = exprswap;
			}
		}
		if(expr->r->nodetype == 'A') {
			cnfexprOptimize_CMPEQ_arr((struct cnfarray *)expr->r);
		}
		/* This should be evaluated last because it may change expr
		 * to a function.
		 */
		if(expr->l->nodetype == 'V') {
			expr = cnfexprOptimize_CMP_var(expr);
		}
		break;
	case CMP_LE:
	case CMP_GE:
	case CMP_LT:
	case CMP_GT:
		expr->l = cnfexprOptimize(expr->l);
		expr->r = cnfexprOptimize(expr->r);
		expr = cnfexprOptimize_CMP_severity_facility(expr);
		break;
	case CMP_CONTAINS:
	case CMP_CONTAINSI:
	case CMP_STARTSWITH:
	case CMP_STARTSWITHI:
		expr->l = cnfexprOptimize(expr->l);
		expr->r = cnfexprOptimize(expr->r);
		break;
	case AND:
	case OR:
		expr->l = cnfexprOptimize(expr->l);
		expr->r = cnfexprOptimize(expr->r);
		expr = cnfexprOptimize_AND_OR(expr);
		break;
	case NOT:
		expr->r = cnfexprOptimize(expr->r);
		expr = cnfexprOptimize_NOT(expr);
		break;
	default:/* nodetypes we cannot optimize */
		break;
	}
	return expr;
}

/* removes NOPs from a statement list and returns the
 * first non-NOP entry.
 */
static struct cnfstmt *
removeNOPs(struct cnfstmt *root)
{
	struct cnfstmt *stmt, *toDel, *prevstmt = NULL;
	struct cnfstmt *newRoot = NULL;
	
	if(root == NULL) goto done;
	stmt = root;
	while(stmt != NULL) {
		if(stmt->nodetype == S_NOP) {
			if(prevstmt != NULL)
				/* end chain, is rebuild if more non-NOPs follow */
				prevstmt->next = NULL;
			toDel = stmt;
			stmt = stmt->next;
			cnfstmtDestruct(toDel);
		} else {
			if(newRoot == NULL)
				newRoot = stmt;
			if(prevstmt != NULL)
				prevstmt->next = stmt;
			prevstmt = stmt;
			stmt = stmt->next;
		}
	}
done:	return newRoot;
}

static void
cnfstmtOptimizeForeach(struct cnfstmt *stmt)
{
	stmt->d.s_foreach.iter->collection = cnfexprOptimize(stmt->d.s_foreach.iter->collection);
	stmt->d.s_foreach.body = removeNOPs(stmt->d.s_foreach.body);
	cnfstmtOptimize(stmt->d.s_foreach.body);
}


static void
cnfstmtOptimizeIf(struct cnfstmt *stmt)
{
	struct cnfstmt *t_then, *t_else;
	struct cnfexpr *expr;
	struct cnffunc *func;
	struct funcData_prifilt *prifilt;

	expr = stmt->d.s_if.expr = cnfexprOptimize(stmt->d.s_if.expr);
	stmt->d.s_if.t_then = removeNOPs(stmt->d.s_if.t_then);
	stmt->d.s_if.t_else = removeNOPs(stmt->d.s_if.t_else);
	cnfstmtOptimize(stmt->d.s_if.t_then);
	cnfstmtOptimize(stmt->d.s_if.t_else);

	if(stmt->d.s_if.expr->nodetype == 'F') {
		func = (struct cnffunc*)expr;
		   if(func->fID == CNFFUNC_PRIFILT) {
			DBGPRINTF("optimizer: change IF to PRIFILT\n");
			t_then = stmt->d.s_if.t_then;
			t_else = stmt->d.s_if.t_else;
			stmt->nodetype = S_PRIFILT;
			prifilt = (struct funcData_prifilt*) func->funcdata;
			memcpy(stmt->d.s_prifilt.pmask, prifilt->pmask,
				sizeof(prifilt->pmask));
			stmt->d.s_prifilt.t_then = t_then;
			stmt->d.s_prifilt.t_else = t_else;
			if(func->nParams == 0)
				stmt->printable = (uchar*)strdup("[Optimizer Result]");
			else
				stmt->printable = (uchar*)
					es_str2cstr(((struct cnfstringval*)func->expr[0])->estr, NULL);
			cnfexprDestruct(expr);
			cnfstmtOptimizePRIFilt(stmt);
		}
	}
}

static void
cnfstmtOptimizeAct(struct cnfstmt *stmt)
{
	action_t *pAct;

	pAct = stmt->d.act;
	if(!strcmp((char*)modGetName(pAct->pMod), "builtin:omdiscard")) {
		DBGPRINTF("optimizer: replacing omdiscard by STOP\n");
		actionDestruct(stmt->d.act);
		stmt->nodetype = S_STOP;
	}
}

static void
cnfstmtOptimizePRIFilt(struct cnfstmt *stmt)
{
	int i;
	int isAlways = 1;
	struct cnfstmt *subroot, *last;

	stmt->d.s_prifilt.t_then = removeNOPs(stmt->d.s_prifilt.t_then);
	cnfstmtOptimize(stmt->d.s_prifilt.t_then);

	for(i = 0; i <= LOG_NFACILITIES; i++)
		if(stmt->d.s_prifilt.pmask[i] != 0xff) {
			isAlways = 0;
			break;
		}
	if(!isAlways)
		goto done;

	DBGPRINTF("optimizer: removing always-true PRIFILT %p\n", stmt);
	if(stmt->d.s_prifilt.t_else != NULL) {
		parser_errmsg("error: always-true PRI filter has else part!\n");
		cnfstmtDestructLst(stmt->d.s_prifilt.t_else);
	}
	free(stmt->printable);
	stmt->printable = NULL;
	subroot = stmt->d.s_prifilt.t_then;
	if(subroot == NULL) {
		/* very strange, we set it to NOP, best we can do
		 * This case is NOT expected in practice
		 */
		 stmt->nodetype = S_NOP;
		 goto done;
	}
	for(last = subroot ; last->next != NULL ; last = last->next)
		/* find last node in subtree */;
	last->next = stmt->next;
	memcpy(stmt, subroot, sizeof(struct cnfstmt));
	free(subroot);

done:	return;
}

static void
cnfstmtOptimizeReloadLookupTable(struct cnfstmt *stmt) {
	if((stmt->d.s_reload_lookup_table.table = lookupFindTable(stmt->d.s_reload_lookup_table.table_name)) == NULL) {
		parser_errmsg("lookup table '%s' not found\n", stmt->d.s_reload_lookup_table.table_name);
	}
}

/* we abuse "optimize" a bit. Actually, we obtain a ruleset pointer, as
 * all rulesets are only known later in the process (now!).
 */
static void
cnfstmtOptimizeCall(struct cnfstmt *stmt)
{
	ruleset_t *pRuleset;
	rsRetVal localRet;
	uchar *rsName;

	rsName = (uchar*) es_str2cstr(stmt->d.s_call.name, NULL);
	localRet = rulesetGetRuleset(loadConf, &pRuleset, rsName);
	if(localRet != RS_RET_OK) {
		/* in that case, we accept that a NOP will "survive" */
		parser_errmsg("ruleset '%s' cannot be found\n", rsName);
		es_deleteStr(stmt->d.s_call.name);
		stmt->nodetype = S_NOP;
		goto done;
	}
	DBGPRINTF("CALL obtained ruleset ptr %p for ruleset '%s' [hasQueue:%d]\n",
		  pRuleset, rsName, rulesetHasQueue(pRuleset));
	if(rulesetHasQueue(pRuleset)) {
		stmt->d.s_call.ruleset = pRuleset;
	} else {
		stmt->d.s_call.ruleset = NULL;
		stmt->d.s_call.stmt = pRuleset->root;
	}
done:
	free(rsName);
	return;
}
/* (recursively) optimize a statement */
void
cnfstmtOptimize(struct cnfstmt *root)
{
	struct cnfstmt *stmt;
	if(root == NULL) goto done;
	for(stmt = root ; stmt != NULL ; stmt = stmt->next) {
		switch(stmt->nodetype) {
		case S_IF:
			cnfstmtOptimizeIf(stmt);
			break;
		case S_FOREACH:
			cnfstmtOptimizeForeach(stmt);
			break;
		case S_PRIFILT:
			cnfstmtOptimizePRIFilt(stmt);
			break;
		case S_PROPFILT:
			stmt->d.s_propfilt.t_then = removeNOPs(stmt->d.s_propfilt.t_then);
			cnfstmtOptimize(stmt->d.s_propfilt.t_then);
			break;
		case S_SET:
			stmt->d.s_set.expr = cnfexprOptimize(stmt->d.s_set.expr);
			break;
		case S_ACT:
			cnfstmtOptimizeAct(stmt);
			break;
		case S_CALL:
			cnfstmtOptimizeCall(stmt);
			break;
		case S_STOP:
			if(stmt->next != NULL)
				parser_errmsg("STOP is followed by unreachable statements!\n");
			break;
		case S_UNSET: /* nothing to do */
			break;
        case S_RELOAD_LOOKUP_TABLE:
            cnfstmtOptimizeReloadLookupTable(stmt);
			break;
		case S_NOP:
			DBGPRINTF("optimizer error: we see a NOP, how come?\n");
			break;
		default:
			DBGPRINTF("error: unknown stmt type %u during optimizer run\n",
				(unsigned) stmt->nodetype);
			break;
		}
	}
done:	return;
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

static const char* const numInWords[] = {"zero", "one", "two", "three", "four", "five", "six"};

#define GENERATE_FUNC_WITH_NARG_RANGE(name, minArg, maxArg, funcId, errMsg) \
	if(nParams < minArg || nParams > maxArg) {	\
		parser_errmsg(errMsg, name, nParams);	\
		return CNFFUNC_INVALID;			\
	}						\
	return funcId


#define GENERATE_FUNC_WITH_ERR_MSG(name, expectedParams, funcId, errMsg) \
	if(nParams != expectedParams) {										\
		parser_errmsg(errMsg, name, numInWords[expectedParams], nParams); \
		return CNFFUNC_INVALID;											\
	}																	\
	return funcId


#define GENERATE_FUNC(name, expectedParams, func_id)					\
	GENERATE_FUNC_WITH_ERR_MSG(											\
		name, expectedParams, func_id,									\
		"number of parameters for %s() must be %s but is %d.")


#define FUNC_NAME(name) !es_strbufcmp(fname, (unsigned char*)name, sizeof(name) - 1)


/* Obtain function id from name AND number of params. Issues the
 * relevant error messages if errors are detected.
 */
static enum cnffuncid
funcName2ID(es_str_t *fname, unsigned short nParams)
{
	if(FUNC_NAME("strlen")) {
		GENERATE_FUNC("strlen", 1, CNFFUNC_STRLEN);
	} else if(FUNC_NAME("getenv")) {
		GENERATE_FUNC("getenv", 1, CNFFUNC_GETENV);
	} else if(FUNC_NAME("tolower")) {
		GENERATE_FUNC("tolower", 1, CNFFUNC_TOLOWER);
	} else if(FUNC_NAME("cstr")) {
		GENERATE_FUNC("cstr", 1, CNFFUNC_CSTR);
	} else if(FUNC_NAME("cnum")) {
		GENERATE_FUNC("cnum", 1, CNFFUNC_CNUM);
	} else if(FUNC_NAME("re_match")) {
		GENERATE_FUNC("re_match", 2, CNFFUNC_RE_MATCH);
	} else if(FUNC_NAME("re_extract")) {
		GENERATE_FUNC("re_extract", 5, CNFFUNC_RE_EXTRACT);
	} else if(FUNC_NAME("field")) {
		GENERATE_FUNC("field", 3, CNFFUNC_FIELD);
	} else if(FUNC_NAME("exec_template")) {
		GENERATE_FUNC("exec_template", 1, CNFFUNC_EXEC_TEMPLATE);
	} else if(FUNC_NAME("prifilt")) {
		GENERATE_FUNC("prifilt", 1, CNFFUNC_PRIFILT);
	} else if(FUNC_NAME("lookup")) {
		GENERATE_FUNC("lookup", 2, CNFFUNC_LOOKUP);
	} else if(FUNC_NAME("dyn_inc")) {
		GENERATE_FUNC("dyn_inc", 2, CNFFUNC_DYN_INC);
	} else if(FUNC_NAME("replace")) {
		GENERATE_FUNC_WITH_ERR_MSG(
			"replace", 3, CNFFUNC_REPLACE,
			"number of parameters for %s() must be %s "
			"(operand_string, fragment_to_find, fragment_to_replace_in_its_place)"
			"but is %d.");
	} else if(FUNC_NAME("wrap")) {
		GENERATE_FUNC_WITH_NARG_RANGE("wrap", 2, 3, CNFFUNC_WRAP,
			"number of parameters for %s() must either be "
			"two (operand_string, wrapper) or"
			"three (operand_string, wrapper, wrapper_escape_str)"
			"but is %d.");
	} else if(FUNC_NAME("random")) {
		GENERATE_FUNC("random", 1, CNFFUNC_RANDOM);
	} else {
		return CNFFUNC_INVALID;
	}
}


static rsRetVal
initFunc_re_match(struct cnffunc *func)
{
	rsRetVal localRet;
	char *regex = NULL;
	regex_t *re;
	DEFiRet;

	if(func->nParams < 2) {
		parser_errmsg("rsyslog logic error in line %d of file %s\n",
			__LINE__, __FILE__);
		FINALIZE;
	}

	func->funcdata = NULL;
	if(func->expr[1]->nodetype != 'S') {
		parser_errmsg("param 2 of re_match/extract() must be a constant string");
		FINALIZE;
	}

	CHKmalloc(re = malloc(sizeof(regex_t)));
	func->funcdata = re;

	regex = es_str2cstr(((struct cnfstringval*) func->expr[1])->estr, NULL);
	
	if((localRet = objUse(regexp, LM_REGEXP_FILENAME)) == RS_RET_OK) {
		if(regexp.regcomp(re, (char*) regex, REG_EXTENDED) != 0) {
			parser_errmsg("cannot compile regex '%s'", regex);
			ABORT_FINALIZE(RS_RET_ERR);
		}
	} else { /* regexp object could not be loaded */
		parser_errmsg("could not load regex support - regex ignored");
		ABORT_FINALIZE(RS_RET_ERR);
	}

finalize_it:
	free(regex);
	RETiRet;
}


static rsRetVal
initFunc_exec_template(struct cnffunc *func)
{
	char *tplName = NULL;
	DEFiRet;

	func->destructable_funcdata = 0;

	if(func->nParams != 1) {
		parser_errmsg("rsyslog logic error in line %d of file %s\n",
			__LINE__, __FILE__);
		FINALIZE;
	}

	if(func->expr[0]->nodetype != 'S') {
		parser_errmsg("exec_template(): param 1 must be a constant string");
		FINALIZE;
	}

	tplName = es_str2cstr(((struct cnfstringval*) func->expr[0])->estr, NULL);
	func->funcdata = tplFind(ourConf, tplName, strlen(tplName));
	if(func->funcdata == NULL) {
		parser_errmsg("exec_template(): template '%s' could not be found", tplName);
		FINALIZE;
	}


finalize_it:
	free(tplName);
	RETiRet;
}


static rsRetVal
initFunc_prifilt(struct cnffunc *func)
{
	struct funcData_prifilt *pData;
	uchar *cstr;
	DEFiRet;

	if(func->nParams != 1) {
		parser_errmsg("rsyslog logic error in line %d of file %s\n",
			__LINE__, __FILE__);
		FINALIZE;
	}

	func->funcdata = NULL;
	if(func->expr[0]->nodetype != 'S') {
		parser_errmsg("param 1 of prifilt() must be a constant string");
		FINALIZE;
	}

	CHKmalloc(pData = calloc(1, sizeof(struct funcData_prifilt)));
	func->funcdata = pData;
	cstr = (uchar*)es_str2cstr(((struct cnfstringval*) func->expr[0])->estr, NULL);
	CHKiRet(DecodePRIFilter(cstr, pData->pmask));
	free(cstr);
finalize_it:
	RETiRet;
}


static rsRetVal
resolveLookupTable(struct cnffunc *func)
{
	uchar *cstr = NULL;
	char *fn_name = NULL;
	DEFiRet;

	func->destructable_funcdata = 0;

	if(func->nParams == 0) {/*we assume first arg is lookup-table-name*/
		parser_errmsg("rsyslog logic error in line %d of file %s\n",
			__LINE__, __FILE__);
		FINALIZE;
	}

	CHKmalloc(fn_name = es_str2cstr(func->fname, NULL));

	func->funcdata = NULL;
	if(func->expr[0]->nodetype != 'S') {
		parser_errmsg("table name (param 1) of %s() must be a constant string", fn_name);
		FINALIZE;
	}

	CHKmalloc(cstr = (uchar*)es_str2cstr(((struct cnfstringval*) func->expr[0])->estr, NULL));
	if((func->funcdata = lookupFindTable(cstr)) == NULL) {
		parser_errmsg("lookup table '%s' not found (used in function: %s)", cstr, fn_name);
		FINALIZE;
	}

finalize_it:
	free(cstr);
	free(fn_name);
	RETiRet;
}

static rsRetVal
initFunc_dyn_stats(struct cnffunc *func)
{
	uchar *cstr = NULL;
	DEFiRet;

	func->destructable_funcdata = 0;

	if(func->nParams != 2) {
		parser_errmsg("rsyslog logic error in line %d of file %s\n",
					  __LINE__, __FILE__);
		FINALIZE;
	}

	func->funcdata = NULL;
	if(func->expr[0]->nodetype != 'S') {
		parser_errmsg("dyn-stats bucket-name (param 1) of dyn-stats manipulating "
		"functions like dyn_inc must be a constant string");
		FINALIZE;
	}

	cstr = (uchar*)es_str2cstr(((struct cnfstringval*) func->expr[0])->estr, NULL);
	if((func->funcdata = dynstats_findBucket(cstr)) == NULL) {
		parser_errmsg("dyn-stats bucket '%s' not found", cstr);
		FINALIZE;
	}

finalize_it:
	free(cstr);
	RETiRet;
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
		func->funcdata = NULL;
		func->destructable_funcdata = 1;
		func->fID = funcName2ID(fname, nParams);
		/* shuffle params over to array (access speed!) */
		param = paramlst;
		for(i = 0 ; i < nParams ; ++i) {
			func->expr[i] = param->expr;
			toDel = param;
			param = param->next;
			free(toDel);
		}
		/* some functions require special initialization */
		switch(func->fID) {
			case CNFFUNC_RE_MATCH:
			case CNFFUNC_RE_EXTRACT:
				/* need to compile the regexp in param 2, so this MUST be a constant */
				initFunc_re_match(func);
				break;
			case CNFFUNC_PRIFILT:
				initFunc_prifilt(func);
				break;
			case CNFFUNC_LOOKUP:
				resolveLookupTable(func);
				break;
			case CNFFUNC_EXEC_TEMPLATE:
				initFunc_exec_template(func);
				break;
			case CNFFUNC_DYN_INC:
				initFunc_dyn_stats(func);
				break;
			default:break;
		}
	}
	return func;
}


/* A special function to create a prifilt() expression during optimization
 * phase.
 */
struct cnffunc *
cnffuncNew_prifilt(int fac)
{
	struct cnffunc* func;

	fac >>= 3;
	if (fac >= LOG_NFACILITIES + 1 || fac < 0)
		return NULL;

	if((func = malloc(sizeof(struct cnffunc))) != NULL) {
		if ((func->funcdata = calloc(1, sizeof(struct funcData_prifilt))) == NULL) {
			free(func);
			return NULL;
		}
		func->nodetype = 'F';
		func->fname = es_newStrFromCStr("prifilt", sizeof("prifilt")-1);
		func->nParams = 0;
		func->fID = CNFFUNC_PRIFILT;
		func->destructable_funcdata = 1;
		((struct funcData_prifilt *)func->funcdata)->pmask[fac] = TABLE_ALLPRI;
	}
	return func;
}


/* returns 0 if everything is OK and config parsing shall continue,
 * and 1 if things are so wrong that config parsing shall be aborted.
 */
int
cnfDoInclude(char *name)
{
	char *cfgFile;
	char *finalName;
	int i;
	int result;
	glob_t cfgFiles;
	struct stat fileInfo;
	char nameBuf[MAXFNAME+1];
	char cwdBuf[MAXFNAME+1];

	finalName = name;
	if(stat(name, &fileInfo) == 0) {
		/* stat usually fails if we have a wildcard - so this does NOT indicate error! */
		if(S_ISDIR(fileInfo.st_mode)) {
			/* if we have a directory, we need to add "*" to get its files */
			snprintf(nameBuf, sizeof(nameBuf), "%s*", name);
			finalName = nameBuf;
		}
	}

	/* Use GLOB_MARK to append a trailing slash for directories. */
	/* Use GLOB_NOMAGIC to detect wildcards that match nothing. */
#ifdef HAVE_GLOB_NOMAGIC
	/* Silently ignore wildcards that match nothing */
	result = glob(finalName, GLOB_MARK | GLOB_NOMAGIC, NULL, &cfgFiles);
	if(result == GLOB_NOMATCH) {
#else
	result = glob(finalName, GLOB_MARK, NULL, &cfgFiles);
    if(result == GLOB_NOMATCH && containsGlobWildcard(finalName)) {
#endif /* HAVE_GLOB_NOMAGIC */
        return 0;
    }

	if(result == GLOB_NOSPACE || result == GLOB_ABORTED) {
		char errStr[1024];
		rs_strerror_r(errno, errStr, sizeof(errStr));
		if(getcwd(cwdBuf, sizeof(cwdBuf)) == NULL)
			strcpy(cwdBuf, "??getcwd() failed??");
		parser_errmsg("error accessing config file or directory '%s' [cwd:%s]: %s",
				finalName, cwdBuf, errStr);
		return 1;
	}

	/* note: bison "stacks" the files, so we need to submit them
	 * in reverse order to the *stack* in order to get the proper
	 * parsing order. Also see
	 * http://bugzilla.adiscon.com/show_bug.cgi?id=411
	 */
	for(i = cfgFiles.gl_pathc - 1; i >= 0 ; i--) {
		cfgFile = cfgFiles.gl_pathv[i];
		if(stat(cfgFile, &fileInfo) != 0) {
			char errStr[1024];
			rs_strerror_r(errno, errStr, sizeof(errStr));
			if(getcwd(cwdBuf, sizeof(cwdBuf)) == NULL)
				strcpy(cwdBuf, "??getcwd() failed??");
			parser_errmsg("error accessing config file or directory '%s' "
					"[cwd: %s]: %s", cfgFile, cwdBuf, errStr);
			return 1;
		}

		if(S_ISREG(fileInfo.st_mode)) { /* config file */
			DBGPRINTF("requested to include config file '%s'\n", cfgFile);
			cnfSetLexFile(cfgFile);
		} else if(S_ISDIR(fileInfo.st_mode)) { /* config directory */
			DBGPRINTF("requested to include directory '%s'\n", cfgFile);
			cnfDoInclude(cfgFile);
		} else {
			DBGPRINTF("warning: unable to process IncludeConfig directive '%s'\n", cfgFile);
		}
	}

	globfree(&cfgFiles);
	return 0;
}

void
varDelete(const struct svar *v)
{
	switch(v->datatype) {
	case 'S':
	case 'J':
		varFreeMembers(v);
		break;
	case 'A':
		cnfarrayContentDestruct(v->d.ar);
		free(v->d.ar);
		break;
	default:break;
	}
}

void
cnfparamvalsDestruct(const struct cnfparamvals *paramvals, const struct cnfparamblk *blk)
{
	int i;
	if(paramvals == NULL)
		return;
	for(i = 0 ; i < blk->nParams ; ++i) {
		if(paramvals[i].bUsed) {
			varDelete(&paramvals[i].val);
		}
	}
	free((void*)paramvals);
}

/* find the index (or -1!) for a config param by name. This is used to 
 * address the parameter array. Of course, we could use with static
 * indices, but that would create some extra bug potential. So we
 * resort to names. As we do this only during the initial config parsing
 * stage the (considerable!) extra overhead is OK. -- rgerhards, 2011-07-19
 */
int
cnfparamGetIdx(struct cnfparamblk *params, const char *name)
{
	int i;
	for(i = 0 ; i < params->nParams ; ++i)
		if(!strcmp(params->descr[i].name, name))
			break;
	if(i == params->nParams)
		i = -1; /* not found */
	return i;
}


void
cstrPrint(const char *text, es_str_t *estr)
{
	char *str;
	str = es_str2cstr(estr, NULL);
	dbgprintf("%s%s", text, str);
	free(str);
}

char *
rmLeadingSpace(char *s)
{
	char *p;
	for(p = s ; *p && isspace(*p) ; ++p)
		;
	return(p);
}

/* init must be called once before any parsing of the script files start */
rsRetVal
initRainerscript(void)
{
	DEFiRet;
	CHKiRet(objGetObjInterface(&obj));
finalize_it:
	RETiRet;
}

/* we need a function to check for octal digits */
static inline int
isodigit(uchar c)
{
	return(c >= '0' && c <= '7');
}

/**
 * Get numerical value of a hex digit. This is a helper function.
 * @param[in] c a character containing 0..9, A..Z, a..z anything else
 * is an (undetected) error.
 */
static int
hexDigitVal(char c)
{
	int r;
	if(c < 'A')
		r = c - '0';
	else if(c < 'a')
		r = c - 'A' + 10;
	else
		r = c - 'a' + 10;
	return r;
}

/* Handle the actual unescaping.
 * a helper to unescapeStr(), to help make the function easier to read.
 */
static void
doUnescape(unsigned char *c, int len, int *iSrc, int iDst)
{
	if(c[*iSrc] == '\\') {
		if(++(*iSrc) == len) {
			/* error, incomplete escape, treat as single char */
			c[iDst] = '\\';
		}
		/* regular case, unescape */
		switch(c[*iSrc]) {
		case 'a':
			c[iDst] = '\007';
			break;
		case 'b':
			c[iDst] = '\b';
			break;
		case 'f':
			c[iDst] = '\014';
			break;
		case 'n':
			c[iDst] = '\n';
			break;
		case 'r':
			c[iDst] = '\r';
			break;
		case 't':
			c[iDst] = '\t';
			break;
		case '\'':
			c[iDst] = '\'';
			break;
		case '"':
			c[iDst] = '"';
			break;
		case '?':
			c[iDst] = '?';
			break;
		case '$':
			c[iDst] = '$';
			break;
		case '\\':
			c[iDst] = '\\';
			break;
		case 'x':
			if(    (*iSrc)+2 >= len
			   || !isxdigit(c[(*iSrc)+1])
			   || !isxdigit(c[(*iSrc)+2])) {
				/* error, incomplete escape, use as is */
				c[iDst] = '\\';
				--(*iSrc);
			}
			c[iDst] = (hexDigitVal(c[(*iSrc)+1]) << 4) +
				  hexDigitVal(c[(*iSrc)+2]);
			*iSrc += 2;
			break;
		case '0': /* octal escape */
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
			if(    (*iSrc)+2 >= len
			   || !isodigit(c[(*iSrc)+1])
			   || !isodigit(c[(*iSrc)+2])) {
				/* error, incomplete escape, use as is */
				c[iDst] = '\\';
				--(*iSrc);
			}
			c[iDst] = ((c[(*iSrc)  ] - '0') << 6) +
			          ((c[(*iSrc)+1] - '0') << 3) +
			          ( c[(*iSrc)+2] - '0');
			*iSrc += 2;
			break;
		default:
			/* error, incomplete escape, indicate by '?' */
			c[iDst] = '?';
			break;
		}
	} else {
		/* regular character */
		c[iDst] = c[*iSrc];
	}
}

void
unescapeStr(uchar *s, int len)
{
	int iSrc, iDst;
	assert(s != NULL);

	/* scan for first escape sequence (if we are luky, there is none!) */
	iSrc = 0;
	while(iSrc < len && s[iSrc] != '\\')
		++iSrc;
	/* now we have a sequence or end of string. In any case, we process
	 * all remaining characters (maybe 0!) and unescape.
	 */
	if(iSrc != len) {
		iDst = iSrc;
		while(iSrc < len) {
			doUnescape(s, len, &iSrc, iDst);
			++iSrc;
			++iDst;
		}
		s[iDst] = '\0';
	}
}

const char *
tokenval2str(const int tok)
{
	if(tok < 256) return "";
	switch(tok) {
	case NAME: return "NAME";
	case FUNC: return "FUNC";
	case BEGINOBJ: return "BEGINOBJ";
	case ENDOBJ: return "ENDOBJ";
	case BEGIN_ACTION: return "BEGIN_ACTION";
	case BEGIN_PROPERTY: return "BEGIN_PROPERTY";
	case BEGIN_CONSTANT: return "BEGIN_CONSTANT";
	case BEGIN_TPL: return "BEGIN_TPL";
	case BEGIN_RULESET: return "BEGIN_RULESET";
	case STOP: return "STOP";
	case SET: return "SET";
	case UNSET: return "UNSET";
	case CONTINUE: return "CONTINUE";
	case CALL: return "CALL";
	case LEGACY_ACTION: return "LEGACY_ACTION";
	case LEGACY_RULESET: return "LEGACY_RULESET";
	case PRIFILT: return "PRIFILT";
	case PROPFILT: return "PROPFILT";
	case BSD_TAG_SELECTOR: return "BSD_TAG_SELECTOR";
	case BSD_HOST_SELECTOR: return "BSD_HOST_SELECTOR";
	case IF: return "IF";
	case THEN: return "THEN";
	case ELSE: return "ELSE";
	case OR: return "OR";
	case AND: return "AND";
	case NOT: return "NOT";
	case VAR: return "VAR";
	case STRING: return "STRING";
	case NUMBER: return "NUMBER";
	case CMP_EQ: return "CMP_EQ";
	case CMP_NE: return "CMP_NE";
	case CMP_LE: return "CMP_LE";
	case CMP_GE: return "CMP_GE";
	case CMP_LT: return "CMP_LT";
	case CMP_GT: return "CMP_GT";
	case CMP_CONTAINS: return "CMP_CONTAINS";
	case CMP_CONTAINSI: return "CMP_CONTAINSI";
	case CMP_STARTSWITH: return "CMP_STARTSWITH";
	case CMP_STARTSWITHI: return "CMP_STARTSWITHI";
	case UMINUS: return "UMINUS";
	default: return "UNKNOWN TOKEN";
	}
}
