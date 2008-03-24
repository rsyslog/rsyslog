/* Support functionality for sending and receiving offers.
 *
 * Copyright 2008 by Rainer Gerhards and Adiscon GmbH.
 *
 * This file is part of librelp.
 *
 * Librelp is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Librelp is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Librelp.  If not, see <http://www.gnu.org/licenses/>.
 *
 * A copy of the GPL can be found in the file "COPYING" in this distribution.
 *
 * If the terms of the GPL are unsuitable for your needs, you may obtain
 * a commercial license from Adiscon. Contact sales@adiscon.com for further
 * details.
 *
 * ALL CONTRIBUTORS PLEASE NOTE that by sending contributions, you assign
 * your copyright to Adiscon GmbH, Germany. This is necessary to permit the
 * dual-licensing set forth here. Our apologies for this inconvenience, but
 * we sincerely believe that the dual-licensing model helps us provide great
 * free software while at the same time obtaining some funding for further
 * development.
 */
#include "config.h"
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <assert.h>
#include "librelp.h"
#include "relp.h"
#include "offers.h"


/** Construct a RELP offer value instance
 * rgerhards, 2008-03-24
 */
static relpRetVal
relpOfferValueConstruct(relpOfferValue_t **ppThis, relpEngine_t *pEngine)
{
	relpOfferValue_t *pThis;

	ENTER_RELPFUNC;
	assert(ppThis != NULL);
	if((pThis = calloc(1, sizeof(relpOfferValue_t))) == NULL) {
		ABORT_FINALIZE(RELP_RET_OUT_OF_MEMORY);
	}

	RELP_CORE_CONSTRUCTOR(pThis, OfferValue);
	pThis->pEngine = pEngine;

	*ppThis = pThis;

finalize_it:
	LEAVE_RELPFUNC;
}


/** Destruct a RELP offer value instance
 * rgerhards, 2008-03-24
 */
static relpRetVal
relpOfferValueDestruct(relpOfferValue_t **ppThis)
{
	relpOfferValue_t *pThis;

	ENTER_RELPFUNC;
	assert(ppThis != NULL);
	pThis = *ppThis;
	RELPOBJ_assert(pThis, OfferValue);

	/* done with de-init work, now free offers object itself */
	free(pThis);
	*ppThis = NULL;

	LEAVE_RELPFUNC;
}


/** Construct a RELP offer instance
 * rgerhards, 2008-03-24
 */
static relpRetVal
relpOfferConstruct(relpOffer_t **ppThis, relpEngine_t *pEngine)
{
	relpOffer_t *pThis;

	ENTER_RELPFUNC;
	assert(ppThis != NULL);
	if((pThis = calloc(1, sizeof(relpOffer_t))) == NULL) {
		ABORT_FINALIZE(RELP_RET_OUT_OF_MEMORY);
	}

	RELP_CORE_CONSTRUCTOR(pThis, Offer);
	pThis->pEngine = pEngine;

	*ppThis = pThis;

finalize_it:
	LEAVE_RELPFUNC;
}


/** Destruct a RELP offer instance
 * rgerhards, 2008-03-24
 */
static relpRetVal
relpOfferDestruct(relpOffer_t **ppThis)
{
	relpOffer_t *pThis;
	relpOfferValue_t *pOfferVal;
	relpOfferValue_t *pOfferValToDel;

	ENTER_RELPFUNC;
	assert(ppThis != NULL);
	pThis = *ppThis;
	RELPOBJ_assert(pThis, Offer);

	pOfferVal = pThis->pValueRoot;
	while(pOfferVal != NULL) {
		pOfferValToDel = pOfferVal;
		pOfferVal = pOfferVal->pNext;
		relpOfferValueDestruct(&pOfferValToDel);
	}

	/* done with de-init work, now free offers object itself */
	free(pThis);
	*ppThis = NULL;

	LEAVE_RELPFUNC;
}


/** Construct a RELP offers list instance
 * rgerhards, 2008-03-24
 */
relpRetVal
relpOffersConstruct(relpOffers_t **ppThis, relpEngine_t *pEngine)
{
	relpOffers_t *pThis;

	ENTER_RELPFUNC;
	assert(ppThis != NULL);
	if((pThis = calloc(1, sizeof(relpOffers_t))) == NULL) {
		ABORT_FINALIZE(RELP_RET_OUT_OF_MEMORY);
	}

	RELP_CORE_CONSTRUCTOR(pThis, Offers);
	pThis->pEngine = pEngine;

	*ppThis = pThis;

finalize_it:
	LEAVE_RELPFUNC;
}


/** Destruct a RELP offers list
 * rgerhards, 2008-03-24
 */
relpRetVal
relpOffersDestruct(relpOffers_t **ppThis)
{
	relpOffers_t *pThis;
	relpOffer_t *pOffer;
	relpOffer_t *pOfferToDel;

	ENTER_RELPFUNC;
	assert(ppThis != NULL);
	pThis = *ppThis;
	RELPOBJ_assert(pThis, Offers);

	pOffer = pThis->pRoot;
	while(pOffer != NULL) {
		pOfferToDel = pOffer;
		pOffer = pOffer->pNext;
		relpOfferDestruct(&pOfferToDel);
	}

	/* done with de-init work, now free offers object itself */
	free(pThis);
	*ppThis = NULL;

	LEAVE_RELPFUNC;
}


/* Construct a offer value with pszVal and add it to the offer value
 * list. This function also checks if the value is an integer and, if so,
 * sets the integer value.
 * rgerhards, 2008-03-24
 */
relpRetVal
relpOfferValueAdd(unsigned char *pszVal, relpOffer_t *pOffer)
{
	relpOfferValue_t *pThis = NULL;
	int i;
	int intVal;

	ENTER_RELPFUNC;
	assert(pszVal != NULL);
	RELPOBJ_assert(pOffer, Offer);

	/* check if we have an integer */
	intVal = 0;
	i = 0;
	while(pszVal[i]) {
		if(isdigit(pszVal[i]))
			intVal = intVal * 10 + pszVal[i] - '0';
		else
			break;
		++i;
	}
	if(pszVal[i] != '\0')
		intVal = -1; /* no (unsigned!) integer! */

	CHKRet(relpOfferValueConstruct(&pThis, pOffer->pEngine));
	strncpy((char*)pThis->szVal, (char*)pszVal, sizeof(pThis->szVal));
	pThis->intVal = intVal;
	pThis->pNext = pOffer->pValueRoot;
	pOffer->pValueRoot = pThis;

finalize_it:
	if(iRet != RELP_RET_OK) {
		if(pThis != NULL)
			relpOfferValueDestruct(&pThis);
	}

	LEAVE_RELPFUNC;
}


/* Construct an offer with name pszName and add it to the offers list.
 * The offer object is returned to the caller. It is read-only and MUST NOT
 * be destructed.
 * rgerhards, 2008-03-24
 */
relpRetVal
relpOfferAdd(relpOffer_t **ppThis, unsigned char *pszName, relpOffers_t *pOffers)
{
	relpOffer_t *pThis = NULL;

	ENTER_RELPFUNC;
	assert(ppThis != NULL);
	assert(pszName != NULL);
	RELPOBJ_assert(pOffers, Offers);

	CHKRet(relpOfferConstruct(&pThis, pOffers->pEngine));
	strncpy((char*)pThis->szName, (char*)pszName, sizeof(pThis->szName));
	pThis->pNext = pOffers->pRoot;
	pOffers->pRoot = pThis;

	*ppThis = pThis;

finalize_it:
	if(iRet != RELP_RET_OK) {
		if(pThis != NULL)
			relpOfferDestruct(&pThis);
	}

	LEAVE_RELPFUNC;
}


/* create as string with the complete offers. The string is dynamically
 * allocated and passed to the caller. The caller is responsible for
 * freeing it. This function always allocates a sufficientla large
 * string (TODO: ensure that!)
 * rgerhards, 2008-03-24
 */
relpRetVal
relpOffersToString(relpOffers_t *pThis, unsigned char **ppszOffers, size_t *plenStr)
{
	unsigned char *pszOffers = NULL;
	size_t iStr;
	relpOffer_t *pOffer;
	relpOfferValue_t *pOfferVal;

	ENTER_RELPFUNC;
	assert(ppszOffers != NULL);
	RELPOBJ_assert(pThis, Offers);

	if((pszOffers = malloc(4096)) == NULL) { // TODO: not fixed!
		ABORT_FINALIZE(RELP_RET_OUT_OF_MEMORY);
	}

	iStr = 0;
	for(pOffer = pThis->pRoot ; pOffer != NULL ; pOffer = pOffer->pNext) {
		strcpy((char*)pszOffers+iStr, (char*)pOffer->szName);
		iStr += strlen((char*)pOffer->szName);
		pszOffers[iStr++] = '=';
		for(pOfferVal = pOffer->pValueRoot ; pOfferVal != NULL ; pOfferVal = pOfferVal->pNext) {
			strcpy((char*)pszOffers+iStr, (char*)pOfferVal->szVal);
			iStr += strlen((char*)pOfferVal->szVal);
			if(pOfferVal->pNext != NULL)
				pszOffers[iStr++] = ',';
		}
		if(pOffer->pNext != NULL)
			pszOffers[iStr++] = '\n';
	}

	*ppszOffers = pszOffers;
	*plenStr = iStr;

finalize_it:
	if(iRet != RELP_RET_OK) {
		if(pszOffers != NULL)
			free(pszOffers);
	}

	LEAVE_RELPFUNC;
}


#if 0 // CODE WE (HOPEFULY) CAN USE...
/* read the response header. The frame's pData read pointer is moved to the
 * first byte of the response data buffer (if any).
 * rgerhards, 2008-03-24
 */
static relpRetVal
readRspHdr(relpFrame_t *pFrame, int *pCode, unsigned char *pText)
{
	int iText;
	unsigned char c;
	relpRetVal localRet;

	ENTER_RELPFUNC;

	/* now come the message. It is terminated after 80 chars or when an LF
	 * is found (the LF is the delemiter to response data) or there is no
	 * more data.
	 */
	for(iText = 0 ; iText < 80 ; ++iText) {
		localRet = relpFrameGetNextC(pFrame, &c);
		if(localRet == RELP_RET_END_OF_DATA)
			break;
		else if(localRet != RELP_RET_OK)
			ABORT_FINALIZE(localRet);
		if(c == '\n')
			break;
		/* we got a regular char which we need to copy */
		pText[iText] = c;
	}
	pText[iText] = '\0'; /* make it a C-string */

finalize_it:
	LEAVE_RELPFUNC;
}
#endif
