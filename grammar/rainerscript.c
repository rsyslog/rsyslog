/* rainerscript.c - routines to support RainerScript config language
 *
 * Module begun 2011-07-01 by Rainer Gerhards
 *
 * Copyright 2011-2012 Rainer Gerhards and Adiscon GmbH.
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
#include <sys/stat.h>
#include <sys/types.h>
#include <libestr.h>
#include "rsyslog.h"
#include "rainerscript.h"
#include "conf.h"
#include "parserif.h"
#include "rsconf.h"
#include "grammar.h"
#include "queue.h"
#include "srUtils.h"
#include "regexp.h"
#include "obj.h"
#include "modules.h"
#include "ruleset.h"

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
char *
tokenToString(int token)
{
	char *tokstr;
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
	default: snprintf(tokbuf, sizeof(tokbuf), "%c[%d]", token, token); 
		 tokstr = tokbuf; break;
	}
	return tokstr;
}


char*
getFIOPName(unsigned iFIOP)
{
	char *pRet;
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

static void
prifiltInvert(struct funcData_prifilt *prifilt)
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
prifiltSetFacility(struct funcData_prifilt *prifilt, int fac, int mode)
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
prifiltCombine(struct funcData_prifilt *prifilt, struct funcData_prifilt *prifilt2, int mode)
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
static inline struct nvlst*
nvlstFindNameCStr(struct nvlst *lst, char *name)
{
	es_size_t lenName = strlen(name);
	while(lst != NULL && es_strcasebufcmp(lst->name, (uchar*)name, lenName))
		lst = lst->next;
	return lst;
}


/* check if there are duplicate names inside a nvlst and emit
 * an error message, if so.
 */
static inline void
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


static inline int
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


static inline int
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

static inline int
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
static inline int
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

static inline int
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
		dbgprintf("param '%s': uid %d obtained for group '%s'\n",
		   param->name, (int) resultBuf->gr_gid, cstr);
		r = 1;
	}
	free(cstr);
	return r;
}

static inline int
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
		dbgprintf("param '%s': uid %d obtained for user '%s'\n",
		   param->name, (int) resultBuf->pw_uid, cstr);
		r = 1;
	}
	free(cstr);
	return r;
}

/* note: we support all integer formats that es_str2num support,
 * so hex and octal representations are also valid.
 */
static inline int
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

static inline int
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

static inline int
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

static inline int
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

static inline int
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

static inline int
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
static inline int
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
		r = doGetPositiveInt(valnode, param, val);
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
		dbgprintf("error: invalid param type\n");
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
		dbgprintf("nvlstGetParams: invalid param block version "
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
		if((valnode = nvlstFindNameCStr(lst, param->name)) == NULL)
			continue;
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
		dbgprintf("nvlstGetParams: invalid param block version "
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
cnfparamsPrint(struct cnfparamblk *params, struct cnfparamvals *vals)
{
	int i;
	char *cstr;

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

	if((o = malloc(sizeof(struct nvlst))) != NULL) {
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


/* ensure that retval is a number; if string is no number,
 * try to convert it to one. The semantics from es_str2num()
 * are used (bSuccess tells if the conversion went well or not).
 */
static long long
var2Number(struct var *r, int *bSuccess)
{
	long long n;
	if(r->datatype == 'S') {
		n = es_str2num(r->d.estr, bSuccess);
	} else {
		if(r->datatype == 'J') {
			n = (r->d.json == NULL) ? 0 : json_object_get_int(r->d.json);
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
static inline es_str_t *
var2String(struct var *r, int *bMustFree)
{
	es_str_t *estr;
	char *cstr;
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

static uchar*
var2CString(struct var *r, int *bMustFree)
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

rsRetVal
doExtractField(uchar *str, uchar delim, int matchnbr, uchar **resstr)
{
	int iCurrFld;
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
	dbgprintf("field() field requested %d, field found %d\n", matchnbr, iCurrFld);
	
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
		CHKmalloc(pBuf = MALLOC((iLen + 1) * sizeof(char)));
		/* now copy */
		memcpy(pBuf, pFld, iLen);
		pBuf[iLen] = '\0'; /* terminate it */
		if(*(pFldEnd+1) != '\0')
			++pFldEnd; /* OK, skip again over delimiter char */
		*resstr = pBuf;
	} else {
		ABORT_FINALIZE(RS_RET_FIELD_NOT_FOUND);
	}
finalize_it:
	RETiRet;
}

/* Perform a function call. This has been moved out of cnfExprEval in order
 * to keep the code small and easier to maintain.
 */
static inline void
doFuncCall(struct cnffunc *func, struct var *ret, void* usrptr)
{
	char *fname;
	char *envvar;
	int bMustFree;
	es_str_t *estr;
	char *str;
	uchar *resStr;
	int retval;
	struct var r[CNFFUNC_MAX_ARGS];
	int delim;
	int matchnbr;
	struct funcData_prifilt *pPrifilt;
	rsRetVal localRet;

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
			estr = var2String(&r[0], &bMustFree);
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
		estr = var2String(&r[0], &bMustFree);
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
		estr = var2String(&r[0], &bMustFree);
		if(!bMustFree) /* let caller handle that M) */
			estr = es_strdup(estr);
		es_tolower(estr);
		ret->datatype = 'S';
		ret->d.estr = estr;
		if(r[0].datatype == 'S') es_deleteStr(r[0].d.estr);
		break;
	case CNFFUNC_CSTR:
		cnfexprEval(func->expr[0], &r[0], usrptr);
		estr = var2String(&r[0], &bMustFree);
		if(!bMustFree) /* let caller handle that M) */
			estr = es_strdup(estr);
		ret->datatype = 'S';
		ret->d.estr = estr;
		if(r[0].datatype == 'S') es_deleteStr(r[0].d.estr);
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
			if(r[0].datatype == 'S') es_deleteStr(r[0].d.estr);
		}
		ret->datatype = 'N';
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
		if(r[0].datatype == 'S') es_deleteStr(r[0].d.estr);
		break;
	case CNFFUNC_FIELD:
		cnfexprEval(func->expr[0], &r[0], usrptr);
		cnfexprEval(func->expr[1], &r[1], usrptr);
		cnfexprEval(func->expr[2], &r[2], usrptr);
		str = (char*) var2CString(&r[0], &bMustFree);
		delim = var2Number(&r[1], NULL);
		matchnbr = var2Number(&r[2], NULL);
		localRet = doExtractField((uchar*)str, (char) delim, matchnbr, &resStr);
		if(localRet == RS_RET_OK) {
			ret->d.estr = es_newStrFromCStr((char*)resStr, strlen((char*)resStr));
			free(resStr);
		} else if(localRet == RS_RET_OK) {
			ret->d.estr = es_newStrFromCStr("***FIELD NOT FOUND***",
					sizeof("***FIELD NOT FOUND***")-1);
		} else {
			ret->d.estr = es_newStrFromCStr("***ERROR in field() FUNCTION***",
					sizeof("***ERROR in field() FUNCTION***")-1);
		}
		ret->datatype = 'S';
		if(bMustFree) free(str);
		if(r[0].datatype == 'S') es_deleteStr(r[0].d.estr);
		if(r[1].datatype == 'S') es_deleteStr(r[1].d.estr);
		if(r[2].datatype == 'S') es_deleteStr(r[2].d.estr);
		break;
	case CNFFUNC_PRIFILT:
		pPrifilt = (struct funcData_prifilt*) func->funcdata;
		if( (pPrifilt->pmask[((msg_t*)usrptr)->iFacility] == TABLE_NOPRI) ||
		   ((pPrifilt->pmask[((msg_t*)usrptr)->iFacility]
			    & (1<<((msg_t*)usrptr)->iSeverity)) == 0) )
			ret->d.n = 0;
		else
			ret->d.n = 1;
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

static inline void
evalVar(struct cnfvar *var, void *usrptr, struct var *ret)
{
	rsRetVal localRet;
	es_str_t *estr;
	struct json_object *json;

	if(var->name[0] == '$' && var->name[1] == '!') {
		/* TODO: unify string libs */
		estr = es_newStrFromBuf(var->name+1, strlen(var->name)-1);
		localRet = msgGetCEEPropJSON((msg_t*)usrptr, estr, &json);
		es_deleteStr(estr);
		ret->datatype = 'J';
		ret->d.json = (localRet == RS_RET_OK) ? json : NULL;
	} else {
		ret->datatype = 'S';
		ret->d.estr = cnfGetVar(var->name, usrptr);
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
evalStrArrayCmp(es_str_t *estr_l, struct cnfarray* ar, int cmpop)
{
	int i;
	int r = 0;
	for(i = 0 ; (r == 0) && (i < ar->nmemb) ; ++i) {
		switch(cmpop) {
		case CMP_EQ:
			r = es_strcmp(estr_l, ar->arr[i]) == 0;
			break;
		case CMP_NE:
			r = es_strcmp(estr_l, ar->arr[i]) != 0;
			break;
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
	return r;
}

#define FREE_BOTH_RET \
		if(r.datatype == 'S') es_deleteStr(r.d.estr); \
		if(l.datatype == 'S') es_deleteStr(l.d.estr)

#define COMP_NUM_BINOP(x) \
	cnfexprEval(expr->l, &l, usrptr); \
	cnfexprEval(expr->r, &r, usrptr); \
	ret->datatype = 'N'; \
	ret->d.n = var2Number(&l, &convok_l) x var2Number(&r, &convok_r); \
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
		if(expr->r->nodetype != 'S' && expr->r->nodetype != 'A' && r.datatype == 'S') es_deleteStr(r.d.estr);  \
		if(bMustFree2) es_deleteStr(estr_l);  \
		if(l.datatype == 'S') es_deleteStr(l.d.estr)

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
cnfexprEval(struct cnfexpr *expr, struct var *ret, void* usrptr)
{
	struct var r, l; /* memory for subexpression results */
	es_str_t *estr_r, *estr_l;
	int convok_r, convok_l;
	int bMustFree, bMustFree2;
	long long n_r, n_l;

	dbgprintf("eval expr %p, type '%s'\n", expr, tokenToString(expr->nodetype));
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
				if(r.datatype == 'S') es_deleteStr(r.d.estr);
			}
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
			if(r.datatype == 'S') es_deleteStr(r.d.estr);
		}
		if(l.datatype == 'S') es_deleteStr(l.d.estr);
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
			if(r.datatype == 'S') es_deleteStr(r.d.estr); 
		}
		if(l.datatype == 'S') es_deleteStr(l.d.estr);
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
			if(r.datatype == 'S') es_deleteStr(r.d.estr); 
		} else {
			ret->d.n = 0ll;
		}
		if(l.datatype == 'S') es_deleteStr(l.d.estr);
		break;
	case NOT:
		cnfexprEval(expr->r, &r, usrptr);
		ret->datatype = 'N';
		ret->d.n = !var2Number(&r, &convok_r);
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
		COMP_NUM_BINOP(/);
		break;
	case '%':
		COMP_NUM_BINOP(%);
		break;
	case 'M':
		cnfexprEval(expr->r, &r, usrptr);
		ret->datatype = 'N';
		ret->d.n = -var2Number(&r, &convok_r);
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

static inline void
cnffuncDestruct(struct cnffunc *func)
{
	unsigned short i;

	for(i = 0 ; i < func->nParams ; ++i) {
		cnfexprDestruct(func->expr[i]);
	}
	/* some functions require special destruction */
	switch(func->fID) {
		case CNFFUNC_RE_MATCH:
			if(func->funcdata != NULL)
				regexp.regfree(func->funcdata);
			break;
		default:break;
	}
	free(func->funcdata);
	free(func->fname);
}

/* Destruct an expression and all sub-expressions contained in it.
 */
void
cnfexprDestruct(struct cnfexpr *expr)
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
cnfexprEvalBool(struct cnfexpr *expr, void *usrptr)
{
	int convok;
	struct var ret;
	cnfexprEval(expr, &ret, usrptr);
	return var2Number(&ret, &convok);
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
		break;
	}
}
void
cnfstmtPrint(struct cnfstmt *root, int indent)
{
	struct cnfstmt *stmt;
	char *cstr;
	//dbgprintf("stmt %p, indent %d, type '%c'\n", expr, indent, expr->nodetype);
	for(stmt = root ; stmt != NULL ; stmt = stmt->next) {
		switch(stmt->nodetype) {
		case S_NOP:
			doIndent(indent); dbgprintf("NOP\n");
			break;
		case S_STOP:
			doIndent(indent); dbgprintf("STOP\n");
			break;
		case S_CALL:
			cstr = es_str2cstr(stmt->d.s_call.name, NULL);
			doIndent(indent); dbgprintf("CALL [%s]\n", cstr);
			free(cstr);
			break;
		case S_ACT:
			doIndent(indent); dbgprintf("ACTION %p [%s]\n", stmt->d.act, stmt->printable);
			break;
		case S_IF:
			doIndent(indent); dbgprintf("IF\n");
			cnfexprPrint(stmt->d.s_if.expr, indent+1);
			doIndent(indent); dbgprintf("THEN\n");
			cnfstmtPrint(stmt->d.s_if.t_then, indent+1);
			if(stmt->d.s_if.t_else != NULL) {
				doIndent(indent); dbgprintf("ELSE\n");
				cnfstmtPrint(stmt->d.s_if.t_else, indent+1);
			}
			doIndent(indent); dbgprintf("END IF\n");
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
		case S_PRIFILT:
			doIndent(indent); dbgprintf("PRIFILT '%s'\n", stmt->printable);
			pmaskPrint(stmt->d.s_prifilt.pmask, indent);
			cnfstmtPrint(stmt->d.s_prifilt.t_then, indent+1);
			if(stmt->d.s_prifilt.t_else != NULL) {
				doIndent(indent); dbgprintf("ELSE\n");
				cnfstmtPrint(stmt->d.s_prifilt.t_else, indent+1);
			}
			doIndent(indent); dbgprintf("END PRIFILT\n");
			break;
		case S_PROPFILT:
			doIndent(indent); dbgprintf("PROPFILT\n");
			doIndent(indent); dbgprintf("\tProperty.: '%s'\n",
				propIDToName(stmt->d.s_propfilt.propID));
			if(stmt->d.s_propfilt.propName != NULL) {
				cstr = es_str2cstr(stmt->d.s_propfilt.propName, NULL);
				doIndent(indent);
				dbgprintf("\tCEE-Prop.: '%s'\n", cstr);
				free(cstr);
			}
			doIndent(indent); dbgprintf("\tOperation: ");
			if(stmt->d.s_propfilt.isNegated)
				dbgprintf("NOT ");
			dbgprintf("'%s'\n", getFIOPName(stmt->d.s_propfilt.operation));
			if(stmt->d.s_propfilt.pCSCompValue != NULL) {
				doIndent(indent); dbgprintf("\tValue....: '%s'\n",
				       rsCStrGetSzStrNoNULL(stmt->d.s_propfilt.pCSCompValue));
			}
			doIndent(indent); dbgprintf("THEN\n");
			cnfstmtPrint(stmt->d.s_propfilt.t_then, indent+1);
			doIndent(indent); dbgprintf("END PROPFILT\n");
			break;
		default:
			dbgprintf("error: unknown stmt type %u\n",
				(unsigned) stmt->nodetype);
			break;
		}
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
cnfarrayAdd(struct cnfarray *ar, es_str_t *val)
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

void
cnfstmtDestruct(struct cnfstmt *root)
{
	struct cnfstmt *stmt, *todel;
	for(stmt = root ; stmt != NULL ; ) {
		switch(stmt->nodetype) {
		case S_NOP:
		case S_STOP:
			break;
		case S_CALL:
			es_deleteStr(stmt->d.s_call.name);
			break;
		case S_ACT:
			actionDestruct(stmt->d.act);
			break;
		case S_IF:
			cnfexprDestruct(stmt->d.s_if.expr);
			if(stmt->d.s_if.t_then != NULL) {
				cnfstmtDestruct(stmt->d.s_if.t_then);
			}
			if(stmt->d.s_if.t_else != NULL) {
				cnfstmtDestruct(stmt->d.s_if.t_else);
			}
			break;
		case S_SET:
			free(stmt->d.s_set.varname);
			cnfexprDestruct(stmt->d.s_set.expr);
			break;
		case S_UNSET:
			free(stmt->d.s_set.varname);
			break;
		case S_PRIFILT:
			cnfstmtDestruct(stmt->d.s_prifilt.t_then);
			cnfstmtDestruct(stmt->d.s_prifilt.t_else);
			break;
		case S_PROPFILT:
			if(stmt->d.s_propfilt.propName != NULL)
				es_deleteStr(stmt->d.s_propfilt.propName);
			if(stmt->d.s_propfilt.regex_cache != NULL)
				rsCStrRegexDestruct(&stmt->d.s_propfilt.regex_cache);
			if(stmt->d.s_propfilt.pCSCompValue != NULL)
				cstrDestruct(&stmt->d.s_propfilt.pCSCompValue);
			cnfstmtDestruct(stmt->d.s_propfilt.t_then);
			break;
		default:
			dbgprintf("error: unknown stmt type during destruct %u\n",
				(unsigned) stmt->nodetype);
			break;
		}
		free(stmt->printable);
		todel = stmt;
		stmt = stmt->next;
		free(todel);
	}
}

struct cnfstmt *
cnfstmtNewSet(char *var, struct cnfexpr *expr)
{
	struct cnfstmt* cnfstmt;
	if((cnfstmt = cnfstmtNew(S_SET)) != NULL) {
		cnfstmt->d.s_set.varname = (uchar*) var;
		cnfstmt->d.s_set.expr = expr;
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
	rsRetVal lRet;
	if((cnfstmt = cnfstmtNew(S_PROPFILT)) != NULL) {
		cnfstmt->printable = (uchar*)propfilt;
		cnfstmt->d.s_propfilt.t_then = t_then;
		cnfstmt->d.s_propfilt.propName = NULL;
		cnfstmt->d.s_propfilt.regex_cache = NULL;
		cnfstmt->d.s_propfilt.pCSCompValue = NULL;
		lRet = DecodePropFilter((uchar*)propfilt, cnfstmt);
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
	if(localRet != RS_RET_OK && localRet != RS_RET_OK_WARN) {
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
static inline void
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
static inline struct cnfexpr*
cnfexprOptimize_CMP_severity_facility(struct cnfexpr *expr)
{
	struct cnffunc *func;

	if(!strcmp("$syslogseverity", ((struct cnfvar*)expr->l)->name)) {
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
	} else if(!strcmp("$syslogfacility", ((struct cnfvar*)expr->l)->name)) {
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
	return expr;
}

/* optimize a comparison with a variable as left-hand operand
 * NOTE: Currently support CMP_EQ, CMP_NE only and code NEEDS 
 *       TO BE CHANGED for other comparisons!
 */
static inline struct cnfexpr*
cnfexprOptimize_CMP_var(struct cnfexpr *expr)
{
	struct cnffunc *func;

	if(!strcmp("$syslogfacility-text", ((struct cnfvar*)expr->l)->name)) {
		if(expr->r->nodetype == 'S') {
			char *cstr = es_str2cstr(((struct cnfstringval*)expr->r)->estr, NULL);
			int fac = decodeSyslogName((uchar*)cstr, syslogFacNames);
			if(fac == -1) {
				parser_errmsg("invalid facility '%s', expression will always "
					      "evaluate to FALSE", cstr);
			} else {
				/* we can acutally optimize! */
				DBGPRINTF("optimizer: change comparison OP to FUNC prifilt()\n");
				func = cnffuncNew_prifilt(fac);
				if(expr->nodetype == CMP_NE)
					prifiltInvert(func->funcdata);
				cnfexprDestruct(expr);
				expr = (struct cnfexpr*) func;
			}
			free(cstr);
		}
	} else if(!strcmp("$syslogseverity-text", ((struct cnfvar*)expr->l)->name)) {
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

static inline struct cnfexpr*
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

static inline struct cnfexpr*
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

/* (recursively) optimize an expression */
struct cnfexpr*
cnfexprOptimize(struct cnfexpr *expr)
{
	long long ln, rn;
	struct cnfexpr *exprswap;

	dbgprintf("optimize expr %p, type '%s'\n", expr, tokenToString(expr->nodetype));
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
			((struct cnfnumval*)expr)->val = ln / rn;
		}
		break;
	case '%':
		if(getConstNumber(expr, &ln, &rn))  {
			expr->nodetype = 'N';
			((struct cnfnumval*)expr)->val = ln % rn;
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
		} else if(expr->l->nodetype == 'V') {
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
static inline struct cnfstmt *
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


static inline void
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

static inline void
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
		cnfstmtDestruct(stmt->d.s_prifilt.t_else);
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
	DBGPRINTF("CALL obtained ruleset ptr %p for ruleset %s\n", pRuleset, rsName);
	stmt->d.s_call.stmt = pRuleset->root;
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
dbgprintf("RRRR: stmtOptimize: stmt %p, nodetype %u\n", stmt, stmt->nodetype);
		switch(stmt->nodetype) {
		case S_IF:
			cnfstmtOptimizeIf(stmt);
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
		case S_NOP:
			DBGPRINTF("optimizer error: we see a NOP, how come?\n");
			break;
		default:
			dbgprintf("error: unknown stmt type %u during optimizer run\n",
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
	} else if(!es_strbufcmp(fname, (unsigned char*)"re_match", sizeof("re_match") - 1)) {
		if(nParams != 2) {
			parser_errmsg("number of parameters for re_match() must be two "
				      "but is %d.", nParams);
			return CNFFUNC_INVALID;
		}
		return CNFFUNC_RE_MATCH;
	} else if(!es_strbufcmp(fname, (unsigned char*)"field", sizeof("field") - 1)) {
		if(nParams != 3) {
			parser_errmsg("number of parameters for field() must be three "
				      "but is %d.", nParams);
			return CNFFUNC_INVALID;
		}
		return CNFFUNC_FIELD;
	} else if(!es_strbufcmp(fname, (unsigned char*)"prifilt", sizeof("prifilt") - 1)) {
		if(nParams != 1) {
			parser_errmsg("number of parameters for prifilt() must be one "
				      "but is %d.", nParams);
			return CNFFUNC_INVALID;
		}
		return CNFFUNC_PRIFILT;
	} else {
		return CNFFUNC_INVALID;
	}
}


static inline rsRetVal
initFunc_re_match(struct cnffunc *func)
{
	rsRetVal localRet;
	char *regex = NULL;
	regex_t *re;
	DEFiRet;

	func->funcdata = NULL;
	if(func->expr[1]->nodetype != 'S') {
		parser_errmsg("param 2 of re_match() must be a constant string");
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


static inline rsRetVal
initFunc_prifilt(struct cnffunc *func)
{
	struct funcData_prifilt *pData;
	uchar *cstr;
	DEFiRet;

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
				/* need to compile the regexp in param 2, so this MUST be a constant */
				initFunc_re_match(func);
				break;
			case CNFFUNC_PRIFILT:
				initFunc_prifilt(func);
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

	if((func = malloc(sizeof(struct cnffunc))) != NULL) {
		func->nodetype = 'F';
		func->fname = es_newStrFromCStr("prifilt", sizeof("prifilt")-1);
		func->nParams = 0;
		func->fID = CNFFUNC_PRIFILT;
		func->funcdata = calloc(1, sizeof(struct funcData_prifilt));
		((struct funcData_prifilt *)func->funcdata)->pmask[fac >> 3] = TABLE_ALLPRI;
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
	unsigned i;
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
	result = glob(finalName, GLOB_MARK | GLOB_NOMAGIC, NULL, &cfgFiles);

	/* Silently ignore wildcards that match nothing */
	if(result == GLOB_NOMATCH) {
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

	for(i = 0; i < cfgFiles.gl_pathc; i++) {
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
			dbgprintf("requested to include config file '%s'\n", cfgFile);
			cnfSetLexFile(cfgFile);
		} else if(S_ISDIR(fileInfo.st_mode)) { /* config directory */
			dbgprintf("requested to include directory '%s'\n", cfgFile);
			cnfDoInclude(cfgFile);
		} else {
			dbgprintf("warning: unable to process IncludeConfig directive '%s'\n", cfgFile);
		}
	}

	globfree(&cfgFiles);
	return 0;
}

void
varDelete(struct var *v)
{
	switch(v->datatype) {
	case 'S':
		es_deleteStr(v->d.estr);
		break;
	case 'A':
		cnfarrayContentDestruct(v->d.ar);
		free(v->d.ar);
		break;
	default:break;
	}
}

void
cnfparamvalsDestruct(struct cnfparamvals *paramvals, struct cnfparamblk *blk)
{
	int i;
	for(i = 0 ; i < blk->nParams ; ++i) {
		if(paramvals[i].bUsed) {
			varDelete(&paramvals[i].val);
		}
	}
	free(paramvals);
}

/* find the index (or -1!) for a config param by name. This is used to 
 * address the parameter array. Of course, we could use with static
 * indices, but that would create some extra bug potential. So we
 * resort to names. As we do this only during the initial config parsing
 * stage the (considerable!) extra overhead is OK. -- rgerhards, 2011-07-19
 */
int
cnfparamGetIdx(struct cnfparamblk *params, char *name)
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
cstrPrint(char *text, es_str_t *estr)
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
static inline int
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
static inline void
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
