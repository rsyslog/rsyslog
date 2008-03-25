/* The RELP (reliable event logging protocol) core protocol library.
 *
 * This file is meant to be included by applications using the relp library.
 * For relp library files themselves, include "relp.h".
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
#ifndef LIBRELP_H_INCLUDED
#define	LIBRELP_H_INCLUDED


/* define some of our types that a caller must know about */
typedef unsigned char relpOctet_t;
typedef int relpTxnr_t;
typedef int relpRetVal;

/* our objects (forward definitions) */
typedef struct relpEngine_s relpEngine_t;
typedef struct relpClt_s relpClt_t;
typedef struct relpSrv_s relpSrv_t;
typedef struct relpSess_s relpSess_t;
typedef struct relpFrame_s relpFrame_t;
typedef struct relpSendbuf_s relpSendbuf_t;
typedef struct relpOffers_s relpOffers_t;
typedef struct relpOffer_s relpOffer_t;

/* IDs of librelp objects */
typedef enum relpObjID_e {
	eRelpObj_Invalid = 0,	/**< invalid object, must be zero to detect unitilized value */
	eRelpObj_Engine = 1,
	eRelpObj_Sess = 2,
	eRelpObj_Frame = 3,
	eRelpObj_Clt = 4,
	eRelpObj_Srv = 5,
	eRelpObj_Sendq = 6,
	eRelpObj_Sendqe = 7,
	eRelpObj_Sendbuf = 8,
	eRelpObj_Tcp = 9,
	eRelpObj_Offers = 10,
	eRelpObj_Offer = 11,
	eRelpObj_OfferValue = 12
} relpObjID_t;



/* macro to assert we are dealing with the right relp object */
#ifdef NDEBUG
#	define RELPOBJ_assert(obj, type)
#else /* debug case */
#	define RELPOBJ_assert(pObj, type) \
		assert((pObj) != NULL); \
		assert((pObj)->objID == eRelpObj_##type)

#endif /* # ifdef NDEBUG */


/* now define our externally-visible error codes */
#ifndef RELPERR_BASE
	/* provide a basis for error numbers if not configured */
#	define RELPERR_BASE 10000
#endif

/* we may argue if RELP_RET_OK should also be relative to RELPERR_BASE. I have deciced against it,
 * because if it is 0, we can use it together with other project's iRet mechanisms, which is quite
 * useful. -- rgerhards, 2008-03-17
 */
#define RELP_RET_OK		0			/**< everything went well, no error */
#define RELP_RET_OUT_OF_MEMORY	RELPERR_BASE + 1	/**< out of memory occured */
#define RELP_RET_INVALID_FRAME	RELPERR_BASE + 2	/**< relp frame received is invalid */
#define RELP_RET_PARAM_ERROR	RELPERR_BASE + 3	/**< an (API) calling parameer is in error */
#define RELP_RET_INVALID_PORT	RELPERR_BASE + 4	/**< invalid port value */
#define RELP_RET_COULD_NOT_BIND	RELPERR_BASE + 5	/**< could not bind socket, defunct */
#define RELP_RET_ACCEPT_ERR	RELPERR_BASE + 6	/**< error during accept() system call */
#define RELP_RET_SESSION_BROKEN	RELPERR_BASE + 7	/**< the RELP session is broken */
#define RELP_RET_SESSION_CLOSED	RELPERR_BASE + 8	/**< the RELP session was closed (not an error) */
#define RELP_RET_INVALID_CMD	RELPERR_BASE + 9	/**< the command contained in a RELP frame was unknown */
#define RELP_RET_DATA_TOO_LONG	RELPERR_BASE + 10	/**< DATALEN exceeds permitted length */
#define RELP_RET_INVALID_TXNR	RELPERR_BASE + 11	/**< a txnr is invalid (probably code error) */
#define RELP_RET_INVALID_DATALEN RELPERR_BASE + 12	/**< DATALEN field is invalid (probably code error) */
#define RELP_RET_PARTIAL_WRITE  RELPERR_BASE + 13	/**< only partial data written (state, not an error) */
#define RELP_RET_IO_ERR         RELPERR_BASE + 14	/**< IO error occured */
#define RELP_RET_TIMED_OUT      RELPERR_BASE + 15	/**< timeout occured */
#define RELP_RET_NOT_FOUND      RELPERR_BASE + 16	/**< searched entity not found */
#define RELP_RET_NOT_IMPLEMENTED RELPERR_BASE + 17	/**< functionality not implemented */
#define RELP_RET_INVALID_RSPHDR RELPERR_BASE + 18	/**< "rsp" packet header is invalid */
#define RELP_RET_END_OF_DATA    RELPERR_BASE + 19	/**< no more data available */
#define RELP_RET_RSP_STATE_ERR	RELPERR_BASE + 20	/**< error status in relp rsp frame */
#define RELP_RET_INVALID_OFFER	RELPERR_BASE + 21	/**< invalid offer (e.g. malformed) during open */
#define RELP_RET_UNKNOWN_CMD	RELPERR_BASE + 22	/**< command is unknown (e.g. not in this version) */
#define RELP_RET_CMD_DISABLED	RELPERR_BASE + 23	/**< tried to use a cmd that is disabled in this session */

/* some macros to work with librelp error codes */
#define CHKRet(code) if((iRet = code) != RELP_RET_OK) goto finalize_it
/* macro below is to be used if we need our own handling, eg for cleanup */
#define CHKRet_Hdlr(code) if((iRet = code) != RELP_RET_OK)

/* prototypes needed by library users */
const char *relpEngineGetVersion(void); /* use this entry point for configure check */
relpRetVal relpEngineConstruct(relpEngine_t **ppThis);
relpRetVal relpEngineDestruct(relpEngine_t **ppThis);
relpRetVal relpEngineSetDbgprint(relpEngine_t *pThis, void (*dbgprint)(char *fmt, ...) __attribute__((format(printf, 1, 2))));
relpRetVal relpEngineAddListner(relpEngine_t *pThis, unsigned char *pLstnPort);
relpRetVal relpEngineRun(relpEngine_t *pThis);
relpRetVal relpEngineCltDestruct(relpEngine_t *pThis, relpClt_t **ppClt);
relpRetVal relpEngineCltConstruct(relpEngine_t *pThis, relpClt_t **ppClt);
relpRetVal relpEngineSetSyslogRcv(relpEngine_t *pThis, relpRetVal (*pCB)(unsigned char*, size_t));

/* exposed relp client functions */
relpRetVal relpCltConnect(relpClt_t *pThis, int protFamily, unsigned char *port, unsigned char *host);
relpRetVal relpCltSendSyslog(relpClt_t *pThis, unsigned char *pMsg, size_t lenMsg);
relpRetVal relpCltReconnect(relpClt_t *pThis);


#endif /* #ifndef RELP_H_INCLUDED */
