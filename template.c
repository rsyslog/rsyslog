/* This is the template processing code of rsyslog.
 * Please see syslogd.c for license information.
 * This code is placed under the GPL.
 * begun 2004-11-17 rgerhards
 */
#include <stdio.h>
#include <malloc.h>
#include "template.h"

static struct template *tplRoot = NULL;	/* the root of the templat list */
static struct template *tplLast = NULL;	/* points to the last element of the template list */

/* Constructs a template entry. Returns pointer to it
 * or NULL (if it fails).
 */
struct template* tplConstruct(void)
{
	struct template *pTpl;
	if((pTpl = malloc(sizeof(struct template))) == NULL)
		return NULL;
	
	pTpl->pszName = NULL;
	pTpl->pszTemplate = NULL;

	if(tplLast == NULL)
	{ /* we are the first element! */
		tplRoot = tplLast = pTpl;
	}

	return(pTpl);
}

/*
 * vi:set ai:
 */
