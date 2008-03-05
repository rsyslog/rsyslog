/* This is the output channel processing code of rsyslog.
 * Output channels - in the long term - will define how
 * messages will be sent to whatever file or other medium.
 * Currently, they mainly provide a way to store some file-related
 * information (most importantly the maximum file size allowed).
 * Please see syslogd.c for license information.
 * This code is placed under the GPL.
 * begun 2005-06-21 rgerhards
 *
 * Copyright (C) 2005 by Rainer Gerhards and Adiscon GmbH
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

#include "rsyslog.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include "stringbuf.h"
#include "outchannel.h"
#include "syslogd.h"

static struct outchannel *ochRoot = NULL;	/* the root of the outchannel list */
static struct outchannel *ochLast = NULL;	/* points to the last element of the outchannel list */

/* Constructs a outchannel list object. Returns pointer to it
 * or NULL (if it fails).
 */
struct outchannel* ochConstruct(void)
{
	struct outchannel *pOch;
	if((pOch = calloc(1, sizeof(struct outchannel))) == NULL)
		return NULL;
	
	/* basic initialisaion is done via calloc() - need to
	 * initialize only values != 0. */

	if(ochLast == NULL)
	{ /* we are the first element! */
		ochRoot = ochLast = pOch;
	}
	else
	{
		ochLast->pNext = pOch;
		ochLast = pOch;
	}

	return(pOch);
}


/* skips the next comma and any whitespace
 * in front and after it.
 */
static void skip_Comma(char **pp)
{
	register char *p;

	assert(pp != NULL);
	assert(*pp != NULL);

	p = *pp;
	while(isspace((int)*p))
		++p;
	if(*p == ',')
		++p;
	while(isspace((int)*p))
		++p;
	*pp = p;
}

/* helper to ochAddLine. Parses a comma-delimited field
 * The field is delimited by SP or comma. Leading whitespace
 * is "eaten" and does not become part of the field content.
 * returns: 0 - ok, 1 - failure
 */
static int get_Field(uchar **pp, uchar **pField)
{
	register uchar *p;
	cstr_t *pStrB;

	assert(pp != NULL);
	assert(*pp != NULL);
	assert(pField != NULL);

	skip_Comma((char**)pp);
	p = *pp;

	if(rsCStrConstruct(&pStrB) != RS_RET_OK)
		 return 1;
	rsCStrSetAllocIncrement(pStrB, 32);

	/* copy the field */
	while(*p && *p != ' ' && *p != ',') {
		rsCStrAppendChar(pStrB, *p++);
	}

	*pp = p;
	rsCStrFinish(pStrB);
	if(rsCStrConvSzStrAndDestruct(pStrB, pField, 0) != RS_RET_OK)
		return 1;

	return 0;
}


/* helper to ochAddLine. Parses a off_t type from the
 * input line.
 * returns: 0 - ok, 1 - failure
 */
static int get_off_t(uchar **pp, off_t *pOff_t)
{
	register uchar *p;
	off_t val;

	assert(pp != NULL);
	assert(*pp != NULL);
	assert(pOff_t != NULL);

	skip_Comma((char**)pp);
	p = *pp;

	val = 0;
	while(*p && isdigit((int)*p)) {
		val = val * 10 + (*p - '0');
		++p;
	}

	*pp = p;
	*pOff_t = val;

	return 0;
}


/* helper to ochAddLine. Parses everything from the
 * current position to the end of line and returns it
 * to the caller. Leading white space is removed, but
 * not trailing.
 * returns: 0 - ok, 1 - failure
 */
static inline int get_restOfLine(uchar **pp, uchar **pBuf)
{
	register uchar *p;
	cstr_t *pStrB;

	assert(pp != NULL);
	assert(*pp != NULL);
	assert(pBuf != NULL);

	skip_Comma((char**)pp);
	p = *pp;

	if(rsCStrConstruct(&pStrB) != RS_RET_OK)
		 return 1;
	rsCStrSetAllocIncrement(pStrB, 32);

	/* copy the field */
	while(*p) {
		rsCStrAppendChar(pStrB, *p++);
	}

	*pp = p;
	rsCStrFinish(pStrB);
	if(rsCStrConvSzStrAndDestruct(pStrB, pBuf, 0) != RS_RET_OK)
		return 1;

	return 0;
}


/* Add a new outchannel line
 * returns pointer to new object if it succeeds, NULL otherwise.
 * An outchannel line is primarily a set of fields delemited by commas.
 * There might be some whitespace between the field (but not within)
 * and the commas. This can be removed.
 */
struct outchannel *ochAddLine(char* pName, uchar** ppRestOfConfLine)
{
	struct outchannel *pOch;
 	uchar *p;

	assert(pName != NULL);
	assert(ppRestOfConfLine != NULL);

	if((pOch = ochConstruct()) == NULL)
		return NULL;
	
	pOch->iLenName = strlen(pName);
	pOch->pszName = (char*) malloc(sizeof(char) * (pOch->iLenName + 1));
	if(pOch->pszName == NULL) {
		dbgprintf("ochAddLine could not alloc memory for outchannel name!");
		pOch->iLenName = 0;
		return NULL;
		/* I know - we create a memory leak here - but I deem
		 * it acceptable as it is a) a very small leak b) very
		 * unlikely to happen. rgerhards 2004-11-17
		 */
	}
	memcpy(pOch->pszName, pName, pOch->iLenName + 1);

	/* now actually parse the line */
	p = *ppRestOfConfLine;
	assert(p != NULL);

	/* get params */
	get_Field(&p, &pOch->pszFileTemplate);
	if(*p) get_off_t(&p, &pOch->uSizeLimit);
	if(*p) get_restOfLine(&p, &pOch->cmdOnSizeLimit);

	*ppRestOfConfLine = p;
	return(pOch);
}


/* Find a outchannel object based on name. Search
 * currently is case-senstive (should we change?).
 * returns pointer to outchannel object if found and
 * NULL otherwise.
 * rgerhards 2004-11-17
 */
struct outchannel *ochFind(char *pName, int iLenName)
{
	struct outchannel *pOch;

	assert(pName != NULL);

	pOch = ochRoot;
	while(pOch != NULL &&
	      !(pOch->iLenName == iLenName &&
	        !strcmp(pOch->pszName, pName)
	        ))
		{
			pOch = pOch->pNext;
		}
	return(pOch);
}

/* Destroy the outchannel structure. This is for de-initialization
 * at program end. Everything is deleted.
 * rgerhards 2005-02-22
 */
void ochDeleteAll(void)
{
	struct outchannel *pOch, *pOchDel;

	pOch = ochRoot;
	while(pOch != NULL) {
		dbgprintf("Delete Outchannel: Name='%s'\n ", pOch->pszName == NULL? "NULL" : pOch->pszName);
		pOchDel = pOch;
		pOch = pOch->pNext;
		if(pOchDel->pszName != NULL)
			free(pOchDel->pszName);
		free(pOchDel);
	}
}


/* Print the outchannel structure. This is more or less a 
 * debug or test aid, but anyhow I think it's worth it...
 */
void ochPrintList(void)
{
	struct outchannel *pOch;

	pOch = ochRoot;
	while(pOch != NULL) {
		dbgprintf("Outchannel: Name='%s'\n", pOch->pszName == NULL? "NULL" : pOch->pszName);
		dbgprintf("\tFile Template: '%s'\n", pOch->pszFileTemplate == NULL ? "NULL" : (char*) pOch->pszFileTemplate);
		dbgprintf("\tMax Size.....: %lu\n", (long unsigned) pOch->uSizeLimit);
		dbgprintf("\tOnSizeLimtCmd: '%s'\n", pOch->cmdOnSizeLimit == NULL ? "NULL" : (char*) pOch->cmdOnSizeLimit);
		pOch = pOch->pNext; /* done, go next */
	}
}
/*
 * vi:set ai:
 */
