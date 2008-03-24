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
 * along with librelp.  If not, see <http://www.gnu.org/licenses/>.
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
#ifndef RELPOFFERS_H_INCLUDED
#define	RELPOFFERS_H_INCLUDED

#include "relp.h"

/* relp offer parameters
 * rgerhards, 2008-03-24
 */
typedef struct relpOfferValue_s {
	BEGIN_RELP_OBJ;
	relpEngine_t *pEngine;
	struct relpOfferValue_s *pNext;
	unsigned char szVal[255+1];
	int intVal; /* -1 if no integer is set, else this is an unsigened value! */
} relpOfferValue_t;


/* the RELPOFFER object 
 * rgerhards, 2008-03-24
 */
typedef struct relpOffer_s {
	BEGIN_RELP_OBJ;
	relpEngine_t *pEngine;
	struct relpOffer_s *pNext;
	relpOfferValue_t *pValueRoot;
	unsigned char szName[32+1];
} relpOffer_t;


/* The list of relpoffers.
 * rgerhards, 2008-03-24
 */
typedef struct relpOffers_s {
	BEGIN_RELP_OBJ;
	relpEngine_t *pEngine;
	relpOffer_t *pRoot;
} relpOffers_t;


/* prototypes */
relpRetVal relpOffersConstruct(relpOffers_t **ppThis, relpEngine_t *pEngine);
relpRetVal relpOffersDestruct(relpOffers_t **ppThis);
relpRetVal relpOfferValueAdd(unsigned char *pszVal, relpOffer_t *pOffer);
relpRetVal relpOfferAdd(relpOffer_t **ppThis, unsigned char *pszName, relpOffers_t *pOffers);
relpRetVal relpOffersToString(relpOffers_t *pThis, unsigned char **ppszOffers, size_t *plenStr);

#endif /* #ifndef RELPOFFERS_H_INCLUDED */
