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

	assert(pp != NULL);
	assert(*pp != NULL);
	assert(pTpl != NULL);

	p = *pp;

	if((pStrB = sbStrBConstruct()) == NULL)
		 return 1;
	sbStrBSetAllocIncrement(pStrB, 32);
	/* TODO: implement escape characters! */
	while(*p && *p != '%') {
		if(*p == '\\')
			if(*++p == '\0')
				--p;	/* the best we can do - it's invalid anyhow... */
		sbStrBAppendChar(pStrB, *p++);
	}

	printf("Constant: '%s'\n", sbStrBFinish(pStrB));
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
	printf("Parameter: '%s'\n", sbStrBFinish(pStrB));

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
		printf("could not alloc memory Template"); /* TODO: change to dprintf()! */
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
	}

printf("tplAddLine, Name '%s', restOfLine: %s", pTpl->pszName, p);

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

/*
 * vi:set ai:
 */
