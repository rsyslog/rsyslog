/* This is the template processing code of rsyslog.
 * Please see syslogd.c for license information.
 * This code is placed under the GPL.
 * begun 2004-11-17 rgerhards
 */
#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include "liblogging-stub.h"
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
	assert(pTpl != NULL);

	struct templateEntry *pTpe;
	if((pTpe = calloc(1, sizeof(struct templateEntry))) == NULL)
		return NULL;
	
	/* basic initialisaion is done via calloc() - need to
	 * initialize only values != 0. */

	if(pTpl->pEntryLast == NULL)
	{ /* we are the first element! */
		pTpl->pEntryRoot = pTpl->pEntryLast  = pTpe;
	}
	else
	{
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
	
	/* basic initialisaion is done via calloc() - need to
	 * initialize only values != 0. */

	if(tplLast == NULL)
	{ /* we are the first element! */
		tplRoot = tplLast = pTpl;
	}
	else
	{
		tplLast->pNext = pTpl;
		tplLast = pTpl;
	}

	return(pTpl);
}


/* helper to tplAddLine. Parses a constant and generates
 * the necessary structure.
 * returns: 0 - ok, 1 - failure
 */
static int do_Constant(char **pp, struct template *pTpl)
{
	register char *p;
	sbStrBObj *pStrB;
	struct templateEntry *pTpe;
	int i;

	assert(pp != NULL);
	assert(*pp != NULL);
	assert(pTpl != NULL);

	p = *pp;

	if((pStrB = sbStrBConstruct()) == NULL)
		 return 1;
	sbStrBSetAllocIncrement(pStrB, 32);
	/* TODO: implement escape characters! */
	while(*p && *p != '%' && *p != '\"') {
		if(*p == '\\') {
			switch(*++p) {
				case '\0':	
					/* the best we can do - it's invalid anyhow... */
					sbStrBAppendChar(pStrB, *p);
					break;
				case 'n':
					sbStrBAppendChar(pStrB, '\n');
					++p;
					break;
				case 'r':
					sbStrBAppendChar(pStrB, '\r');
					++p;
					break;
				case '\\':
					sbStrBAppendChar(pStrB, '\\');
					++p;
					break;
				case '%':
					sbStrBAppendChar(pStrB, '%');
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
					while(*p && isdigit(*p)) {
						i = i * 10 + *p++ - '0';
					}
					sbStrBAppendChar(pStrB, i);
					break;
				default:
					sbStrBAppendChar(pStrB, *p++);
					break;
			}
		}
		else
			sbStrBAppendChar(pStrB, *p++);
	}

	if((pTpe = tpeConstruct(pTpl)) == NULL) {
		/* TODO: add handler */
		return 1;
	}
	pTpe->eEntryType = CONSTANT;
	pTpe->data.constant.pConstant = sbStrBFinish(pStrB);
	/* we need to call strlen() below because of the escape sequneces.
	 * due to them p -*pp is NOT the rigt size!
	 */
	pTpe->data.constant.iLenConstant = strlen(pTpe->data.constant.pConstant);

	*pp = p;

	return 0;
}


/* helper to tplAddLine. Parses a parameter and generates
 * the necessary structure.
 * returns: 0 - ok, 1 - failure
 */
static int do_Parameter(char **pp, struct template *pTpl)
{
	register char *p;
	sbStrBObj *pStrB;
	struct templateEntry *pTpe;

	assert(pp != NULL);
	assert(*pp != NULL);
	assert(pTpl != NULL);

	p = *pp;

	if((pStrB = sbStrBConstruct()) == NULL)
		 return 1;
	while(*p && *p != '%') {
		sbStrBAppendChar(pStrB, *p++);
	}
	if(*p) ++p; /* eat '%' */

	/* now store param */
	if((pTpe = tpeConstruct(pTpl)) == NULL) {
		/* TODO: add handler */
		dprintf("Could not allocate memory for template parameter!\n");
		return 1;
	}
	pTpe->eEntryType = FIELD;
	pTpe->data.field.pPropRepl = sbStrBFinish(pStrB);

	*pp = p;
	return 0;
}


/* Add a new template line
 * returns pointer to new object if it succeeds, NULL otherwise.
 */
struct template *tplAddLine(char* pName, char** ppRestOfConfLine)
{
	struct template *pTpl;
 	char *p;
	int bDone;

	assert(pName != NULL);
	assert(ppRestOfConfLine != NULL);

	if((pTpl = tplConstruct()) == NULL)
		return NULL;
	
	pTpl->iLenName = strlen(pName);
	pTpl->pszName = (char*) malloc(sizeof(char) * pTpl->iLenName);
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

#if 0
	pTpl->iLenTemplate = strlen(pName);
	pTpl->pszTemplate = (char*) malloc(sizeof(char) * pTpl->iLenTemplate);
	if(pTpl->pszTemplate == NULL) {
		/rintf("could not alloc memory Template"); /* TODO: change to dprintf()! */
		free(pTpl->pszName);
		pTpl->pszName = NULL;
		pTpl->iLenName = 0;
		pTpl->iLenTemplate = 0;
		return NULL;
		/* see note on leak above */
	}
#endif

	p = *ppRestOfConfLine;
	assert(p != NULL);

	/* skip whitespace */
	while(isspace(*p))
		++p;
	
	if(*p != '"') {
		dprintf("Template invalid, does not start with '\"'!\n");
		/* TODO: Free mem! */
		return NULL;
	}
	++p;

	/* we finally got to the actual template string - so let's have some fun... */
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
		if(*p == '\"') {/* end of template string? */
			++p;	/* eat it! */
			bDone = 1;
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

/* Print the template structure. This is more or less a 
 * debug or test aid, but anyhow I think it's worth it...
 */
void tplPrintList(void)
{
	struct template *pTpl;
	struct templateEntry *pTpe;

	pTpl = tplRoot;
	while(pTpl != NULL) {
		dprintf("Template: Name='%s'\n", pTpl->pszName == NULL? "NULL" : pTpl->pszName);
		pTpe = pTpl->pEntryRoot;
		while(pTpe != NULL) {
			dprintf("\tEntry(%x): type %d, ", (unsigned) pTpe, pTpe->eEntryType);
			switch(pTpe->eEntryType) {
				case UNDEFINED:
					dprintf("(UNDEFINED)\n");
					break;
				case CONSTANT:
					dprintf("(CONSTANT), value: '%s'\n",
					        pTpe->data.constant.pConstant);
					break;
				case FIELD:
					dprintf("(FIELD), value: '%s'\n", pTpe->data.field.pPropRepl);
					break;
			}
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
