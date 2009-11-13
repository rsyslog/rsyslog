/* objomsr.c
 * Implementation of the omsr (omodStringRequest) object.
 *
 * File begun on 2007-07-27 by RGerhards
 *
 * Copyright 2007 Rainer Gerhards and Adiscon GmbH.
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
	CHKmalloc(pThis = calloc(1, sizeof(omodStringRequest_t)));

	/* got the structure, so fill it */
	pThis->iNumEntries = iNumEntries;
	/* allocate string for template name array. The individual strings will be
	 * allocated as the code progresses (we do not yet know the string sizes)
	 */
	CHKmalloc(pThis->ppTplName = calloc(iNumEntries, sizeof(uchar*)));

	/* allocate the template options array. */
	CHKmalloc(pThis->piTplOpts = calloc(iNumEntries, sizeof(int)));
	
finalize_it:
	if(iRet != RS_RET_OK) {
		if(pThis != NULL) {
			OMSRdestruct(pThis);
			pThis = NULL;
		}
	}
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


/* return the full set of template options that are supported by this version of
 * OMSR. They are returned in an unsigned long value. The caller can mask that
 * value to check on the option he is interested in.
 * Note that this interface was added in 4.1.6, so a plugin must obtain a pointer
 * to this interface via queryHostEtryPt().
 * rgerhards, 2009-04-03
 */
rsRetVal
OMSRgetSupportedTplOpts(unsigned long *pOpts)
{
	DEFiRet;
	assert(pOpts != NULL);
	*pOpts = OMSR_RQD_TPL_OPT_SQL | OMSR_TPL_AS_ARRAY | OMSR_TPL_AS_MSG;
	RETiRet;
}

/* vim:set ai:
 */
