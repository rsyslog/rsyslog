/* The command handler for server-based "open" (after reception)
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
 * free software while at the same time obtaining some development funding.
 */
#include "config.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "relp.h"
#include "offers.h"
#include "cmdif.h"


/* Select which offers to use during a session. This takes the client offers
 * and constructs a new server offers object based on it. This new object
 * contains only what we support (and require, so we may actually add
 * something to the offer).
 * rgerhards, 2008-03-25
 */
static relpRetVal
selectOffers(relpSess_t *pSess, relpOffers_t *pCltOffers, relpOffers_t **ppSrvOffers)
{
	relpEngine_t *pEngine;
	relpOffer_t *pOffer;
	relpOfferValue_t *pOfferVal;

	ENTER_RELPFUNC;
	assert(ppSrvOffers != NULL);
	RELPOBJ_assert(pCltOffers, Offers);
	RELPOBJ_assert(pSess, Sess);
	pEngine = pSess->pEngine;

	/* we loop through the offers and set session parameters */
	for(pOffer = pCltOffers->pRoot ; pOffer != NULL ; pOffer = pOffer->pNext) {
		pEngine->dbgprint("processing client offer '%s'\n", pOffer->szName);
		if(!strcmp((char*)pOffer->szName, "relp_version")) {
			if(pOffer->pValueRoot == NULL)
				ABORT_FINALIZE(RELP_RET_INVALID_OFFER);
			if(pOffer->pValueRoot->intVal == -1)
				ABORT_FINALIZE(RELP_RET_INVALID_OFFER);
			if(pOffer->pValueRoot->intVal > pEngine->protocolVersion)
				relpSessSetProtocolVersion(pSess, pEngine->protocolVersion);
			else
				relpSessSetProtocolVersion(pSess, pOffer->pValueRoot->intVal);
		} else if(!strcmp((char*)pOffer->szName, "commands")) {
			for(pOfferVal = pOffer->pValueRoot ; pOfferVal != NULL ; pOfferVal = pOfferVal->pNext) {
pSess->pEngine->dbgprint("cmd syslog state in srv session: %d\n", pSess->stateCmdSyslog);
				if(pSess->stateCmdSyslog == eRelpCmdState_Desired) {
					/* we do not care about return code in this case */
					relpSessSetEnableCmd(pSess, pOfferVal->szVal, eRelpCmdState_Enabled);
				}
			}
		} else if(!strcmp((char*)pOffer->szName, "relp_software")) {
			/* we know this parameter, but we do not do anything
			 * with it -- this may change if we need to emulate
			 * something based on known bad relp software behaviour.
			 */
		} else {
			/* if we do not know an offer name, we ignore it - in this
			 * case, we may simply not support it (but the client does and
			 * must now live without it...)
			 */
			pEngine->dbgprint("ignoring unknown client offer '%s'\n", pOffer->szName);
		}
	}

	/* now we have processed the client offers, so now we can construct
	 * our own offers based on what is set in the session parameters.
	 * TODO: move this to the session object? May also be used during
	 * client connect...
	 */
	CHKRet(relpSessConstructOffers(pSess, ppSrvOffers));

finalize_it:
	LEAVE_RELPFUNC;
}


/* process the "open" command
 * rgerhards, 2008-03-17
 */
BEGINcommand(S, Init)
	relpOffers_t *pCltOffers = NULL;
	relpOffers_t *pSrvOffers = NULL;
	unsigned char *pszSrvOffers = NULL;
	size_t lenSrvOffers;

	ENTER_RELPFUNC;
	pSess->pEngine->dbgprint("in open command handler\n");

	CHKRet(relpOffersConstructFromFrame(&pCltOffers, pFrame));
	CHKRet(selectOffers(pSess, pCltOffers, &pSrvOffers));

	/* we got our offers together, so we now can send the response */
	CHKRet(relpOffersToString(pSrvOffers, (unsigned char*)"200 OK\n", 7, &pszSrvOffers, &lenSrvOffers));
	CHKRet(relpSessSendResponse(pSess, pFrame->txnr, pszSrvOffers, lenSrvOffers));

finalize_it:
	if(pszSrvOffers != NULL)
		free(pszSrvOffers);
	if(pCltOffers != NULL)
		relpOffersDestruct(&pCltOffers);
	if(pSrvOffers != NULL)
		relpOffersDestruct(&pSrvOffers);
ENDcommand
