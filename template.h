/* This is the header for template processing code of rsyslog.
 * Please see syslogd.c for license information.
 * This code is placed under the GPL.
 * begun 2004-11-17 rgerhards
 */

#ifndef	TEMPLATE_H_INCLUDED
#define	TEMPLATE_H_INCLUDED 1


#include "stringbuf.h"

#ifdef FEATURE_REGEXP
/* Include regular expressions */
#include <regex.h>
#endif

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
		      tplFmtRFC3164Date = 2, tplFmtRFC3339Date = 3 };
enum tplFormatCaseConvTypes { tplCaseConvNo = 0, tplCaseConvUpper = 1, tplCaseConvLower = 2 };

#include "msg.h"

/* a specific parse entry */
struct templateEntry {
	struct templateEntry *pNext;
	enum EntryTypes eEntryType;
	union {
		struct {
			char *pConstant;	/* pointer to constant value */
			int iLenConstant;	/* its length */
		} constant;
		struct {
			char *pPropRepl;	/* pointer to property replacer string */
			unsigned iFromPos;	/* for partial strings only chars from this position ... */
			unsigned iToPos;	/* up to that one... */
#ifdef FEATURE_REGEXP
			regex_t re;	/* APR: this is the regular expression */
			unsigned has_regex;
#endif
			unsigned has_fields; /* support for field-counting: field to extract */
			unsigned char field_delim; /* support for field-counting: field delemiter char */
			enum tplFormatTypes eDateFormat;
			enum tplFormatCaseConvTypes eCaseConv;
			struct { 		/* bit fields! */
				unsigned bDropCC: 1;		/* drop control characters? */
				unsigned bSpaceCC: 1;		/* change control characters to spaceescape? */
				unsigned bEscapeCC: 1;		/* escape control characters? */
				unsigned bDropLastLF: 1;	/* drop last LF char in msg (PIX!) */
			} options;		/* options as bit fields */
		} field;
	} data;
};

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
uchar *tplToString(struct template *pTpl, msg_t *pMsg);
void doSQLEscape(char **pp, size_t *pLen, unsigned short *pbMustBeFreed, int escapeMode);

#endif /* #ifndef TEMPLATE_H_INCLUDED */
/*
 * vi:set ai:
 */
