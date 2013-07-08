/* The RELP (reliable event logging protocol) core protocol library.
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
#ifndef RELP_H_INCLUDED
#define	RELP_H_INCLUDED

#include <pthread.h>
#if HAVE_SYS_EPOLL_H
#	include <sys/epoll.h>
#endif

#include "librelp.h"

/* "config params" - these can be overridden by defining the
 * respective constants at compile time.
 */
#ifndef DEFAULT_DH_BITS /* security param: default Diffie-Hellman bits to use */
#  define DEFAULT_DH_BITS 1024
#endif

/* the following macro MUST be specified as the first member of each
 * RELP object.
 */
#define BEGIN_RELP_OBJ relpObjID_t objID

/* the core intializer to call on top of each constructure (right after the calloc) */
#define RELP_CORE_CONSTRUCTOR(pObj, objType) \
	(pObj)->objID = eRelpObj_##objType

/* a structure to store permitted peer information (a type of ACL) */
typedef struct relpPermittedPeers_s {
	int nmemb;
	char **name;
} relpPermittedPeers_t;

#if defined(HAVE_EPOLL_CREATE1) || defined(HAVE_EPOLL_CREATE)
typedef struct epolld_s epolld_t;
#endif
/* a linked list entry for the list of relp servers (of this engine) */
typedef struct relpEngSrvLst_s {
	struct relpEngSrvLst_s *pPrev;
	struct relpEngSrvLst_s *pNext;
#	if defined(HAVE_EPOLL_CREATE1) || defined(HAVE_EPOLL_CREATE)
	epolld_t **epevts;
#	endif
	struct relpSrv_s *pSrv;
} relpEngSrvLst_t;

#if defined(HAVE_EPOLL_CREATE1) || defined(HAVE_EPOLL_CREATE)
/* type of object stored in epoll descriptor */
typedef enum {
	epolld_lstn,
	epolld_sess
} epolld_type_t;

/* an epoll descriptor. contains all information necessary to process
 * the result of epoll.
 */
struct epolld_s {
	epolld_type_t typ;
	void *ptr;
	int sock;
	struct epoll_event ev;
};
#endif


/* a linked list entry for the list of relp sessions (of this engine) */
typedef struct relpEngSessLst_s {
	struct relpEngSessLst_s *pPrev;
	struct relpEngSessLst_s *pNext;
	struct relpSess_s *pSess;
#	if defined(HAVE_EPOLL_CREATE1) || defined(HAVE_EPOLL_CREATE)
	enum { epoll_wronly, epoll_rdonly, epoll_rdwr } epollState;
	epolld_t *epevt;
#	endif
} relpEngSessLst_t;


/* the RELP engine object 
 * Having a specific engine object enables multiple plugins to call the
 * RELP engine at the same time. The core idea of librelp is to have no
 * static data at all and everything stored in RELP engine objects. Every
 * program/plugin should aquire a RELP engine once on startup (or first need)
 * and release it only when it is fully done with RELP, which usually means
 * on shutdown.
 * rgerhards, 2008-03-16
 */
struct relpEngine_s {
	BEGIN_RELP_OBJ;
	void (*dbgprint)(char *fmt, ...) __attribute__((format(printf, 1, 2)));
	relpRetVal (*onSyslogRcv)(unsigned char*pHostname, unsigned char *pIP,
		                  unsigned char *pMsg, size_t lenMsg); /**< callback for "syslog" cmd */
	relpRetVal (*onSyslogRcv2)(void*, unsigned char*pHostname, unsigned char *pIP,
		                  unsigned char *pMsg, size_t lenMsg); /**< callback for "syslog" cmd */
	void (*onAuthErr)(void*pUsr, char *authinfo, char*errmsg, relpRetVal errcode);
	void (*onErr)(void*pUsr, char *objinfo, char*errmsg, relpRetVal errcode);
	void (*onGenericErr)(char *objinfo, char*errmsg, relpRetVal errcode);
	int protocolVersion; /**< version of the relp protocol supported by this engine */

	/* Flags */
	int bEnableDns; /**< enabled DNS lookups 0 - no, 1 - yes */
	int bAcceptSessFromMalDnsHost; /**< accept session from host with malicious DNS? (0-no, 1-yes) */
	int ai_family;	/**< to support IPv4/v6 modes */

	/* default for enabled commands */
	relpCmdEnaState_t stateCmdSyslog;

	/* linked list of our servers */
	relpEngSrvLst_t *pSrvLstRoot;
	relpEngSrvLst_t *pSrvLstLast;
	int lenSrvLst;
	pthread_mutex_t mutSrvLst;

	/* linked list of our sessions */
	relpEngSessLst_t *pSessLstRoot;
	relpEngSessLst_t *pSessLstLast;
	int lenSessLst;
	pthread_mutex_t mutSessLst;

#	if defined(HAVE_EPOLL_CREATE1) || defined(HAVE_EPOLL_CREATE)
	int efd;	/**< file descriptor for epoll */
#	endif
	int bStop;	/* set to 1 to stop server after next select */
	int *bShutdownImmdt; /* if non-NULL provides a kind of "external" */
		/* bStop functionality. This is in support for rsyslog,
		 * whom's output interface is not capable of calling into
		 * librelp at time of stop request.
		 */
};


/* macro to assert we are dealing with the right relp object */
#ifdef NDEBUG
#	define RELPOBJ_assert(obj, type)
#else /* debug case */
#	define RELPOBJ_assert(pObj, type) \
		assert((pObj) != NULL); \
		assert((pObj)->objID == eRelpObj_##type)

#endif /* # ifdef NDEBUG */


#define RELP_CURR_PROTOCOL_VERSION 0

/* some defines that may also come from the config */
#ifndef RELP_DFLT_PORT
#	define RELP_DFLT_PORT 20514
#endif
#ifndef RELP_DFLT_MAX_DATA_SIZE
#	define RELP_DFLT_MAX_DATA_SIZE 128 * 1024 /* 128K should be sufficient for everything... */
#endif
#ifndef RELP_DFLT_WINDOW_SIZE
#	define RELP_DFLT_WINDOW_SIZE 128 /* 128 unacked frames should be fairly good in most cases */
#endif

/* set the default receive buffer size if none is externally configured
 * NOTE: do not define to less than 1.5K or you'll probably see a severe
 * performance hit!
 */
#ifndef	RELP_RCV_BUF_SIZE
#	define RELP_RCV_BUF_SIZE 32 * 1024 /* 32K */
#endif


/* some macros to work with librelp error codes */
#define CHKRet(code) if((iRet = code) != RELP_RET_OK) goto finalize_it
#define CHKmalloc(r) if((r) == NULL) { iRet = RELP_RET_OUT_OF_MEMORY; goto finalize_it; }
/* macro below is to be used if we need our own handling, eg for cleanup */
#define CHKRet_Hdlr(code) if((iRet = code) != RELP_RET_OK)
/* macro below is used in conjunction with CHKiRet_Hdlr, else use ABORT_FINALIZE */
#define FINALIZE goto finalize_it;
#define ENTER_RELPFUNC relpRetVal iRet = RELP_RET_OK
#define LEAVE_RELPFUNC return iRet
#define ABORT_FINALIZE(errCode)			\
	do {					\
		iRet = errCode;			\
		goto finalize_it;		\
	} while (0)


/* some macro-implemented functionality of the RELP engine */
#define relpEngineNextTXNR(txnr) \
	((txnr > 999999999) ? 1 : txnr + 1)

static inline int relpEngineShouldStop(relpEngine_t *pThis) {
//pThis->dbgprint("DDDD: librelp bStop %d, ShutdownImmdt %p, immdet result %d\n", pThis->bStop, pThis->bShutdownImmdt, (pThis->bShutdownImmdt == NULL) ? 0 : *pThis->bShutdownImmdt);
	return     pThis->bStop
	       || (pThis->bShutdownImmdt != NULL && *pThis->bShutdownImmdt);
}

/* prototypes needed by library itself (rest is in librelp.h) */
relpRetVal relpEngineDispatchFrame(relpEngine_t *pThis, relpSess_t *pSess, relpFrame_t *pFrame);
void __attribute__((format(printf, 4, 5))) relpEngineCallOnGenericErr(relpEngine_t *pThis, char *eobj, relpRetVal ecode, char *fmt, ...);

#endif /* #ifndef RELP_H_INCLUDED */
