/* objomsr.c
 * Implementation of the omsr (omodStringRequest) object.
 *
 * File begun on 2007-07-27 by RGerhards
 *
 * Copyright 2007 Rainer Gerhards and Adiscon GmbH.
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

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "rsyslog.h"
#include "objomsr.h"


/* destructor
 */
rsRetVal OMSRdestruct(omodStringRequest_t *pThis)
{
	int i;

	assert(pThis != NULL);
	/* free the strings */
	if(pThis->ppTplName != NULL) {
		for(i = 0 ; i < pThis->iNumEntries ; ++i) {
			if(pThis->ppTplName[i] != NULL) {
				free(pThis->ppTplName[i]);
			}
		}
		free(pThis->ppTplName);
	}
	if(pThis->piTplOpts != NULL)
		free(pThis->piTplOpts);
	free(pThis);
	
	return RS_RET_OK;
}


/* constructor
 */
rsRetVal OMSRconstruct(omodStringRequest_t **ppThis, int iNumEntries)
{
	omodStringRequest_t *pThis;
	DEFiRet;

	assert(ppThis != NULL);
	assert(iNumEntries >= 0);
	if((pThis = calloc(1, sizeof(omodStringRequest_t))) == NULL) {
		iRet = RS_RET_OUT_OF_MEMORY;
		goto abort_it;
	}

	/* got the structure, so fill it */
	pThis->iNumEntries = iNumEntries;
	/* allocate string for template name array. The individual strings will be
	 * allocated as the code progresses (we do not yet know the string sizes)
	 */
	if((pThis->ppTplName = calloc(iNumEntries, sizeof(uchar*))) == NULL) {
		OMSRdestruct(pThis);
		pThis = NULL;
		iRet =  RS_RET_OUT_OF_MEMORY;
		goto abort_it;
	}
	/* allocate the template options array. */
	if((pThis->piTplOpts = calloc(iNumEntries, sizeof(int))) == NULL) {
		OMSRdestruct(pThis);
		pThis = NULL;
		iRet =  RS_RET_OUT_OF_MEMORY;
		goto abort_it;
	}
	
abort_it:
	*ppThis = pThis;
	RETiRet;
}

/* set a template name and option to the object. Index must be given. The pTplName must be
 * pointing to memory that can be freed. If in doubt, the caller must strdup() the value.
 */
rsRetVal OMSRsetEntry(omodStringRequest_t *pThis, int iEntry, uchar *pTplName, int iTplOpts)
{
	assert(pThis != NULL);
	assert(pTplName != NULL);
	assert(iEntry < pThis->iNumEntries);

	if(pThis->ppTplName[iEntry] != NULL)
		free(pThis->ppTplName[iEntry]);
	pThis->ppTplName[iEntry] = pTplName;
	pThis->piTplOpts[iEntry] = iTplOpts;

	return RS_RET_OK;
}


/* get number of entries for this object
 */
int OMSRgetEntryCount(omodStringRequest_t *pThis)
{
	assert(pThis != NULL);
	return pThis->iNumEntries;
}


/* return data for a specific entry. All data returned is 
 * read-only and lasts only as long as the object lives. If the caller
 * needs it for an extended period of time, the caller must copy the
 * strings. Please note that the string pointer may be NULL, which is the
 * case when it was never set.
 */
int OMSRgetEntry(omodStringRequest_t *pThis, int iEntry, uchar **ppTplName, int *piTplOpts)
{
	assert(pThis != NULL);
	assert(ppTplName != NULL);
	assert(piTplOpts != NULL);
	assert(iEntry < pThis->iNumEntries);

	*ppTplName = pThis->ppTplName[iEntry];
	*piTplOpts = pThis->piTplOpts[iEntry];

	return RS_RET_OK;
}
/*
 * vi:set ai:
 */
