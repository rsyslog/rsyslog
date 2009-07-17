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
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <assert.h>
#include "librelp.h"
#include "relp.h"
#include "relpframe.h"
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
 * sets the integer value. If no string pointer is provided, the integer
 * value is added. The integer is ignored if a string pointer is present.
 * rgerhards, 2008-03-24
 */
relpRetVal
relpOfferValueAdd(unsigned char *pszVal, int intVal, relpOffer_t *pOffer)
{
	relpOfferValue_t *pThis = NULL;
	int i;
	int Val;

	ENTER_RELPFUNC;
	RELPOBJ_assert(pOffer, Offer);

	CHKRet(relpOfferValueConstruct(&pThis, pOffer->pEngine));

	/* check which value we need to use */
	if(pszVal == NULL) {
		snprintf((char*)pThis->szVal, sizeof(pThis->szVal), "%d", intVal);
		pThis->intVal = intVal;
	} else {
		strncpy((char*)pThis->szVal, (char*)pszVal, sizeof(pThis->szVal));
		/* check if the string actually is an integer... */
		Val = 0;
		i = 0;
		while(pszVal[i]) {
			if(isdigit(pszVal[i]))
				Val = Val * 10 + pszVal[i] - '0';
			else
				break;
			++i;
		}
		if(pszVal[i] != '\0')
			Val = -1; /* no (unsigned!) integer! */
		pThis->intVal = Val;
	}

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
 * freeing it. This function always allocates a sufficiently large
 * string (TODO: ensure that!). A string my be prepended to the offers
 * block (this is for rsp status). If that is not desired, the param
 * must be NULL, in which case its length is simply ignored (suggest to use
 * 0 in that case).
 * rgerhards, 2008-03-24
 */
relpRetVal
relpOffersToString(relpOffers_t *pThis, unsigned char *pszHdr, size_t lenHdr,
		   unsigned char **ppszOffers, size_t *plenStr)
{
	unsigned char *pszOffers = NULL;
	size_t iStr;
	size_t currSize;
	size_t iAlloc;
	relpOffer_t *pOffer;
	relpOfferValue_t *pOfferVal;

	ENTER_RELPFUNC;
	assert(ppszOffers != NULL);
	RELPOBJ_assert(pThis, Offers);

	if(pszHdr != NULL && lenHdr > 4096)
		iAlloc = 4096 + lenHdr;
	else
		iAlloc = 4096;

	if((pszOffers = malloc(iAlloc)) == NULL) {
		ABORT_FINALIZE(RELP_RET_OUT_OF_MEMORY);
	}

	currSize = iAlloc;
	iAlloc = 4096;

	/* check if we need to prepend anything */
	if(pszHdr == NULL) {
		iStr = 0; /* no, start at the beginning */
	} else {
		memcpy((char*)pszOffers, (char*)pszHdr, lenHdr);
		iStr = lenHdr;
	}

	for(pOffer = pThis->pRoot ; pOffer != NULL ; pOffer = pOffer->pNext) {
		/* we use -3 in the realloc-guard ifs so that we have space for constants following! */
		if(currSize - iStr - 3 < strlen((char*)pOffer->szName)) {
			if((pszOffers = realloc(pszOffers, currSize + iAlloc)) == NULL) {
				ABORT_FINALIZE(RELP_RET_OUT_OF_MEMORY);
				currSize += iAlloc;
			}
		}
		strcpy((char*)pszOffers+iStr, (char*)pOffer->szName);
		iStr += strlen((char*)pOffer->szName);
		pszOffers[iStr++] = '=';
		for(pOfferVal = pOffer->pValueRoot ; pOfferVal != NULL ; pOfferVal = pOfferVal->pNext) {
			if(currSize - iStr - 3 < strlen((char*)pOfferVal->szVal)) {
				if((pszOffers = realloc(pszOffers, currSize + iAlloc)) == NULL) {
					ABORT_FINALIZE(RELP_RET_OUT_OF_MEMORY);
					currSize += iAlloc;
				}
			}
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


/* construct an offers list from the offers contained in a frame. The frame's
 * Read Next pointer must be positioned at the first character of the offer.
 * rgerhards, 2008-03-25
 */
relpRetVal
relpOffersConstructFromFrame(relpOffers_t **ppOffers, relpFrame_t *pFrame)
{
	relpOffers_t *pOffers = NULL;
	relpOffer_t *pOffer;
	relpRetVal localRet;
	unsigned char c;
	size_t iName;
	size_t iVal;
	unsigned char szFeatNam[RELP_MAX_OFFER_FEATURENAME+1];
	unsigned char szFeatVal[RELP_MAX_OFFER_FEATUREVALUE+1];

	ENTER_RELPFUNC;
	assert(ppOffers != NULL);
	RELPOBJ_assert(pFrame, Frame);

	CHKRet(relpOffersConstruct(&pOffers, pFrame->pEngine));

	/* now process the command data */

	localRet = relpFrameGetNextC(pFrame, &c);
	while(localRet == RELP_RET_OK) {
		/* command name */
		iName = 0;
		while(iName < RELP_MAX_OFFER_FEATURENAME && c != '=' && localRet == RELP_RET_OK) {
			szFeatNam[iName++] = c;
			localRet = relpFrameGetNextC(pFrame, &c);
		}
		szFeatNam[iName] = '\0'; /* space is reserved for this! */
		CHKRet(relpOfferAdd(&pOffer, szFeatNam, pOffers));

		/* and now process the values (if any) */
		while(localRet == RELP_RET_OK && c != '\n') {
			localRet = relpFrameGetNextC(pFrame, &c); /* eat the "=" or "," */
			iVal = 0;
			while(   iVal < RELP_MAX_OFFER_FEATUREVALUE && localRet == RELP_RET_OK
			      && c != ',' && c != '\n' ) {
				szFeatVal[iVal++] = c;
				localRet = relpFrameGetNextC(pFrame, &c);
			}
			if(iVal > 0) { /* only set feature if one is actually given */
				szFeatVal[iVal] = '\0'; /* space is reserved for this */
				CHKRet(relpOfferValueAdd(szFeatVal, 0, pOffer));
			}
		}

		if(localRet == RELP_RET_OK && c == '\n')
			localRet = relpFrameGetNextC(pFrame, &c); /* eat '\n' */

	}

	if(localRet != RELP_RET_END_OF_DATA)
		ABORT_FINALIZE(localRet);

	*ppOffers = pOffers;

finalize_it:
	if(iRet != RELP_RET_OK) {
		if(pOffers != NULL)
			relpOffersDestruct(&pOffers);
	}

	LEAVE_RELPFUNC;
}
