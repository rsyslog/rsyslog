/* The relp client.
 *
 * Copyright 2008-2013 by Rainer Gerhards and Adiscon GmbH.
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
#include <assert.h>
#include <string.h>
#include <sys/types.h>
#include "relp.h"
#include "relpsess.h"
#include "relpclt.h"


/** Construct a RELP clt instance
 * This is the first thing that a caller must do before calling any
 * RELP function. The relp clt must only destructed after all RELP
 * operations have been finished.
 */
relpRetVal
relpCltConstruct(relpClt_t **ppThis, relpEngine_t *pEngine)
{
	relpClt_t *pThis;

	ENTER_RELPFUNC;
	assert(ppThis != NULL);
	if((pThis = calloc(1, sizeof(relpClt_t))) == NULL) {
		ABORT_FINALIZE(RELP_RET_OUT_OF_MEMORY);
	}

	RELP_CORE_CONSTRUCTOR(pThis, Clt);
	pThis->pEngine = pEngine;
	pThis->timeout = 90; /* 90-second timeout is the default */
	pThis->pUsr = NULL;
	pThis->authmode = eRelpAuthMode_None;
	pThis->pristring = NULL;
	pThis->caCertFile = NULL;
	pThis->ownCertFile = NULL;
	pThis->privKey = NULL;
	pThis->permittedPeers.nmemb = 0;

	*ppThis = pThis;

finalize_it:
	LEAVE_RELPFUNC;
}


/** Destruct a RELP clt instance
 */
relpRetVal
relpCltDestruct(relpClt_t **ppThis)
{
	int i;
	relpClt_t *pThis;

	ENTER_RELPFUNC;
	assert(ppThis != NULL);
	pThis = *ppThis;
	RELPOBJ_assert(pThis, Clt);

	if(pThis->pSess != NULL)
		relpSessDestruct(&pThis->pSess);
	free(pThis->clientIP);
	free(pThis->pristring);
	free(pThis->caCertFile);
	free(pThis->ownCertFile);
	free(pThis->privKey);
	for(i = 0 ; i < pThis->permittedPeers.nmemb ; ++i)
		free(pThis->permittedPeers.name[i]);

	/* done with de-init work, now free clt object itself */
	free(pThis);
	*ppThis = NULL;

	LEAVE_RELPFUNC;
}


/** open a relp session to a remote peer
 * remote servers parameters must already have been set.
 * rgerhards, 2008-03-19
 */
relpRetVal
relpCltConnect(relpClt_t *pThis, int protFamily, unsigned char *port, unsigned char *host)
{
	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Clt);

	CHKRet(relpSessConstruct(&pThis->pSess, pThis->pEngine, RELP_CLT_CONN, pThis));
	CHKRet(relpSessSetTimeout(pThis->pSess, pThis->timeout));
	CHKRet(relpSessSetWindowSize(pThis->pSess, pThis->sizeWindow));
	CHKRet(relpSessSetClientIP(pThis->pSess, pThis->clientIP));
	CHKRet(relpSessSetUsrPtr(pThis->pSess, pThis->pUsr));
	if(pThis->bEnableTLS) {
		CHKRet(relpSessEnableTLS(pThis->pSess));
		if(pThis->bEnableTLSZip) {
			CHKRet(relpSessEnableTLSZip(pThis->pSess));
		}
		CHKRet(relpSessSetGnuTLSPriString(pThis->pSess, pThis->pristring));
		CHKRet(relpSessSetCACert(pThis->pSess, pThis->caCertFile));
		CHKRet(relpSessSetOwnCert(pThis->pSess, pThis->ownCertFile));
		CHKRet(relpSessSetPrivKey(pThis->pSess, pThis->privKey));
		CHKRet(relpSessSetAuthMode(pThis->pSess, pThis->authmode));
		CHKRet(relpSessSetPermittedPeers(pThis->pSess, &pThis->permittedPeers));
	}
	CHKRet(relpSessConnect(pThis->pSess, protFamily, port, host));

finalize_it:
	if(iRet != RELP_RET_OK) {
		if(pThis->pSess != NULL) {
			relpSessDestruct(&pThis->pSess);
		}
	}

	LEAVE_RELPFUNC;
}


/** Try to reconnect a broken session to the remote
 * server. The main difference to relpCltConnect() is that the
 * session object is already existing and session parameters (like
 * remote host) can not be changed.
 * rgerhards, 2008-03-23
 */
relpRetVal
relpCltReconnect(relpClt_t *pThis)
{
	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Clt);
	RELPOBJ_assert(pThis->pSess, Sess);

	CHKRet(relpSessTryReestablish(pThis->pSess));

finalize_it:
	LEAVE_RELPFUNC;
}


/** Set the relp window size for this client. Value 0 means
 * that the default value is to be used.
 */
relpRetVal
relpCltSetWindowSize(relpClt_t *pThis, int sizeWindow)
{
	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Clt);
	if(sizeWindow < 0)
		ABORT_FINALIZE(RELP_RET_ERR_INVAL);
	if(sizeWindow != 0)
		pThis->sizeWindow = sizeWindow;
finalize_it:
	LEAVE_RELPFUNC;
}

/** Set the timeout value for this client.  */
relpRetVal relpCltSetTimeout(relpClt_t *pThis, unsigned timeout)
{
	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Clt);
	pThis->timeout = timeout;
	LEAVE_RELPFUNC;
}


/** Set the local IP address to be used when acting as a client.
 */
relpRetVal relpCltSetClientIP(relpClt_t *pThis, unsigned char *ipAddr)
{
	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Clt);
	free(pThis->clientIP);
	pThis->clientIP = ipAddr == NULL ? NULL :
					   (unsigned char*)strdup((char*) ipAddr);
	LEAVE_RELPFUNC;
}

relpRetVal
relpCltAddPermittedPeer(relpClt_t *pThis, char *peer)
{
	char **newName;
	int newMemb;
	ENTER_RELPFUNC;
	newMemb = pThis->permittedPeers.nmemb + 1;
	RELPOBJ_assert(pThis, Clt);
	newName = realloc(pThis->permittedPeers.name, sizeof(char*) * newMemb);
	if(newName == NULL) {
		ABORT_FINALIZE(RELP_RET_OUT_OF_MEMORY);
	}
	if((newName[newMemb - 1] = strdup(peer)) == NULL) {
		free(newName);
		ABORT_FINALIZE(RELP_RET_OUT_OF_MEMORY);
	}
	pThis->permittedPeers.name = newName;
	pThis->permittedPeers.nmemb = newMemb;
	pThis->pEngine->dbgprint("librelp: CLT permitted peer added: '%s'\n", peer);

finalize_it:
	LEAVE_RELPFUNC;
}

relpRetVal
relpCltSetUsrPtr(relpClt_t *pThis, void *pUsr)
{
	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Clt);
	pThis->pUsr = pUsr;
	LEAVE_RELPFUNC;
}

/* Note: mode==NULL is valid and means "no change" */
relpRetVal
relpCltSetAuthMode(relpClt_t *pThis, char *mode)
{
	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Clt);
	if(mode == NULL) 
		FINALIZE;

	if(!strcasecmp(mode, "fingerprint"))
		pThis->authmode = eRelpAuthMode_Fingerprint;
	else if(!strcasecmp(mode, "name"))
		pThis->authmode = eRelpAuthMode_Name;
	else
		ABORT_FINALIZE(RELP_RET_INVLD_AUTH_MD);
		
finalize_it:
	LEAVE_RELPFUNC;
}

/* set the GnuTLS priority string. Providing NULL does re-set
 * any previously set string. -- rgerhards, 2013-06-12
 */
relpRetVal
relpCltSetGnuTLSPriString(relpClt_t *pThis, char *pristr)
{
	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Clt);
	free(pThis->pristring);
	if(pristr == NULL) {
		pThis->pristring = NULL;
	} else {
		if((pThis->pristring = strdup(pristr)) == NULL)
			ABORT_FINALIZE(RELP_RET_OUT_OF_MEMORY);
	}
finalize_it:
	LEAVE_RELPFUNC;
}
relpRetVal
relpCltSetCACert(relpClt_t *pThis, char *file)
{
	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Clt);
	free(pThis->caCertFile);
	if(file == NULL) {
		pThis->caCertFile = NULL;
	} else {
		if((pThis->caCertFile = strdup(file)) == NULL)
			ABORT_FINALIZE(RELP_RET_OUT_OF_MEMORY);
	}
finalize_it:
	LEAVE_RELPFUNC;
}
relpRetVal
relpCltSetOwnCert(relpClt_t *pThis, char *file)
{
	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Clt);
	free(pThis->ownCertFile);
	if(file == NULL) {
		pThis->ownCertFile = NULL;
	} else {
		if((pThis->ownCertFile = strdup(file)) == NULL)
			ABORT_FINALIZE(RELP_RET_OUT_OF_MEMORY);
	}
finalize_it:
	LEAVE_RELPFUNC;
}
relpRetVal
relpCltSetPrivKey(relpClt_t *pThis, char *file)
{
	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Clt);
	free(pThis->privKey);
	if(file == NULL) {
		pThis->privKey = NULL;
	} else {
		if((pThis->privKey = strdup(file)) == NULL)
			ABORT_FINALIZE(RELP_RET_OUT_OF_MEMORY);
	}
finalize_it:
	LEAVE_RELPFUNC;
}
/* Enable TLS mode. */
relpRetVal
relpCltEnableTLS(relpClt_t *pThis)
{
	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Clt);
	pThis->bEnableTLS = 1;
	LEAVE_RELPFUNC;
}

relpRetVal
relpCltEnableTLSZip(relpClt_t *pThis)
{
	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Clt);
	pThis->bEnableTLSZip = 1;
	LEAVE_RELPFUNC;
}

/* this function is called to hint librelp that a "burst" of data is to be
 * sent. librelp can than try to optimize it's handling.
 * The function is intentionally void as it must operate in a way that
 * does not interfere with normal operations.
 */
void
relpCltHintBurstBegin(relpClt_t *pThis)
{
	relpTcpHintBurstBegin(pThis->pSess->pTcp);
}
/* this is the counterpart to relpCltHintBurstBegin -- see there for doc */
void
relpCltHintBurstEnd(relpClt_t *pThis)
{
	relpTcpHintBurstEnd(pThis->pSess->pTcp);
}

/* Send a syslog message through RELP. The session must be established.
 * The provided message buffer is not touched by this function. The caller
 * must free it if it is no longer needed.
 * rgerhards, 2008-03-20
 */
relpRetVal
relpCltSendSyslog(relpClt_t *pThis, unsigned char *pMsg, size_t lenMsg)
{

	ENTER_RELPFUNC;
	if(pThis == NULL || pThis->objID != eRelpObj_Clt)
		ABORT_FINALIZE(RELP_RET_INVALID_HDL);

	CHKRet(relpSessSendSyslog(pThis->pSess, pMsg, lenMsg));

finalize_it:
	LEAVE_RELPFUNC;
}
