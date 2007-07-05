/* This is the template processing code of rsyslog.
 * Please see syslogd.c for license information.
 * This code is placed under the GPL.
 * begun 2004-11-17 rgerhards
 */
#ifdef __FreeBSD__
#define	BSD
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include "rsyslog.h"
#include "stringbuf.h"
#include "template.h"
#include "syslogd.h"

static struct template *tplRoot = NULL;	/* the root of the template list */
static struct template *tplLast = NULL;	/* points to the last element of the template list */

/* Constructs a template entry object. Returns pointer to it
 * or NULL (if it fails). Pointer to associated template list entry 
 * must be provided.
 */
struct templateEntry* tpeConstruct(struct template *pTpl)
{
	struct templateEntry *pTpe;

	assert(pTpl != NULL);

	if((pTpe = calloc(1, sizeof(struct templateEntry))) == NULL)
		return NULL;
	
	/* basic initialization is done via calloc() - need to
	 * initialize only values != 0. */

	if(pTpl->pEntryLast == NULL){
		/* we are the first element! */
		pTpl->pEntryRoot = pTpl->pEntryLast  = pTpe;
	} else {
		pTpl->pEntryLast->pNext = pTpe;
		pTpl->pEntryLast  = pTpe;
	}
	pTpl->tpenElements++;

	return(pTpe);
}


/* Constructs a template list object. Returns pointer to it
 * or NULL (if it fails).
 */
struct template* tplConstruct(void)
{
	struct template *pTpl;
	if((pTpl = calloc(1, sizeof(struct template))) == NULL)
		return NULL;
	
	/* basic initialisation is done via calloc() - need to
	 * initialize only values != 0. */

	if(tplLast == NULL)	{
		/* we are the first element! */
		tplRoot = tplLast = pTpl;
	} else {
		tplLast->pNext = pTpl;
		tplLast = pTpl;
	}

	return(pTpl);
}


/* helper to tplAddLine. Parses a constant and generates
 * the necessary structure.
 * returns: 0 - ok, 1 - failure
 */
static int do_Constant(unsigned char **pp, struct template *pTpl)
{
	register unsigned char *p;
	rsCStrObj *pStrB;
	struct templateEntry *pTpe;
	int i;

	assert(pp != NULL);
	assert(*pp != NULL);
	assert(pTpl != NULL);

	p = *pp;

	if((pStrB = rsCStrConstruct()) == NULL)
		 return 1;
	rsCStrSetAllocIncrement(pStrB, 32);
	/* process the message and expand escapes
	 * (additional escapes can be added here if needed)
	 */
	while(*p && *p != '%' && *p != '\"') {
		if(*p == '\\') {
			switch(*++p) {
				case '\0':	
					/* the best we can do - it's invalid anyhow... */
					rsCStrAppendChar(pStrB, *p);
					break;
				case 'n':
					rsCStrAppendChar(pStrB, '\n');
					++p;
					break;
				case 'r':
					rsCStrAppendChar(pStrB, '\r');
					++p;
					break;
				case '\\':
					rsCStrAppendChar(pStrB, '\\');
					++p;
					break;
				case '%':
					rsCStrAppendChar(pStrB, '%');
					++p;
					break;
				case '0': /* numerical escape sequence */
				case '1':
				case '2':
				case '3':
				case '4':
				case '5':
				case '6':
				case '7':
				case '8':
				case '9':
					i = 0;
					while(*p && isdigit((int)*p)) {
						i = i * 10 + *p++ - '0';
					}
					rsCStrAppendChar(pStrB, i);
					break;
				default:
					rsCStrAppendChar(pStrB, *p++);
					break;
			}
		}
		else
			rsCStrAppendChar(pStrB, *p++);
	}

	if((pTpe = tpeConstruct(pTpl)) == NULL) {
		/* OK, we are out of luck. Let's invalidate the
		 * entry and that's it.
		 * TODO: add panic message once we have a mechanism for this
		 */
		pTpe->eEntryType = UNDEFINED;
		return 1;
	}
	pTpe->eEntryType = CONSTANT;
	rsCStrFinish(pStrB);
	/* We obtain the length from the counted string object
	 * (before we delete it). Later we might take additional
	 * benefit from the counted string object.
	 * 2005-09-09 rgerhards
	 */
	pTpe->data.constant.iLenConstant = rsCStrLen(pStrB);
	pTpe->data.constant.pConstant = (char*) rsCStrConvSzStrAndDestruct(pStrB);

	*pp = p;

	return 0;
}


/* Helper to do_Parameter(). This parses the formatting options
 * specified in a template variable. It returns the passed-in pointer
 * updated to the next processed character.
 */
static void doOptions(unsigned char **pp, struct templateEntry *pTpe)
{
	register unsigned char *p;
	unsigned char Buf[64];
	int i;

	assert(pp != NULL);
	assert(*pp != NULL);
	assert(pTpe != NULL);

	p = *pp;

	while(*p && *p != '%') {
		/* outer loop - until end of options */
		i = 0;
		while((i < sizeof(Buf) / sizeof(char)) &&
		      *p && *p != '%' && *p != ',') {
			/* inner loop - until end of ONE option */
			Buf[i++] = tolower((int)*p);
			++p;
		}
		Buf[i] = '\0'; /* terminate */
		/* check if we need to skip oversize option */
		while(*p && *p != '%' && *p != ',')
			++p;	/* just skip */
		/* OK, we got the option, so now lets look what
		 * it tells us...
		 */
		 if(!strcmp((char*)Buf, "date-mysql")) {
			pTpe->data.field.eDateFormat = tplFmtMySQLDate;
		 } else if(!strcmp((char*)Buf, "date-rfc3164")) {
			pTpe->data.field.eDateFormat = tplFmtRFC3164Date;
		 } else if(!strcmp((char*)Buf, "date-rfc3339")) {
			pTpe->data.field.eDateFormat = tplFmtRFC3339Date;
		 } else if(!strcmp((char*)Buf, "lowercase")) {
			pTpe->data.field.eCaseConv = tplCaseConvLower;
		 } else if(!strcmp((char*)Buf, "uppercase")) {
			pTpe->data.field.eCaseConv = tplCaseConvUpper;
		 } else if(!strcmp((char*)Buf, "escape-cc")) {
			pTpe->data.field.options.bEscapeCC = 1;
		 } else if(!strcmp((char*)Buf, "drop-cc")) {
			pTpe->data.field.options.bDropCC = 1;
		 } else if(!strcmp((char*)Buf, "space-cc")) {
			pTpe->data.field.options.bSpaceCC = 1;
		 } else if(!strcmp((char*)Buf, "drop-last-lf")) {
			pTpe->data.field.options.bDropLastLF = 1;
		 } else {
			dprintf("Invalid field option '%s' specified - ignored.\n", Buf);
		 }
	}

	*pp = p;
}


/* helper to tplAddLine. Parses a parameter and generates
 * the necessary structure.
 * returns: 0 - ok, 1 - failure
 */
static int do_Parameter(unsigned char **pp, struct template *pTpl)
{
	unsigned char *p;
	rsCStrObj *pStrB;
	struct templateEntry *pTpe;
	int iNum;	/* to compute numbers */

#ifdef FEATURE_REGEXP
	/* APR: variables for regex */
	int longitud;
	unsigned char *regex_char;
	unsigned char *regex_end;
#endif

	assert(pp != NULL);
	assert(*pp != NULL);
	assert(pTpl != NULL);

	p = (unsigned char*) *pp;

	if((pStrB = rsCStrConstruct()) == NULL)
		 return 1;

	if((pTpe = tpeConstruct(pTpl)) == NULL) {
		/* TODO: add handler */
		dprintf("Could not allocate memory for template parameter!\n");
		return 1;
	}
	pTpe->eEntryType = FIELD;

	while(*p && *p != '%' && *p != ':') {
		rsCStrAppendChar(pStrB, *p++);
	}

	/* got the name*/
	rsCStrFinish(pStrB);
	pTpe->data.field.pPropRepl = (char*) rsCStrConvSzStrAndDestruct(pStrB);

	/* Check frompos, if it has an R, then topos should be a regex */
	if(*p == ':') {
		++p; /* eat ':' */
#ifdef FEATURE_REGEXP
		if (*p == 'R') {
			/* APR: R found! regex alarm ! :) */
			++p;	/* eat ':' */

			if (*p != ':') {
				/* There is something more than an R , this is invalid ! */
				/* Complain on extra characters */
				logerrorSz
				  ("error: invalid character in frompos after \"R\", property: '%%%s'",
				    (char*) *pp);
			} else {
				pTpe->data.field.has_regex = 1;
			}
		} else {
			/* now we fall through the "regular" FromPos code */
#endif /* #ifdef FEATURE_REGEXP */
			if(*p == 'F') {
				/* we have a field counter, so indicate it in the template */
				++p; /* eat 'F' */
				if (*p == ':') {
					/* no delimiter specified, so use the default (HT) */
					pTpe->data.field.has_fields = 1;
					pTpe->data.field.field_delim = 9;
				} else if (*p == ',') {
					++p; /* eat ',' */
					/* configured delimiter follows, so we need to obtain
					 * it. Important: the following number must be the
					 * **DECIMAL** ASCII value of the delimiter character.
					 */
					pTpe->data.field.has_fields = 1;
					if(!isdigit((int)*p)) {
						/* complain and use default */
						logerrorSz
						  ("error: invalid character in frompos after \"F,\", property: '%%%s' - using 9 (HT) as field delimiter",
						    (char*) *pp);
						pTpe->data.field.field_delim = 9;
					} else {
						iNum = 0;
						while(isdigit((int)*p))
							iNum = iNum * 10 + *p++ - '0';
						if(iNum < 0 || iNum > 255) {
							logerrorInt
							  ("error: non-USASCII delimiter character value in template - using 9 (HT) as substitute", iNum);
							pTpe->data.field.field_delim = 9;
						  } else {
							pTpe->data.field.field_delim = iNum;
							}
					}
				} else {
					/* invalid character after F, so we need to reject
					 * this.
					 */
					logerrorSz
					  ("error: invalid character in frompos after \"F\", property: '%%%s'",
					    (char*) *pp);
				}
			} else {
				/* we now have a simple offset in frompos (the previously "normal" case) */
				iNum = 0;
				while(isdigit((int)*p))
					iNum = iNum * 10 + *p++ - '0';
				pTpe->data.field.iFromPos = iNum;
				/* skip to next known good */
				while(*p && *p != '%' && *p != ':') {
					/* TODO: complain on extra characters */
					dprintf("error: extra character in frompos: '%s'\n", p);
					++p;
				}
			}
#ifdef FEATURE_REGEXP
		}
#endif /* #ifdef FEATURE_REGEXP */
	}
	/* check topos  (holds an regex if FromPos is "R"*/
	if(*p == ':') {
		++p; /* eat ':' */

#ifdef FEATURE_REGEXP
		if (pTpe->data.field.has_regex) {

			dprintf("debug: has regex \n");

			/* APR 2005-09 I need the string that represent the regex */
			/* The regex end is: "--end" */
			/* TODO : this is hardcoded and cant be escaped, please change */
			regex_end = (unsigned char*) strstr((char*)p, "--end");
			if (regex_end == NULL) {
				dprintf("error: can not find regex end in: '%s'\n", p);
				pTpe->data.field.has_regex = 0;
			} else {
				/* We get here ONLY if the regex end was found */
				longitud = regex_end - p;
				/* Malloc for the regex string */
				regex_char = (unsigned char *) malloc(longitud + 1);
				if (regex_char == NULL) {
					dprintf
					    ("Could not allocate memory for template parameter!\n");
					pTpe->data.field.has_regex = 0;
					return 1;
					/* TODO: RGer: check if we can recover better... (probably not) */
				}

				/* Get the regex string for compiling later */
				memcpy(regex_char, p, longitud);
				regex_char[longitud] = '\0';

				dprintf("debug: regex detected: '%s'\n", regex_char);

				/* Now i compile the regex */
				/* Remember that the re is an attribute of the Template entry */
				if(regcomp(&(pTpe->data.field.re), (char*) regex_char, 0) != 0) {
					dprintf("error: can not compile regex: '%s'\n", regex_char);
					pTpe->data.field.has_regex = 2;
				}

				/* Finally we move the pointer to the end of the regex
				 * so it aint parsed twice or something weird */
				p = regex_end + 5/*strlen("--end")*/;
				free(regex_char);
			}
		} else if(*p == '$') {
			/* shortcut for "end of message */
			p++; /* eat '$' */
			/* in this case, we do a quick, somewhat dirty but totally
			 * legitimate trick: we simply use a topos that is higher than
			 * potentially ever can happen. The code below checks that no copy
			 * will occur after the end of string, so this is perfectly legal.
			 * rgerhards, 2006-10-17
			 */
			pTpe->data.field.iToPos = 9999999;
		} else {
			/* fallthrough to "regular" ToPos code */
#endif /* #ifdef FEATURE_REGEXP */

			iNum = 0;
			while(isdigit((int)*p))
				iNum = iNum * 10 + *p++ - '0';
			pTpe->data.field.iToPos = iNum;
			/* skip to next known good */
			while(*p && *p != '%' && *p != ':') {
				/* TODO: complain on extra characters */
				dprintf("error: extra character in frompos: '%s'\n", p);
				++p;
			}
#ifdef FEATURE_REGEXP
		}
#endif /* #ifdef FEATURE_REGEXP */
	}

	/* TODO: add more sanity checks. For now, we do the bare minimum */
	if((pTpe->data.field.has_fields == 0) && (pTpe->data.field.iToPos < pTpe->data.field.iFromPos)) {
		iNum = pTpe->data.field.iToPos;
		pTpe->data.field.iToPos = pTpe->data.field.iFromPos;
		pTpe->data.field.iFromPos = iNum;
	}

	/* check options */
	if(*p == ':') {
		++p; /* eat ':' */
		doOptions(&p, pTpe);
	}

	if(*p) ++p; /* eat '%' */

	*pp = p;
	return 0;
}


/* Add a new template line
 * returns pointer to new object if it succeeds, NULL otherwise.
 */
struct template *tplAddLine(char* pName, unsigned char** ppRestOfConfLine)
{
	struct template *pTpl;
 	unsigned char *p;
	int bDone;
	char optBuf[128]; /* buffer for options - should be more than enough... */
	int i;

	assert(pName != NULL);
	assert(ppRestOfConfLine != NULL);

	if((pTpl = tplConstruct()) == NULL)
		return NULL;
	
	pTpl->iLenName = strlen(pName);
	pTpl->pszName = (char*) malloc(sizeof(char) * (pTpl->iLenName + 1));
	if(pTpl->pszName == NULL) {
		dprintf("tplAddLine could not alloc memory for template name!");
		pTpl->iLenName = 0;
		return NULL;
		/* I know - we create a memory leak here - but I deem
		 * it acceptable as it is a) a very small leak b) very
		 * unlikely to happen. rgerhards 2004-11-17
		 */
	}
	memcpy(pTpl->pszName, pName, pTpl->iLenName + 1);

	/* now actually parse the line */
	p = *ppRestOfConfLine;
	assert(p != NULL);

	while(isspace((int)*p))/* skip whitespace */
		++p;
	
	if(*p != '"') {
		dprintf("Template '%s' invalid, does not start with '\"'!\n", pTpl->pszName);
		/* we simply make the template defunct in this case by setting
		 * its name to a zero-string. We do not free it, as this would
		 * require additional code and causes only a very small memory
		 * consumption. Memory is freed, however, in normal operation
		 * and most importantly by HUPing syslogd.
		 */
		*pTpl->pszName = '\0';
		return NULL;
	}
	++p;

	/* we finally go to the actual template string - so let's have some fun... */
	bDone = *p ? 0 : 1;
	while(!bDone) {
		switch(*p) {
			case '\0':
				bDone = 1;
				break;
			case '%': /* parameter */
				++p; /* eat '%' */
				do_Parameter(&p, pTpl);
				break;
			default: /* constant */
				do_Constant(&p, pTpl);
				break;
		}
		if(*p == '"') {/* end of template string? */
			++p;	/* eat it! */
			bDone = 1;
		}
	}
	
	/* we now have the template - let's look at the options (if any)
	 * we process options until we reach the end of the string or 
	 * an error occurs - whichever is first.
	 */
	while(*p) {
		while(isspace((int)*p))/* skip whitespace */
			++p;
		
		if(*p != ',')
			break;
		++p; /* eat ',' */

		while(isspace((int)*p))/* skip whitespace */
			++p;
		
		/* read option word */
		i = 0;
		while(i < sizeof(optBuf) / sizeof(char) - 1
		      && *p && *p != '=' && *p !=',' && *p != '\n') {
			optBuf[i++] = tolower((int)*p);
			++p;
		}
		optBuf[i] = '\0';

		if(*p == '\n')
			++p;

		/* as of now, the no form is nonsense... but I do include
		 * it anyhow... ;) rgerhards 2004-11-22
		 */
		if(!strcmp(optBuf, "stdsql")) {
			pTpl->optFormatForSQL = 2;
		} else if(!strcmp(optBuf, "sql")) {
			pTpl->optFormatForSQL = 1;
		} else if(!strcmp(optBuf, "nosql")) {
			pTpl->optFormatForSQL = 0;
		} else {
			dprintf("Invalid option '%s' ignored.\n", optBuf);
		}
	}

	*ppRestOfConfLine = p;
	return(pTpl);
}


/* Find a template object based on name. Search
 * currently is case-senstive (should we change?).
 * returns pointer to template object if found and
 * NULL otherwise.
 * rgerhards 2004-11-17
 */
struct template *tplFind(char *pName, int iLenName)
{
	struct template *pTpl;

	assert(pName != NULL);

	pTpl = tplRoot;
	while(pTpl != NULL &&
	      !(pTpl->iLenName == iLenName &&
	        !strcmp(pTpl->pszName, pName)
	        ))
		{
			pTpl = pTpl->pNext;
		}
	return(pTpl);
}

/* Destroy the template structure. This is for de-initialization
 * at program end. Everything is deleted.
 * rgerhards 2005-02-22
 * I have commented out dprintfs, because they are not needed for
 * "normal" debugging. Uncomment them, if they are needed.
 * rgerhards, 2007-07-05
 */
void tplDeleteAll(void)
{
	struct template *pTpl, *pTplDel;
	struct templateEntry *pTpe, *pTpeDel;

	pTpl = tplRoot;
	while(pTpl != NULL) {
		/* dprintf("Delete Template: Name='%s'\n ", pTpl->pszName == NULL? "NULL" : pTpl->pszName);*/
		pTpe = pTpl->pEntryRoot;
		while(pTpe != NULL) {
			pTpeDel = pTpe;
			pTpe = pTpe->pNext;
			/*dprintf("\tDelete Entry(%x): type %d, ", (unsigned) pTpeDel, pTpeDel->eEntryType);*/
			switch(pTpeDel->eEntryType) {
			case UNDEFINED:
				dprintf("(UNDEFINED)");
				break;
			case CONSTANT:
				dprintf("(CONSTANT), value: '%s'",
					pTpeDel->data.constant.pConstant);
				free(pTpeDel->data.constant.pConstant);
				break;
			case FIELD:
				dprintf("(FIELD), value: '%s'", pTpeDel->data.field.pPropRepl);
				free(pTpeDel->data.field.pPropRepl);
				break;
			}
			dprintf("\n");
			free(pTpeDel);
		}
		pTplDel = pTpl;
		pTpl = pTpl->pNext;
		if(pTplDel->pszName != NULL)
			free(pTplDel->pszName);
		free(pTplDel);
	}
}


/* Print the template structure. This is more or less a 
 * debug or test aid, but anyhow I think it's worth it...
 */
void tplPrintList(void)
{
	struct template *pTpl;
	struct templateEntry *pTpe;

	pTpl = tplRoot;
	while(pTpl != NULL) {
		dprintf("Template: Name='%s' ", pTpl->pszName == NULL? "NULL" : pTpl->pszName);
		if(pTpl->optFormatForSQL == 1)
			dprintf("[SQL-Format (MySQL)] ");
		else if(pTpl->optFormatForSQL == 2)
			dprintf("[SQL-Format (standard SQL)] ");
		dprintf("\n");
		pTpe = pTpl->pEntryRoot;
		while(pTpe != NULL) {
			dprintf("\tEntry(%x): type %d, ", (unsigned) pTpe, pTpe->eEntryType);
			switch(pTpe->eEntryType) {
			case UNDEFINED:
				dprintf("(UNDEFINED)");
				break;
			case CONSTANT:
				dprintf("(CONSTANT), value: '%s'",
					pTpe->data.constant.pConstant);
				break;
			case FIELD:
				dprintf("(FIELD), value: '%s' ", pTpe->data.field.pPropRepl);
				switch(pTpe->data.field.eDateFormat) {
				case tplFmtDefault:
					break;
				case tplFmtMySQLDate:
					dprintf("[Format as MySQL-Date] ");
					break;
				case tplFmtRFC3164Date:
					dprintf("[Format as RFC3164-Date] ");
					break;
				case tplFmtRFC3339Date:
					dprintf("[Format as RFC3339-Date] ");
					break;
				}
				switch(pTpe->data.field.eCaseConv) {
				case tplCaseConvNo:
					break;
				case tplCaseConvLower:
					dprintf("[Converted to Lower Case] ");
					break;
				case tplCaseConvUpper:
					dprintf("[Converted to Upper Case] ");
					break;
				}
				if(pTpe->data.field.options.bEscapeCC) {
				  	dprintf("[escape control-characters] ");
				}
				if(pTpe->data.field.options.bDropCC) {
				  	dprintf("[drop control-characters] ");
				}
				if(pTpe->data.field.options.bSpaceCC) {
				  	dprintf("[replace control-characters with space] ");
				}
				if(pTpe->data.field.options.bDropLastLF) {
				  	dprintf("[drop last LF in msg] ");
				}
				if(pTpe->data.field.has_fields == 1) {
				  	dprintf("[substring, field #%d only (delemiter %d)] ",
						pTpe->data.field.iToPos, pTpe->data.field.field_delim);
				} else if(pTpe->data.field.iFromPos != 0 ||
				          pTpe->data.field.iToPos != 0) {
				  	dprintf("[substring, from character %d to %d] ",
						pTpe->data.field.iFromPos,
						pTpe->data.field.iToPos);
				}
				break;
			}
			dprintf("\n");
			pTpe = pTpe->pNext;
		}
		pTpl = pTpl->pNext; /* done, go next */
	}
}

int tplGetEntryCount(struct template *pTpl)
{
	assert(pTpl != NULL);
	return(pTpl->tpenElements);
}
/*
 * vi:set ai:
 */
