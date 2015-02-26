/* omfwd.c
 * This is the implementation of the build-in forwarding output module.
 *
 * NOTE: read comments in module-template.h to understand how this file
 *       works!
 *
 * Copyright 2007-2015 Adiscon GmbH.
 *
 * This file is part of rsyslog.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *       http://www.apache.org/licenses/LICENSE-2.0
 *       -or-
 *       see COPYING.ASL20 in the source distribution
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "config.h"
#include "rsyslog.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <netinet/in.h>
#include <netdb.h>
#include <fnmatch.h>
#include <assert.h>
#include <errno.h>
#include <ctype.h>
#include <unistd.h>
#include <stdint.h>
#include <zlib.h>
#include <pthread.h>
#include "syslogd.h"
#include "conf.h"
#include "syslogd-types.h"
#include "srUtils.h"
#include "net.h"
#include "netstrms.h"
#include "netstrm.h"
#include "omfwd.h"
#include "template.h"
#include "msg.h"
#include "tcpclt.h"
#include "cfsysline.h"
#include "module-template.h"
#include "glbl.h"
#include "errmsg.h"
#include "unicode-helper.h"

MODULE_TYPE_OUTPUT
MODULE_TYPE_NOKEEP
MODULE_CNFNAME("omfwd")

/* internal structures
 */
DEF_OMOD_STATIC_DATA
DEFobjCurrIf(errmsg)
DEFobjCurrIf(glbl)
DEFobjCurrIf(net)
DEFobjCurrIf(netstrms)
DEFobjCurrIf(netstrm)
DEFobjCurrIf(tcpclt)


/* some local constants (just) for better readybility */
#define IS_FLUSH 1
#define NO_FLUSH 0

typedef struct _instanceData {
	uchar 	*tplName;	/* name of assigned template */
	uchar *pszStrmDrvr;
	uchar *pszStrmDrvrAuthMode;
	permittedPeers_t *pPermPeers;
	int iStrmDrvrMode;
	char	*target;
	int compressionLevel;	/* 0 - no compression, else level for zlib */
	char *port;
	int protocol;
	int iRebindInterval;	/* rebind interval */
#	define	FORW_UDP 0
#	define	FORW_TCP 1
	/* following fields for UDP-based delivery */
	int bSendToAll;
	int iUDPSendDelay;
	/* following fields for TCP-based delivery */
	TCPFRAMINGMODE tcp_framing;
	int bResendLastOnRecon; /* should the last message be re-sent on a successful reconnect? */
#	define COMPRESS_NEVER 0
#	define COMPRESS_SINGLE_MSG 1	/* old, single-message compression */
	/* all other settings are for stream-compression */
#	define COMPRESS_STREAM_ALWAYS 2
	uint8_t compressionMode;
	int errsToReport;	/* max number of errors to report (per instance) */
	sbool strmCompFlushOnTxEnd; /* flush stream compression on transaction end? */
} instanceData;

typedef struct wrkrInstanceData {
	instanceData *pData;
	netstrms_t *pNS; /* netstream subsystem */
	netstrm_t *pNetstrm; /* our output netstream */
	struct addrinfo *f_addr;
	int *pSockArray;	/* sockets to use for UDP */
	int bIsConnected;  /* are we connected to remote host? 0 - no, 1 - yes, UDP means addr resolved */
	int nXmit;		/* number of transmissions since last (re-)bind */
	tcpclt_t *pTCPClt;	/* our tcpclt object */
	sbool bzInitDone; /* did we do an init of zstrm already? */
	z_stream zstrm;	/* zip stream to use for tcp compression */
	uchar sndBuf[16*1024];	/* this is intensionally fixed -- see no good reason to make configurable */
	unsigned offsSndBuf;	/* next free spot in send buffer */
	int errsToReport;	/* (remaining) number of errors to report */
} wrkrInstanceData_t;

/* config data */
typedef struct configSettings_s {
	uchar *pszTplName; /* name of the default template to use */
	uchar *pszStrmDrvr; /* name of the stream driver to use */
	int iStrmDrvrMode; /* mode for stream driver, driver-dependent (0 mostly means plain tcp) */
	int bResendLastOnRecon; /* should the last message be re-sent on a successful reconnect? */
	uchar *pszStrmDrvrAuthMode; /* authentication mode to use */
	int iTCPRebindInterval;	/* support for automatic re-binding (load balancers!). 0 - no rebind */
	int iUDPRebindInterval;	/* support for automatic re-binding (load balancers!). 0 - no rebind */
	permittedPeers_t *pPermPeers;
} configSettings_t;
static configSettings_t cs;

/* tables for interfacing with the v6 config system */
/* module-global parameters */
static struct cnfparamdescr modpdescr[] = {
	{ "template", eCmdHdlrGetWord, 0 },
};
static struct cnfparamblk modpblk =
	{ CNFPARAMBLK_VERSION,
	  sizeof(modpdescr)/sizeof(struct cnfparamdescr),
	  modpdescr
	};

/* action (instance) parameters */
static struct cnfparamdescr actpdescr[] = {
	{ "target", eCmdHdlrGetWord, 0 },
	{ "port", eCmdHdlrGetWord, 0 },
	{ "protocol", eCmdHdlrGetWord, 0 },
	{ "tcp_framing", eCmdHdlrGetWord, 0 },
	{ "ziplevel", eCmdHdlrInt, 0 },
	{ "compression.mode", eCmdHdlrGetWord, 0 },
	{ "compression.stream.flushontxend", eCmdHdlrBinary, 0 },
	{ "maxerrormessages", eCmdHdlrInt, 0 },
	{ "rebindinterval", eCmdHdlrInt, 0 },
	{ "streamdriver", eCmdHdlrGetWord, 0 },
	{ "streamdrivermode", eCmdHdlrInt, 0 },
	{ "streamdriverauthmode", eCmdHdlrGetWord, 0 },
	{ "streamdriverpermittedpeers", eCmdHdlrGetWord, 0 },
	{ "resendlastmsgonreconnect", eCmdHdlrBinary, 0 },
	{ "udp.sendtoall", eCmdHdlrBinary, 0 },
	{ "udp.senddelay", eCmdHdlrInt, 0 },
	{ "template", eCmdHdlrGetWord, 0 },
};
static struct cnfparamblk actpblk =
	{ CNFPARAMBLK_VERSION,
	  sizeof(actpdescr)/sizeof(struct cnfparamdescr),
	  actpdescr
	};

struct modConfData_s {
	rsconf_t *pConf;	/* our overall config object */
	uchar 	*tplName;	/* default template */
};

static modConfData_t *loadModConf = NULL;/* modConf ptr to use for the current load process */
static modConfData_t *runModConf = NULL;/* modConf ptr to use for the current exec process */


static rsRetVal initTCP(wrkrInstanceData_t *pWrkrData);


BEGINinitConfVars		/* (re)set config variables to default values */
CODESTARTinitConfVars 
	cs.pszTplName = NULL; /* name of the default template to use */
	cs.pszStrmDrvr = NULL; /* name of the stream driver to use */
	cs.iStrmDrvrMode = 0; /* mode for stream driver, driver-dependent (0 mostly means plain tcp) */
	cs.bResendLastOnRecon = 0; /* should the last message be re-sent on a successful reconnect? */
	cs.pszStrmDrvrAuthMode = NULL; /* authentication mode to use */
	cs.iUDPRebindInterval = 0;	/* support for automatic re-binding (load balancers!). 0 - no rebind */
	cs.iTCPRebindInterval = 0;	/* support for automatic re-binding (load balancers!). 0 - no rebind */
	cs.pPermPeers = NULL;
ENDinitConfVars


static rsRetVal doTryResume(wrkrInstanceData_t *);
static rsRetVal doZipFinish(wrkrInstanceData_t *);

/* this function gets the default template. It coordinates action between
 * old-style and new-style configuration parts.
 */
static inline uchar*
getDfltTpl(void)
{
	if(loadModConf != NULL && loadModConf->tplName != NULL)
		return loadModConf->tplName;
	else if(cs.pszTplName == NULL)
		return (uchar*)"RSYSLOG_TraditionalForwardFormat";
	else
		return cs.pszTplName;
}


/* set the default template to be used
 * This is a module-global parameter, and as such needs special handling. It needs to
 * be coordinated with values set via the v2 config system (rsyslog v6+). What we do
 * is we do not permit this directive after the v2 config system has been used to set
 * the parameter.
 */
static rsRetVal
setLegacyDfltTpl(void __attribute__((unused)) *pVal, uchar* newVal)
{
	DEFiRet;

	if(loadModConf != NULL && loadModConf->tplName != NULL) {
		free(newVal);
		errmsg.LogError(0, RS_RET_ERR, "omfwd default template already set via module "
			"global parameter - can no longer be changed");
		ABORT_FINALIZE(RS_RET_ERR);
	}
	free(cs.pszTplName);
	cs.pszTplName = newVal;
finalize_it:
	RETiRet;
}

/* Close the UDP sockets.
 * rgerhards, 2009-05-29
 */
static rsRetVal
closeUDPSockets(wrkrInstanceData_t *pWrkrData)
{
	DEFiRet;
	if(pWrkrData->pSockArray != NULL) {
		net.closeUDPListenSockets(pWrkrData->pSockArray);
		pWrkrData->pSockArray = NULL;
		freeaddrinfo(pWrkrData->f_addr);
		pWrkrData->f_addr = NULL;
	}
pWrkrData->bIsConnected = 0; // TODO: remove this variable altogether
	RETiRet;
}


/* destruct the TCP helper objects
 * This, for example, is needed after something went wrong.
 * This function is void because it "can not" fail.
 * rgerhards, 2008-06-04
 * Note that we DO NOT discard the current buffer contents
 * (if any). This permits us to save data between sessions. In
 * the worst case, some duplication occurs, but we do not
 * loose data.
 */
static inline void
DestructTCPInstanceData(wrkrInstanceData_t *pWrkrData)
{
	doZipFinish(pWrkrData);
	if(pWrkrData->pNetstrm != NULL)
		netstrm.Destruct(&pWrkrData->pNetstrm);
	if(pWrkrData->pNS != NULL)
		netstrms.Destruct(&pWrkrData->pNS);
}


BEGINbeginCnfLoad
CODESTARTbeginCnfLoad
	loadModConf = pModConf;
	pModConf->pConf = pConf;
	pModConf->tplName = NULL;
ENDbeginCnfLoad

BEGINsetModCnf
	struct cnfparamvals *pvals = NULL;
	int i;
CODESTARTsetModCnf
	if((pvals = nvlstGetParams(lst, &modpblk, NULL))) {
		ABORT_FINALIZE(RS_RET_MISSING_CNFPARAMS);
	}

	if(Debug) {
		dbgprintf("module (global) param blk for omfwd:\n");
		cnfparamsPrint(&modpblk, pvals);
	}

	for(i = 0 ; i < modpblk.nParams ; ++i) {
		if(!pvals[i].bUsed)
			continue;
		if(!strcmp(modpblk.descr[i].name, "template")) {
			loadModConf->tplName = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
			if(cs.pszTplName != NULL) {
				errmsg.LogError(0, RS_RET_DUP_PARAM, "omfwd: warning: default template "
						"was already set via legacy directive - may lead to inconsistent "
						"results.");
			}
		} else {
			dbgprintf("omfwd: program error, non-handled "
			  "param '%s' in beginCnfLoad\n", modpblk.descr[i].name);
		}
	}
finalize_it:
	if(pvals != NULL)
		cnfparamvalsDestruct(pvals, &modpblk);
ENDsetModCnf

BEGINendCnfLoad
CODESTARTendCnfLoad
	loadModConf = NULL; /* done loading */
	/* free legacy config vars */
	free(cs.pszTplName);
	cs.pszTplName = NULL;
ENDendCnfLoad

BEGINcheckCnf
CODESTARTcheckCnf
ENDcheckCnf

BEGINactivateCnf
CODESTARTactivateCnf
	runModConf = pModConf;
ENDactivateCnf

BEGINfreeCnf
CODESTARTfreeCnf
	free(pModConf->tplName);
ENDfreeCnf

BEGINcreateInstance
CODESTARTcreateInstance
	pData->errsToReport = 5;
	if(cs.pszStrmDrvr != NULL)
		CHKmalloc(pData->pszStrmDrvr = (uchar*)strdup((char*)cs.pszStrmDrvr));
	if(cs.pszStrmDrvrAuthMode != NULL)
		CHKmalloc(pData->pszStrmDrvrAuthMode =
				     (uchar*)strdup((char*)cs.pszStrmDrvrAuthMode));
finalize_it:
ENDcreateInstance


BEGINcreateWrkrInstance
CODESTARTcreateWrkrInstance
	dbgprintf("DDDD: createWrkrInstance: pWrkrData %p\n", pWrkrData);
	pWrkrData->offsSndBuf = 0;
	pWrkrData->errsToReport = pData->errsToReport;
	iRet = initTCP(pWrkrData);
ENDcreateWrkrInstance


BEGINisCompatibleWithFeature
CODESTARTisCompatibleWithFeature
	if(eFeat == sFEATURERepeatedMsgReduction)
		iRet = RS_RET_OK;
ENDisCompatibleWithFeature


BEGINfreeInstance
CODESTARTfreeInstance
	free(pData->pszStrmDrvr);
	free(pData->pszStrmDrvrAuthMode);
	free(pData->port);
	free(pData->target);
	net.DestructPermittedPeers(&pData->pPermPeers);
ENDfreeInstance


BEGINfreeWrkrInstance
CODESTARTfreeWrkrInstance
	DestructTCPInstanceData(pWrkrData);
	closeUDPSockets(pWrkrData);

	if(pWrkrData->pData->protocol == FORW_TCP) {
		tcpclt.Destruct(&pWrkrData->pTCPClt);
	}
ENDfreeWrkrInstance


BEGINdbgPrintInstInfo
CODESTARTdbgPrintInstInfo
	dbgprintf("%s", pData->target);
ENDdbgPrintInstInfo


/* Send a message via UDP
 * rgehards, 2007-12-20
 */
static rsRetVal UDPSend(wrkrInstanceData_t *__restrict__ const pWrkrData,
	uchar *__restrict__ const msg,
	const size_t len)
{
	DEFiRet;
	struct addrinfo *r;
	int i;
	unsigned lsent = 0;
	sbool bSendSuccess;
	int lasterrno = ENOENT;
	char errStr[1024];

	if(pWrkrData->pData->iRebindInterval && (pWrkrData->nXmit++ % pWrkrData->pData->iRebindInterval == 0)) {
		dbgprintf("omfwd dropping UDP 'connection' (as configured)\n");
		pWrkrData->nXmit = 1;	/* else we have an addtl wrap at 2^31-1 */
		CHKiRet(closeUDPSockets(pWrkrData));
	}

	if(pWrkrData->pSockArray == NULL) {
		CHKiRet(doTryResume(pWrkrData));
	}

	if(pWrkrData->pSockArray != NULL) {
		/* we need to track if we have success sending to the remote
		 * peer. Success is indicated by at least one sendto() call
		 * succeeding. We track this be bSendSuccess. We can not simply
		 * rely on lsent, as a call might initially work, but a later
		 * call fails. Then, lsent has the error status, even though
		 * the sendto() succeeded. -- rgerhards, 2007-06-22
		 */
		bSendSuccess = RSFALSE;
		for (r = pWrkrData->f_addr; r; r = r->ai_next) {
			for (i = 0; i < *pWrkrData->pSockArray; i++) {
			       lsent = sendto(pWrkrData->pSockArray[i+1], msg, len, 0, r->ai_addr, r->ai_addrlen);
				if (lsent == len) {
					bSendSuccess = RSTRUE;
					break;
				} else {
					lasterrno = errno;
					DBGPRINTF("sendto() error: %d = %s.\n",
						lasterrno,
						rs_strerror_r(lasterrno, errStr, sizeof(errStr)));
				}
			}
			if (lsent == len && !pWrkrData->pData->bSendToAll)
			       break;
		}
		/* finished looping */
		if(bSendSuccess == RSTRUE) {
			if(pWrkrData->pData->iUDPSendDelay > 0) {
				srSleep(pWrkrData->pData->iUDPSendDelay / 1000000,
				        pWrkrData->pData->iUDPSendDelay % 1000000);
				}
		} else {
			dbgprintf("error forwarding via udp, suspending\n");
			if(pWrkrData->errsToReport > 0) {
				errmsg.LogError(lasterrno, RS_RET_ERR_UDPSEND,
						"omfwd: error %d sending "
						"via udp", lasterrno);
				if(pWrkrData->errsToReport == 1) {
					errmsg.LogMsg(0, RS_RET_LAST_ERRREPORT, LOG_WARNING, "omfwd: "
							"max number of error message emitted "
							"- further messages will be "
							"suppressed");
				}
				--pWrkrData->errsToReport;
			}
			iRet = RS_RET_SUSPENDED;
		}
	}

finalize_it:
	RETiRet;
}


/* set the permitted peers -- rgerhards, 2008-05-19
 */
static rsRetVal
setPermittedPeer(void __attribute__((unused)) *pVal, uchar *pszID)
{
	DEFiRet;
	CHKiRet(net.AddPermittedPeer(&cs.pPermPeers, pszID));
	free(pszID); /* no longer needed, but we must free it as of interface def */
finalize_it:
	RETiRet;
}



/* CODE FOR SENDING TCP MESSAGES */

static rsRetVal
TCPSendBufUncompressed(wrkrInstanceData_t *pWrkrData, uchar *buf, unsigned len)
{
	DEFiRet;
	unsigned alreadySent;
	ssize_t lenSend;

	alreadySent = 0;
	CHKiRet(netstrm.CheckConnection(pWrkrData->pNetstrm)); /* hack for plain tcp syslog - see ptcp driver for details */

	while(alreadySent != len) {
		lenSend = len - alreadySent;
		CHKiRet(netstrm.Send(pWrkrData->pNetstrm, buf+alreadySent, &lenSend));
		DBGPRINTF("omfwd: TCP sent %ld bytes, requested %u\n", (long) lenSend, len - alreadySent);
		alreadySent += lenSend;
	}

finalize_it:
	if(iRet != RS_RET_OK) {
		/* error! */
		dbgprintf("TCPSendBuf error %d, destruct TCP Connection!\n", iRet);
		DestructTCPInstanceData(pWrkrData);
		iRet = RS_RET_SUSPENDED;
	}
	RETiRet;
}

static rsRetVal
TCPSendBufCompressed(wrkrInstanceData_t *pWrkrData, uchar *buf, unsigned len, sbool bIsFlush)
{
	int zRet;	/* zlib return state */
	unsigned outavail;
	uchar zipBuf[32*1024];
	int op;
	DEFiRet;

	if(!pWrkrData->bzInitDone) {
		/* allocate deflate state */
		pWrkrData->zstrm.zalloc = Z_NULL;
		pWrkrData->zstrm.zfree = Z_NULL;
		pWrkrData->zstrm.opaque = Z_NULL;
		/* see note in file header for the params we use with deflateInit2() */
		zRet = deflateInit(&pWrkrData->zstrm, 9);
		if(zRet != Z_OK) {
			DBGPRINTF("error %d returned from zlib/deflateInit()\n", zRet);
			ABORT_FINALIZE(RS_RET_ZLIB_ERR);
		}
		pWrkrData->bzInitDone = RSTRUE;
	}

	/* now doing the compression */
	pWrkrData->zstrm.next_in = (Bytef*) buf;
	pWrkrData->zstrm.avail_in = len;
	if(pWrkrData->pData->strmCompFlushOnTxEnd && bIsFlush)
		op = Z_SYNC_FLUSH;
	else
		op = Z_NO_FLUSH;
	/* run deflate() on buffer until everything has been compressed */
	do {
		DBGPRINTF("omfwd: in deflate() loop, avail_in %d, total_in %ld, isFlush %d\n", pWrkrData->zstrm.avail_in, pWrkrData->zstrm.total_in, bIsFlush);
		pWrkrData->zstrm.avail_out = sizeof(zipBuf);
		pWrkrData->zstrm.next_out = zipBuf;
		zRet = deflate(&pWrkrData->zstrm, op);    /* no bad return value */
		DBGPRINTF("after deflate, ret %d, avail_out %d\n", zRet, pWrkrData->zstrm.avail_out);
		outavail = sizeof(zipBuf) - pWrkrData->zstrm.avail_out;
		if(outavail != 0) {
			CHKiRet(TCPSendBufUncompressed(pWrkrData, zipBuf, outavail));
		}
	} while (pWrkrData->zstrm.avail_out == 0);

finalize_it:
	RETiRet;
}

static rsRetVal
TCPSendBuf(wrkrInstanceData_t *pWrkrData, uchar *buf, unsigned len, sbool bIsFlush)
{
	DEFiRet;
	if(pWrkrData->pData->compressionMode >= COMPRESS_STREAM_ALWAYS)
		iRet = TCPSendBufCompressed(pWrkrData, buf, len, bIsFlush);
	else
		iRet = TCPSendBufUncompressed(pWrkrData, buf, len);
	RETiRet;
}

/* finish zlib buffer, to be called before closing the ZIP file (if
 * running in stream mode).
 */
static rsRetVal
doZipFinish(wrkrInstanceData_t *pWrkrData)
{
	int zRet;	/* zlib return state */
	DEFiRet;
	unsigned outavail;
	uchar zipBuf[32*1024];

	if(!pWrkrData->bzInitDone)
		goto done;

// TODO: can we get this into a single common function?
dbgprintf("DDDD: in doZipFinish()\n");
	pWrkrData->zstrm.avail_in = 0;
	/* run deflate() on buffer until everything has been compressed */
	do {
		DBGPRINTF("in deflate() loop, avail_in %d, total_in %ld\n", pWrkrData->zstrm.avail_in, pWrkrData->zstrm.total_in);
		pWrkrData->zstrm.avail_out = sizeof(zipBuf);
		pWrkrData->zstrm.next_out = zipBuf;
		zRet = deflate(&pWrkrData->zstrm, Z_FINISH);    /* no bad return value */
		DBGPRINTF("after deflate, ret %d, avail_out %d\n", zRet, pWrkrData->zstrm.avail_out);
		outavail = sizeof(zipBuf) - pWrkrData->zstrm.avail_out;
		if(outavail != 0) {
			CHKiRet(TCPSendBufUncompressed(pWrkrData, zipBuf, outavail));
		}
	} while (pWrkrData->zstrm.avail_out == 0);

finalize_it:
	zRet = deflateEnd(&pWrkrData->zstrm);
	if(zRet != Z_OK) {
		DBGPRINTF("error %d returned from zlib/deflateEnd()\n", zRet);
	}

	pWrkrData->bzInitDone = 0;
done:	RETiRet;
}


/* Add frame to send buffer (or send, if requried)
 */
static rsRetVal TCPSendFrame(void *pvData, char *msg, size_t len)
{
	DEFiRet;
	wrkrInstanceData_t *pWrkrData = (wrkrInstanceData_t *) pvData;

	DBGPRINTF("omfwd: add %u bytes to send buffer (curr offs %u)\n",
		(unsigned) len, pWrkrData->offsSndBuf);
	if(pWrkrData->offsSndBuf != 0 && pWrkrData->offsSndBuf + len >= sizeof(pWrkrData->sndBuf)) {
		/* no buffer space left, need to commit previous records */
		CHKiRet(TCPSendBuf(pWrkrData, pWrkrData->sndBuf, pWrkrData->offsSndBuf, NO_FLUSH));
		pWrkrData->offsSndBuf = 0;
		iRet = RS_RET_PREVIOUS_COMMITTED;
	}

	/* check if the message is too large to fit into buffer */
	if(len > sizeof(pWrkrData->sndBuf)) {
		CHKiRet(TCPSendBuf(pWrkrData, (uchar*)msg, len, NO_FLUSH));
		ABORT_FINALIZE(RS_RET_OK);	/* committed everything so far */
	}

	/* we now know the buffer has enough free space */
	memcpy(pWrkrData->sndBuf + pWrkrData->offsSndBuf, msg, len);
	pWrkrData->offsSndBuf += len;
	iRet = RS_RET_DEFER_COMMIT;

finalize_it:
	RETiRet;
}


/* This function is called immediately before a send retry is attempted.
 * It shall clean up whatever makes sense.
 * rgerhards, 2007-12-28
 */
static rsRetVal TCPSendPrepRetry(void *pvData)
{
	DEFiRet;
	wrkrInstanceData_t *pWrkrData = (wrkrInstanceData_t *) pvData;

	assert(pWrkrData != NULL);
	DestructTCPInstanceData(pWrkrData);
	RETiRet;
}


/* initializes everything so that TCPSend can work.
 * rgerhards, 2007-12-28
 */
static rsRetVal TCPSendInit(void *pvData)
{
	DEFiRet;
	wrkrInstanceData_t *pWrkrData = (wrkrInstanceData_t *) pvData;
	instanceData *pData;

	assert(pWrkrData != NULL);
	pData = pWrkrData->pData;

	if(pWrkrData->pNetstrm == NULL) {
		dbgprintf("TCPSendInit CREATE\n");
		CHKiRet(netstrms.Construct(&pWrkrData->pNS));
		/* the stream driver must be set before the object is finalized! */
		CHKiRet(netstrms.SetDrvrName(pWrkrData->pNS, pData->pszStrmDrvr));
		CHKiRet(netstrms.ConstructFinalize(pWrkrData->pNS));

		/* now create the actual stream and connect to the server */
		CHKiRet(netstrms.CreateStrm(pWrkrData->pNS, &pWrkrData->pNetstrm));
		CHKiRet(netstrm.ConstructFinalize(pWrkrData->pNetstrm));
		CHKiRet(netstrm.SetDrvrMode(pWrkrData->pNetstrm, pData->iStrmDrvrMode));
		/* now set optional params, but only if they were actually configured */
		if(pData->pszStrmDrvrAuthMode != NULL) {
			CHKiRet(netstrm.SetDrvrAuthMode(pWrkrData->pNetstrm, pData->pszStrmDrvrAuthMode));
		}
		if(pData->pPermPeers != NULL) {
			CHKiRet(netstrm.SetDrvrPermPeers(pWrkrData->pNetstrm, pData->pPermPeers));
		}
		/* params set, now connect */
		CHKiRet(netstrm.Connect(pWrkrData->pNetstrm, glbl.GetDefPFFamily(),
			(uchar*)pData->port, (uchar*)pData->target));
	}

finalize_it:
	if(iRet != RS_RET_OK) {
		dbgprintf("TCPSendInit FAILED with %d.\n", iRet);
		DestructTCPInstanceData(pWrkrData);
	}

	RETiRet;
}


/* try to resume connection if it is not ready
 * rgerhards, 2007-08-02
 */
static rsRetVal doTryResume(wrkrInstanceData_t *pWrkrData)
{
	int iErr;
	struct addrinfo *res;
	struct addrinfo hints;
	instanceData *pData;
	DEFiRet;

	if(pWrkrData->bIsConnected)
		FINALIZE;
	pData = pWrkrData->pData;

	/* The remote address is not yet known and needs to be obtained */
	dbgprintf(" %s\n", pData->target);
	if(pData->protocol == FORW_UDP) {
		memset(&hints, 0, sizeof(hints));
		/* port must be numeric, because config file syntax requires this */
		hints.ai_flags = AI_NUMERICSERV;
		hints.ai_family = glbl.GetDefPFFamily();
		hints.ai_socktype = SOCK_DGRAM;
		if((iErr = (getaddrinfo(pData->target, pData->port, &hints, &res))) != 0) {
			dbgprintf("could not get addrinfo for hostname '%s':'%s': %d%s\n",
				  pData->target, pData->port, iErr, gai_strerror(iErr));
			ABORT_FINALIZE(RS_RET_SUSPENDED);
		}
		dbgprintf("%s found, resuming.\n", pData->target);
		pWrkrData->f_addr = res;
		pWrkrData->bIsConnected = 1;
		if(pWrkrData->pSockArray == NULL) {
			pWrkrData->pSockArray = net.create_udp_socket((uchar*)pData->target, NULL, 0, 0);
		}
	} else {
		CHKiRet(TCPSendInit((void*)pWrkrData));
	}

finalize_it:
	if(iRet != RS_RET_OK) {
		if(pWrkrData->f_addr != NULL) {
			freeaddrinfo(pWrkrData->f_addr);
			pWrkrData->f_addr = NULL;
		}
		iRet = RS_RET_SUSPENDED;
	}

	RETiRet;
}


BEGINtryResume
CODESTARTtryResume
	dbgprintf("DDDD: tryResume: pWrkrData %p\n", pWrkrData);
	iRet = doTryResume(pWrkrData);
ENDtryResume


BEGINbeginTransaction
CODESTARTbeginTransaction
dbgprintf("omfwd: beginTransaction\n");
	iRet = doTryResume(pWrkrData);
ENDbeginTransaction


static rsRetVal
processMsg(wrkrInstanceData_t *__restrict__ const pWrkrData,
	actWrkrIParams_t *__restrict__ const iparam)
{
	uchar *psz; /* temporary buffering */
	register unsigned l;
	int iMaxLine;
	Bytef *out = NULL; /* for compression */
	instanceData *__restrict__ const pData = pWrkrData->pData;
	DEFiRet;

	iMaxLine = glbl.GetMaxLine();

	psz = iparam->param;
	l = iparam->lenStr;
	if((int) l > iMaxLine)
		l = iMaxLine;

	/* Check if we should compress and, if so, do it. We also
	 * check if the message is large enough to justify compression.
	 * The smaller the message, the less likely is a gain in compression.
	 * To save CPU cycles, we do not try to compress very small messages.
	 * What "very small" means needs to be configured. Currently, it is
	 * hard-coded but this may be changed to a config parameter.
	 * rgerhards, 2006-11-30
	 */
	if(pData->compressionMode == COMPRESS_SINGLE_MSG && (l > CONF_MIN_SIZE_FOR_COMPRESS)) {
		uLongf destLen = iMaxLine + iMaxLine/100 +12; /* recommended value from zlib doc */
		uLong srcLen = l;
		int ret;
		CHKmalloc(out = (Bytef*) MALLOC(destLen));
		out[0] = 'z';
		out[1] = '\0';
		ret = compress2((Bytef*) out+1, &destLen, (Bytef*) psz,
				srcLen, pData->compressionLevel);
		dbgprintf("Compressing message, length was %d now %d, return state  %d.\n",
			l, (int) destLen, ret);
		if(ret != Z_OK) {
			/* if we fail, we complain, but only in debug mode
			 * Otherwise, we are silent. In any case, we ignore the
			 * failed compression and just sent the uncompressed
			 * data, which is still valid. So this is probably the
			 * best course of action.
			 * rgerhards, 2006-11-30
			 */
			dbgprintf("Compression failed, sending uncompressed message\n");
		} else if(destLen+1 < l) {
			/* only use compression if there is a gain in using it! */
			dbgprintf("there is gain in compression, so we do it\n");
			psz = out;
			l = destLen + 1; /* take care for the "z" at message start! */
		}
		++destLen;
	}

	if(pData->protocol == FORW_UDP) {
		/* forward via UDP */
		CHKiRet(UDPSend(pWrkrData, psz, l));
	} else {
		/* forward via TCP */
		iRet = tcpclt.Send(pWrkrData->pTCPClt, pWrkrData, (char *)psz, l);
		if(iRet != RS_RET_OK && iRet != RS_RET_DEFER_COMMIT && iRet != RS_RET_PREVIOUS_COMMITTED) {
			/* error! */
			dbgprintf("error forwarding via tcp, suspending\n");
			DestructTCPInstanceData(pWrkrData);
			iRet = RS_RET_SUSPENDED;
		}
	}
finalize_it:
	free(out); /* is NULL if it was never used... */
	RETiRet;
}

BEGINcommitTransaction
	unsigned i;
CODESTARTcommitTransaction
	CHKiRet(doTryResume(pWrkrData));

	dbgprintf(" %s:%s/%s\n", pWrkrData->pData->target, pWrkrData->pData->port,
		 pWrkrData->pData->protocol == FORW_UDP ? "udp" : "tcp");

	for(i = 0 ; i < nParams ; ++i) {
		iRet = processMsg(pWrkrData, &actParam(pParams, 1, i, 0));
		if(iRet != RS_RET_OK && iRet != RS_RET_DEFER_COMMIT && iRet != RS_RET_PREVIOUS_COMMITTED)
			FINALIZE;
	}

dbgprintf("omfwd: endTransaction, offsSndBuf %u, iRet %d\n", pWrkrData->offsSndBuf, iRet);
	if(pWrkrData->offsSndBuf != 0) {
		iRet = TCPSendBuf(pWrkrData, pWrkrData->sndBuf, pWrkrData->offsSndBuf, IS_FLUSH);
		pWrkrData->offsSndBuf = 0;
	}
finalize_it:
ENDcommitTransaction


/* This function loads TCP support, if not already loaded. It will be called
 * during config processing. To server ressources, TCP support will only
 * be loaded if it actually is used. -- rgerhard, 2008-04-17
 */
static rsRetVal
loadTCPSupport(void)
{
	DEFiRet;
	CHKiRet(objUse(netstrms, LM_NETSTRMS_FILENAME));
	CHKiRet(objUse(netstrm, LM_NETSTRMS_FILENAME));
	CHKiRet(objUse(tcpclt, LM_TCPCLT_FILENAME));

finalize_it:
	RETiRet;
}


/* initialize TCP structures (if necessary) after the instance has been
 * created.
 */
static rsRetVal
initTCP(wrkrInstanceData_t *pWrkrData)
{
	instanceData *pData;
	DEFiRet;

	pData = pWrkrData->pData;
	if(pData->protocol == FORW_TCP) {
		/* create our tcpclt */
		CHKiRet(tcpclt.Construct(&pWrkrData->pTCPClt));
		CHKiRet(tcpclt.SetResendLastOnRecon(pWrkrData->pTCPClt, pData->bResendLastOnRecon));
		/* and set callbacks */
		CHKiRet(tcpclt.SetSendInit(pWrkrData->pTCPClt, TCPSendInit));
		CHKiRet(tcpclt.SetSendFrame(pWrkrData->pTCPClt, TCPSendFrame));
		CHKiRet(tcpclt.SetSendPrepRetry(pWrkrData->pTCPClt, TCPSendPrepRetry));
		CHKiRet(tcpclt.SetFraming(pWrkrData->pTCPClt, pData->tcp_framing));
		CHKiRet(tcpclt.SetRebindInterval(pWrkrData->pTCPClt, pData->iRebindInterval));
	}
finalize_it:
	RETiRet;
}


static inline void
setInstParamDefaults(instanceData *pData)
{
	pData->tplName = NULL;
	pData->protocol = FORW_UDP;
	pData->tcp_framing = TCP_FRAMING_OCTET_STUFFING;
	pData->pszStrmDrvr = NULL;
	pData->pszStrmDrvrAuthMode = NULL;
	pData->iStrmDrvrMode = 0;
	pData->iRebindInterval = 0;
	pData->bResendLastOnRecon = 0; 
	pData->bSendToAll = -1;  /* unspecified */
	pData->iUDPSendDelay = 0;
	pData->pPermPeers = NULL;
	pData->compressionLevel = 9;
	pData->strmCompFlushOnTxEnd = 1;
	pData->compressionMode = COMPRESS_NEVER;
	pData->errsToReport = 5;
}

BEGINnewActInst
	struct cnfparamvals *pvals;
	uchar *tplToUse;
	char *cstr;
	int i;
	rsRetVal localRet;
	int complevel = -1;
CODESTARTnewActInst
	DBGPRINTF("newActInst (omfwd)\n");

	pvals = nvlstGetParams(lst, &actpblk, NULL);
	if(pvals == NULL) {
		errmsg.LogError(0, RS_RET_MISSING_CNFPARAMS, "omfwd: either the \"file\" or "
				"\"dynfile\" parameter must be given");
		ABORT_FINALIZE(RS_RET_MISSING_CNFPARAMS);
	}

	if(Debug) {
		dbgprintf("action param blk in omfwd:\n");
		cnfparamsPrint(&actpblk, pvals);
	}

	CHKiRet(createInstance(&pData));
	setInstParamDefaults(pData);

	for(i = 0 ; i < actpblk.nParams ; ++i) {
		if(!pvals[i].bUsed)
			continue;
		if(!strcmp(actpblk.descr[i].name, "target")) {
			pData->target = es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(actpblk.descr[i].name, "port")) {
			pData->port = es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(actpblk.descr[i].name, "protocol")) {
			if(!es_strcasebufcmp(pvals[i].val.d.estr, (uchar*)"udp", 3)) {
				pData->protocol = FORW_UDP;
			} else if(!es_strcasebufcmp(pvals[i].val.d.estr, (uchar*)"tcp", 3)) {
				localRet = loadTCPSupport();
				if(localRet != RS_RET_OK) {
					errmsg.LogError(0, localRet, "could not activate network stream modules for TCP "
							"(internal error %d) - are modules missing?", localRet);
					ABORT_FINALIZE(localRet);
				}
				pData->protocol = FORW_TCP;
			} else {
				uchar *str;
				str = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
				errmsg.LogError(0, RS_RET_INVLD_PROTOCOL,
						"omfwd: invalid protocol \"%s\"", str);
				free(str);
				ABORT_FINALIZE(RS_RET_INVLD_PROTOCOL);
			}
		} else if(!strcmp(actpblk.descr[i].name, "tcp_framing")) {
			if(!es_strcasebufcmp(pvals[i].val.d.estr, (uchar*)"traditional", 11)) {
				pData->tcp_framing = TCP_FRAMING_OCTET_STUFFING;
			} else if(!es_strcasebufcmp(pvals[i].val.d.estr, (uchar*)"octet-counted", 13)) {
				pData->tcp_framing = TCP_FRAMING_OCTET_COUNTING;
			} else {
				uchar *str;
				str = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
				errmsg.LogError(0, RS_RET_CNF_INVLD_FRAMING,
						"omfwd: invalid framing \"%s\"", str);
				free(str);
				ABORT_FINALIZE(RS_RET_CNF_INVLD_FRAMING );
			}
		} else if(!strcmp(actpblk.descr[i].name, "rebindinterval")) {
			pData->iRebindInterval = (int) pvals[i].val.d.n;
		} else if(!strcmp(actpblk.descr[i].name, "streamdriver")) {
			pData->pszStrmDrvr = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(actpblk.descr[i].name, "streamdrivermode")) {
			pData->iStrmDrvrMode = pvals[i].val.d.n;
		} else if(!strcmp(actpblk.descr[i].name, "streamdriverauthmode")) {
			pData->pszStrmDrvrAuthMode = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(actpblk.descr[i].name, "streamdriverpermittedpeers")) {
			uchar *start, *str;
			uchar save;
			uchar *p;
			int lenStr;
			str = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
			start = str;
			lenStr = ustrlen(start); /* we need length after '\0' has been dropped... */
			while(lenStr > 0) {
				p = start;
				while(*p && *p != ',' && lenStr--)
					p++;
				if(*p == ',') {
					*p = '\0';
				}
				save = *(p+1); /* we always have this, at least the \0 byte at EOS */
				*(p+1) = '\0';
				if(*start == '\0') {
					DBGPRINTF("omfwd: ignoring empty permitted peer\n");
				} else {
					dbgprintf("omfwd: adding permitted peer: '%s'\n", start);
					CHKiRet(net.AddPermittedPeer(&(pData->pPermPeers), start));
				}
				start = p+1;
				if(lenStr)
					--lenStr;
				*(p+1) = save;
			}
			free(str);
		} else if(!strcmp(actpblk.descr[i].name, "ziplevel")) {
			complevel = pvals[i].val.d.n;
			if(complevel >= 0 && complevel <= 10) {
				pData->compressionLevel = complevel;
				pData->compressionMode = COMPRESS_SINGLE_MSG;
			} else {
				errmsg.LogError(0, NO_ERRCODE, "Invalid ziplevel %d specified in "
					 "forwardig action - NOT turning on compression.",
					 complevel);
			}
		} else if(!strcmp(actpblk.descr[i].name, "maxerrormessages")) {
			pData->errsToReport = (int) pvals[i].val.d.n;
		} else if(!strcmp(actpblk.descr[i].name, "resendlastmsgonreconnect")) {
			pData->bResendLastOnRecon = (int) pvals[i].val.d.n;
		} else if(!strcmp(actpblk.descr[i].name, "udp.sendtoall")) {
			pData->bSendToAll = (int) pvals[i].val.d.n;
		} else if(!strcmp(actpblk.descr[i].name, "udp.senddelay")) {
			pData->iUDPSendDelay = (int) pvals[i].val.d.n;
		} else if(!strcmp(actpblk.descr[i].name, "template")) {
			pData->tplName = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(actpblk.descr[i].name, "compression.stream.flushontxend")) {
			pData->strmCompFlushOnTxEnd = (sbool) pvals[i].val.d.n;
		} else if(!strcmp(actpblk.descr[i].name, "compression.mode")) {
			cstr = es_str2cstr(pvals[i].val.d.estr, NULL);
			if(!strcasecmp(cstr, "stream:always")) {
				pData->compressionMode = COMPRESS_STREAM_ALWAYS;
			} else if(!strcasecmp(cstr, "none")) {
				pData->compressionMode = COMPRESS_NEVER;
			} else if(!strcasecmp(cstr, "single")) {
				pData->compressionMode = COMPRESS_SINGLE_MSG;
			} else {
				errmsg.LogError(0, RS_RET_PARAM_ERROR, "omfwd: invalid value for 'compression.mode' "
					 "parameter (given is '%s')", cstr);
				free(cstr);
				ABORT_FINALIZE(RS_RET_PARAM_ERROR);
			}
			free(cstr);
		} else {
			DBGPRINTF("omfwd: program error, non-handled "
			  "param '%s'\n", actpblk.descr[i].name);
		}
	}

	if(complevel != -1) {
		pData->compressionLevel = complevel;
		if(pData->compressionMode == COMPRESS_NEVER) {
			/* to keep compatible with pre-7.3.11, only setting the
			 * compresion level means old-style single-message mode.
			 */
			pData->compressionMode = COMPRESS_SINGLE_MSG;
		}
	}

	CODE_STD_STRING_REQUESTnewActInst(1)

	tplToUse = ustrdup((pData->tplName == NULL) ? getDfltTpl() : pData->tplName);
	CHKiRet(OMSRsetEntry(*ppOMSR, 0, tplToUse, OMSR_NO_RQD_TPL_OPTS));

	if(pData->bSendToAll == -1) {
		pData->bSendToAll = send_to_all;
	} else {
		if(pData->protocol == FORW_TCP) {
			errmsg.LogError(0, RS_RET_PARAM_ERROR, "omfwd: paramter udp.sendToAll "
					"cannot be used with tcp transport -- ignored");
		}
	}
CODE_STD_FINALIZERnewActInst
	cnfparamvalsDestruct(pvals, &actpblk);
ENDnewActInst


BEGINparseSelectorAct
	uchar *q;
	int i;
	rsRetVal localRet;
        struct addrinfo;
	TCPFRAMINGMODE tcp_framing = TCP_FRAMING_OCTET_STUFFING;
CODESTARTparseSelectorAct
CODE_STD_STRING_REQUESTparseSelectorAct(1)
	if(*p != '@')
		ABORT_FINALIZE(RS_RET_CONFLINE_UNPROCESSED);

	CHKiRet(createInstance(&pData));

	++p; /* eat '@' */
	if(*p == '@') { /* indicator for TCP! */
		localRet = loadTCPSupport();
		if(localRet != RS_RET_OK) {
			errmsg.LogError(0, localRet, "could not activate network stream modules for TCP "
					"(internal error %d) - are modules missing?", localRet);
			ABORT_FINALIZE(localRet);
		}
		pData->protocol = FORW_TCP;
		++p; /* eat this '@', too */
	} else {
		pData->protocol = FORW_UDP;
	}
	/* we are now after the protocol indicator. Now check if we should
	 * use compression. We begin to use a new option format for this:
	 * @(option,option)host:port
	 * The first option defined is "z[0..9]" where the digit indicates
	 * the compression level. If it is not given, 9 (best compression) is
	 * assumed. An example action statement might be:
	 * @@(z5,o)127.0.0.1:1400  
	 * Which means send via TCP with medium (5) compresion (z) to the local
	 * host on port 1400. The '0' option means that octet-couting (as in
	 * IETF I-D syslog-transport-tls) is to be used for framing (this option
	 * applies to TCP-based syslog only and is ignored when specified with UDP).
	 * That is not yet implemented.
	 * rgerhards, 2006-12-07
	 * In order to support IPv6 addresses, we must introduce an extension to
	 * the hostname. If it is in square brackets, whatever is in them is treated as
	 * the hostname - without any exceptions ;) -- rgerhards, 2008-08-05
	 */
	if(*p == '(') {
		/* at this position, it *must* be an option indicator */
		do {
			++p; /* eat '(' or ',' (depending on when called) */
			/* check options */
			if(*p == 'z') { /* compression */
				++p; /* eat */
				if(isdigit((int) *p)) {
					int iLevel;
					iLevel = *p - '0';
					++p; /* eat */
					pData->compressionLevel = iLevel;
					pData->compressionMode = COMPRESS_SINGLE_MSG;
				} else {
					errmsg.LogError(0, NO_ERRCODE, "Invalid compression level '%c' specified in "
						 "forwardig action - NOT turning on compression.",
						 *p);
				}
			} else if(*p == 'o') { /* octet-couting based TCP framing? */
				++p; /* eat */
				/* no further options settable */
				tcp_framing = TCP_FRAMING_OCTET_COUNTING;
			} else { /* invalid option! Just skip it... */
				errmsg.LogError(0, NO_ERRCODE, "Invalid option %c in forwarding action - ignoring.", *p);
				++p; /* eat invalid option */
			}
			/* the option processing is done. We now do a generic skip
			 * to either the next option or the end of the option
			 * block.
			 */
			while(*p && *p != ')' && *p != ',')
				++p;	/* just skip it */
		} while(*p && *p == ','); /* Attention: do.. while() */
		if(*p == ')')
			++p; /* eat terminator, on to next */
		else
			/* we probably have end of string - leave it for the rest
			 * of the code to handle it (but warn the user)
			 */
			errmsg.LogError(0, NO_ERRCODE, "Option block not terminated in forwarding action.");
	}

	/* extract the host first (we do a trick - we replace the ';' or ':' with a '\0')
	 * now skip to port and then template name. rgerhards 2005-07-06
	 */
	if(*p == '[') { /* everything is hostname upto ']' */
		++p; /* skip '[' */
		for(q = p ; *p && *p != ']' ; ++p)
			/* JUST SKIP */;
		if(*p == ']') {
			*p = '\0'; /* trick to obtain hostname (later)! */
			++p; /* eat it */
		}
	} else { /* traditional view of hostname */
		for(q = p ; *p && *p != ';' && *p != ':' && *p != '#' ; ++p)
			/* JUST SKIP */;
	}

	pData->tcp_framing = tcp_framing;
	pData->port = NULL;
	if(*p == ':') { /* process port */
		uchar * tmp;

		*p = '\0'; /* trick to obtain hostname (later)! */
		tmp = ++p;
		for(i=0 ; *p && isdigit((int) *p) ; ++p, ++i)
			/* SKIP AND COUNT */;
		pData->port = MALLOC(i + 1);
		if(pData->port == NULL) {
			errmsg.LogError(0, NO_ERRCODE, "Could not get memory to store syslog forwarding port, "
				 "using default port, results may not be what you intend\n");
			/* we leave f_forw.port set to NULL, this is then handled below */
		} else {
			memcpy(pData->port, tmp, i);
			*(pData->port + i) = '\0';
		}
	}
	/* check if no port is set. If so, we use the IANA-assigned port of 514 */
	if(pData->port == NULL) {
		CHKmalloc(pData->port = strdup("514"));
	}
	
	/* now skip to template */
	while(*p && *p != ';'  && *p != '#' && !isspace((int) *p))
		++p; /*JUST SKIP*/

	if(*p == ';' || *p == '#' || isspace(*p)) {
		uchar cTmp = *p;
		*p = '\0'; /* trick to obtain hostname (later)! */
		CHKmalloc(pData->target = strdup((char*) q));
		*p = cTmp;
	} else {
		CHKmalloc(pData->target = strdup((char*) q));
	}

	/* copy over config data as needed */
	pData->iRebindInterval = (pData->protocol == FORW_TCP) ?
				 cs.iTCPRebindInterval : cs.iUDPRebindInterval;

	/* process template */
	CHKiRet(cflineParseTemplateName(&p, *ppOMSR, 0, OMSR_NO_RQD_TPL_OPTS, getDfltTpl()));

	if(pData->protocol == FORW_TCP) {
		pData->bResendLastOnRecon = cs.bResendLastOnRecon;
		pData->iStrmDrvrMode = cs.iStrmDrvrMode;
		if(cs.pszStrmDrvr != NULL)
			CHKmalloc(pData->pszStrmDrvr = (uchar*)strdup((char*)cs.pszStrmDrvr));
		if(cs.pszStrmDrvrAuthMode != NULL)
			CHKmalloc(pData->pszStrmDrvrAuthMode =
				     (uchar*)strdup((char*)cs.pszStrmDrvrAuthMode));
		if(cs.pPermPeers != NULL) {
			pData->pPermPeers = cs.pPermPeers;
			cs.pPermPeers = NULL;
		}
	}
CODE_STD_FINALIZERparseSelectorAct
ENDparseSelectorAct


/* a common function to free our configuration variables - used both on exit
 * and on $ResetConfig processing. -- rgerhards, 2008-05-16
 */
static void
freeConfigVars(void)
{
	free(cs.pszStrmDrvr);
	cs.pszStrmDrvr = NULL;
	free(cs.pszStrmDrvrAuthMode);
	cs.pszStrmDrvrAuthMode = NULL;
	free(cs.pPermPeers);
	cs.pPermPeers = NULL; /* TODO: fix in older builds! */
}


BEGINmodExit
CODESTARTmodExit
	/* release what we no longer need */
	objRelease(errmsg, CORE_COMPONENT);
	objRelease(glbl, CORE_COMPONENT);
	objRelease(net, LM_NET_FILENAME);
	objRelease(netstrm, LM_NETSTRMS_FILENAME);
	objRelease(netstrms, LM_NETSTRMS_FILENAME);
	objRelease(tcpclt, LM_TCPCLT_FILENAME);
	freeConfigVars();
ENDmodExit


BEGINqueryEtryPt
CODESTARTqueryEtryPt
CODEqueryEtryPt_STD_OMODTX_QUERIES
CODEqueryEtryPt_STD_OMOD8_QUERIES
CODEqueryEtryPt_STD_CONF2_QUERIES
CODEqueryEtryPt_STD_CONF2_setModCnf_QUERIES
CODEqueryEtryPt_STD_CONF2_OMOD_QUERIES
ENDqueryEtryPt


/* Reset config variables for this module to default values.
 * rgerhards, 2008-03-28
 */
static rsRetVal resetConfigVariables(uchar __attribute__((unused)) *pp, void __attribute__((unused)) *pVal)
{
	freeConfigVars();

	/* we now must reset all non-string values */
	cs.iStrmDrvrMode = 0;
	cs.bResendLastOnRecon = 0;
	cs.iUDPRebindInterval = 0;
	cs.iTCPRebindInterval = 0;

	return RS_RET_OK;
}


BEGINmodInit(Fwd)
CODESTARTmodInit
INITLegCnfVars
	*ipIFVersProvided = CURR_MOD_IF_VERSION; /* we only support the current interface specification */
CODEmodInit_QueryRegCFSLineHdlr
	CHKiRet(objUse(glbl, CORE_COMPONENT));
	CHKiRet(objUse(errmsg, CORE_COMPONENT));
	CHKiRet(objUse(net,LM_NET_FILENAME));

	CHKiRet(regCfSysLineHdlr((uchar *)"actionforwarddefaulttemplate", 0, eCmdHdlrGetWord, setLegacyDfltTpl, NULL, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"actionsendtcprebindinterval", 0, eCmdHdlrInt, NULL, &cs.iTCPRebindInterval, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"actionsendudprebindinterval", 0, eCmdHdlrInt, NULL, &cs.iUDPRebindInterval, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"actionsendstreamdriver", 0, eCmdHdlrGetWord, NULL, &cs.pszStrmDrvr, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"actionsendstreamdrivermode", 0, eCmdHdlrInt, NULL, &cs.iStrmDrvrMode, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"actionsendstreamdriverauthmode", 0, eCmdHdlrGetWord, NULL, &cs.pszStrmDrvrAuthMode, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"actionsendstreamdriverpermittedpeer", 0, eCmdHdlrGetWord, setPermittedPeer, NULL, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"actionsendresendlastmsgonreconnect", 0, eCmdHdlrBinary, NULL, &cs.bResendLastOnRecon, NULL));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"resetconfigvariables", 1, eCmdHdlrCustomHandler, resetConfigVariables, NULL, STD_LOADABLE_MODULE_ID));
ENDmodInit

/* vim:set ai:
 */
