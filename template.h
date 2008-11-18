/* This is the header for template processing code of rsyslog.
 * Please see syslogd.c for license information.
 * begun 2004-11-17 rgerhards
 *
 * Copyright (C) 2004 by Rainer Gerhards and Adiscon GmbH
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

#ifndef	TEMPLATE_H_INCLUDED
#define	TEMPLATE_H_INCLUDED 1

#include "regexp.h"
#include "stringbuf.h"

struct template {
	struct template *pNext;
	char *pszName;
	int iLenName;
	int tpenElements; /* number of elements in templateEntry list */
	struct templateEntry *pEntryRoot;
	struct templateEntry *pEntryLast;
	char optFormatForSQL;	/* in text fields,  0 - do not escape,
	                         * 1 - escape quotes by double quotes,
				 * 2 - escape "the MySQL way" 
				 */
	/* following are options. All are 0/1 defined (either on or off).
	 * we use chars because they are faster than bit fields and smaller
	 * than short...
	 */
};

enum EntryTypes { UNDEFINED = 0, CONSTANT = 1, FIELD = 2 };
enum tplFormatTypes { tplFmtDefault = 0, tplFmtMySQLDate = 1,
                      tplFmtRFC3164Date = 2, tplFmtRFC3339Date = 3, tplFmtPgSQLDate = 4,
		      tplFmtSecFrac = 5};
enum tplFormatCaseConvTypes { tplCaseConvNo = 0, tplCaseConvUpper = 1, tplCaseConvLower = 2 };

#include "msg.h"

/* a specific parse entry */
struct templateEntry {
	struct templateEntry *pNext;
	enum EntryTypes eEntryType;
	union {
		struct {
			uchar *pConstant;	/* pointer to constant value */
			int iLenConstant;	/* its length */
		} constant;
		struct {
			uchar *pPropRepl;	/* pointer to property replacer string */
			unsigned iFromPos;	/* for partial strings only chars from this position ... */
			unsigned iToPos;	/* up to that one... */
#ifdef FEATURE_REGEXP
			regex_t re;	/* APR: this is the regular expression */
			short has_regex;
			short iMatchToUse;/* which match should be obtained (10 max) */
			short iSubMatchToUse;/* which submatch should be obtained (10 max) */
			enum {
				TPL_REGEX_BRE = 0, /* posix BRE */
				TPL_REGEX_ERE = 1  /* posix ERE */
			} typeRegex;
			enum {
				TPL_REGEX_NOMATCH_USE_DFLTSTR = 0, /* use the (old style) default "**NO MATCH**" string */
				TPL_REGEX_NOMATCH_USE_BLANK = 1, /* use a blank string */
				TPL_REGEX_NOMATCH_USE_WHOLE_FIELD = 2, /* use the full field contents that we were searching in*/
				TPL_REGEX_NOMATCH_USE_ZERO = 3 /* use  0 (useful for numerical values) */
			}  nomatchAction;	/**< what to do if we do not have a match? */
			
#endif
			unsigned has_fields; /* support for field-counting: field to extract */
			unsigned char field_delim; /* support for field-counting: field delemiter char */
			int field_expand;	/* use multiple instances of the field delimiter as a single one? */

			enum tplFormatTypes eDateFormat;
			enum tplFormatCaseConvTypes eCaseConv;
			struct { 		/* bit fields! */
				unsigned bDropCC: 1;		/* drop control characters? */
				unsigned bSpaceCC: 1;		/* change control characters to spaceescape? */
				unsigned bEscapeCC: 1;		/* escape control characters? */
				unsigned bDropLastLF: 1;	/* drop last LF char in msg (PIX!) */
				unsigned bSecPathDrop: 1;		/* drop slashes, replace dots, empty string */
				unsigned bSecPathReplace: 1;		/* replace slashes, replace dots, empty string */
				unsigned bSPIffNo1stSP: 1;		/* replace slashes, replace dots, empty string */
			} options;		/* options as bit fields */
		} field;
	} data;
};


/* interfaces */
BEGINinterface(tpl) /* name must also be changed in ENDinterface macro! */
ENDinterface(tpl)
#define tplCURR_IF_VERSION 1 /* increment whenever you change the interface structure! */

/* prototypes */
PROTOTYPEObj(tpl);


struct template* tplConstruct(void);
struct template *tplAddLine(char* pName, unsigned char** pRestOfConfLine);
struct template *tplFind(char *pName, int iLenName);
int tplGetEntryCount(struct template *pTpl);
void tplDeleteAll(void);
void tplDeleteNew(void);
void tplPrintList(void);
void tplLastStaticInit(struct template *tpl);
/* note: if a compiler warning for undefined type tells you to look at this
 * code line below, the actual cause is that you currently MUST include template.h
 * BEFORE msg.h, even if your code file does not actually need it.
 * rgerhards, 2007-08-06
 */
rsRetVal tplToString(struct template *pTpl, msg_t *pMsg, uchar** ppSz);
rsRetVal doSQLEscape(uchar **pp, size_t *pLen, unsigned short *pbMustBeFreed, int escapeMode);

rsRetVal templateInit();

#endif /* #ifndef TEMPLATE_H_INCLUDED */
/* vim:set ai:
 */
