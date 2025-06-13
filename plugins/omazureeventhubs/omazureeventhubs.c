/* omazureeventhubs.c
 * This output plugin make rsyslog talk to Azure EventHubs.
 *
 * Copyright 2014-2017 by Adiscon GmbH.
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
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/uio.h>
#include <sys/queue.h>
#include <sys/types.h>
#include <math.h>
#ifdef HAVE_SYS_STAT_H
#	include <sys/stat.h>
#endif
#include <sys/time.h>
#include <time.h>

// Include Proton headers
#include <proton/version.h>
#include <proton/condition.h>
#include <proton/connection.h>
#include <proton/delivery.h>
#include <proton/link.h>
#include <proton/listener.h>
#include <proton/netaddr.h>
#include <proton/message.h>
#include <proton/object.h>
#include <proton/proactor.h>
#include <proton/sasl.h>
#include <proton/session.h>
#include <proton/transport.h>
#include <proton/ssl.h>

// Include rsyslog headers
#include "rsyslog.h"
#include "syslogd-types.h"
#include "srUtils.h"
#include "template.h"
#include "module-template.h"
#include "errmsg.h"
#include "atomic.h"
#include "statsobj.h"
#include "unicode-helper.h"
#include "datetime.h"
#include "glbl.h"

MODULE_TYPE_OUTPUT
MODULE_TYPE_NOKEEP
MODULE_CNFNAME("omazureeventhubs")

/* internal structures
 */
DEF_OMOD_STATIC_DATA
DEFobjCurrIf(glbl)
DEFobjCurrIf(datetime)
DEFobjCurrIf(strm)
DEFobjCurrIf(statsobj)

statsobj_t *azureStats;
STATSCOUNTER_DEF(ctrMessageSubmit, mutCtrMessageSubmit);
STATSCOUNTER_DEF(ctrAzureFail, mutCtrAzureFail);
STATSCOUNTER_DEF(ctrCacheMiss, mutCtrCacheMiss);
STATSCOUNTER_DEF(ctrCacheEvict, mutCtrCacheEvict);
STATSCOUNTER_DEF(ctrCacheSkip, mutCtrCacheSkip);
STATSCOUNTER_DEF(ctrAzureAck, mutCtrAzureAck);
STATSCOUNTER_DEF(ctrAzureMsgTooLarge, mutCtrAzureMsgTooLarge);
STATSCOUNTER_DEF(ctrAzureQueueFull, mutCtrAzureQueueFull);
STATSCOUNTER_DEF(ctrAzureOtherErrors, mutCtrAzureOtherErrors);
STATSCOUNTER_DEF(ctrAzureRespTimedOut, mutCtrAzureRespTimedOut);
STATSCOUNTER_DEF(ctrAzureRespTransport, mutCtrAzureRespTransport);
STATSCOUNTER_DEF(ctrAzureRespBrokerDown, mutCtrAzureRespBrokerDown);
STATSCOUNTER_DEF(ctrAzureRespAuth, mutCtrAzureRespAuth);
STATSCOUNTER_DEF(ctrAzureRespSSL, mutCtrAzureRespSSL);
STATSCOUNTER_DEF(ctrAzureRespOther, mutCtrAzureRespOther);

#define MAX_ERRMSG 1024		/* max size of error messages that we support */
#define MAX_DEFAULTMSGS 1024	/* Initial max size of the proton message helper array */

#define SETUP_PROTON_NONE 0
#define SETUP_PROTON_AUTOCLOSE 1

/* flags for writeAzure: shall we resubmit a failed message? */
#define RESUBMIT	1
#define NO_RESUBMIT	0

/* flags for transaction Handling */
enum proton_submission_status
{
	PROTON_UNSUBMITTED = 0,	// Message not submitted yet
	PROTON_SUBMITTED,	// Message submitted to proton sender instance
	PROTON_ACCEPTED,	// Message accepted from remote target
	PROTON_REJECTED,	// Message rejected from remote target (zero credit?)
};

// event_property NEEDED?
struct event_property {
	const char *key;
	const char *val;
};

static pn_timestamp_t time_now(void);

/* Struct for Proton Messages Listitems */
struct s_protonmsg_entry {
	uchar* payload;
	size_t payload_len;
	uchar* MsgID;
	size_t MsgID_len;

	uchar* address;
	char status;
};
typedef struct s_protonmsg_entry protonmsg_entry;

/* Struct for module InstanceData */
typedef struct _instanceData {
	uchar *amqp_address;
	uchar *azurehost;
	uchar *azureport;
	uchar *azure_key_name;
	uchar *azure_key;
	uchar *container;
	uchar *tplName;			/* assigned output template */

	int nEventProperties;
	struct event_property *eventProperties;

	uchar *statsName;
	statsobj_t *stats;
	STATSCOUNTER_DEF(ctrMessageSubmit, mutCtrMessageSubmit);
	STATSCOUNTER_DEF(ctrAzureFail, mutCtrAzureFail);
	STATSCOUNTER_DEF(ctrAzureAck, mutCtrAzureAck);
	STATSCOUNTER_DEF(ctrAzureOtherErrors, mutCtrAzureOtherErrors);
} instanceData;

/* Struct for module workerInstanceData */
typedef struct wrkrInstanceData {
	instanceData *pData;

	protonmsg_entry **aProtonMsgs;	/* dynamically sized array for transactional outputs */
	unsigned int nProtonMsgs;	/* current used proton msgs */
	unsigned int nMaxProtonMsgs;	/* current max */

	int bIsConnecting;		/* 1 if connecting, 0 if disconnected */
	int bIsConnected;		/* 1 if connected, 0 if disconnected */
	int bIsSuspended;		/* when broker fail, we need to suspend the action */
	pthread_rwlock_t pnLock;

	// PROTON Handles
	pn_proactor_t *pnProactor;
	pn_transport_t *pnTransport;
	pn_connection_t *pnConn;
	pn_link_t* pnSender;
	pn_rwbytes_t pnMessageBuffer;	/* Buffer for messages */
	int pnStatus;

	// Message Counters for sender link in worker instance
	unsigned int iMsgSeq;
	unsigned int iMaxMsgSeq;

	/* The following structure controls the proton handling threads. Passes necessary pointers
	* needed for their access.
	*/
	sbool bThreadRunning;
	pthread_t tid;		/* the worker's thread ID */

} wrkrInstanceData_t;

#define INST_STATSCOUNTER_INC(inst, ctr, mut) \
	do { \
		if (inst->stats) { STATSCOUNTER_INC(ctr, mut); } \
	} while(0);

// QPID Proton Handler functions
static rsRetVal proton_run_thread(wrkrInstanceData_t *pWrkrData);
static rsRetVal proton_shutdown_thread(wrkrInstanceData_t *pWrkrData);
static void * proton_thread(void *myInfo);
static void handleProtonDelivery(wrkrInstanceData_t *const pWrkrData);
static void handleProton(wrkrInstanceData_t *const pWrkrData, pn_event_t *event);
static rsRetVal writeProton(wrkrInstanceData_t *__restrict__ const pWrkrData,
	  const actWrkrIParams_t *__restrict__ const pParam,
	  const int iMsg);

/* tables for interfacing with the v6 config system */
/* action (instance) parameters */
static struct cnfparamdescr actpdescr[] = {
	{ "azurehost", eCmdHdlrString, 0 },
	{ "azureport", eCmdHdlrString, 0 },
	{ "azure_key_name", eCmdHdlrString, 0 },
	{ "azure_key", eCmdHdlrString, 0 },
	{ "amqp_address", eCmdHdlrString, 0 },
	{ "container", eCmdHdlrString, 0 },
	{ "eventproperties", eCmdHdlrArray, 0 },
	{ "template", eCmdHdlrGetWord, 0 },
	{ "statsname", eCmdHdlrGetWord, 0 }
};
static struct cnfparamblk actpblk =
	{ CNFPARAMBLK_VERSION,
	  sizeof(actpdescr)/sizeof(struct cnfparamdescr),
	  actpdescr
	};

BEGINinitConfVars		/* (re)set config variables to default values */
CODESTARTinitConfVars
ENDinitConfVars

/* Helper function to URL encode a string */
static char* url_encode(const char *str) {
	if(str == NULL) return NULL;

	char *encoded = malloc(strlen(str) * 3 + 1); // Worst case: each char needs %XX
	if(encoded == NULL) return NULL;

char *p = encoded;
/* each char may expand to %XX; convert manually to avoid overruns */
static const char hex[] = "0123456789ABCDEF";
	while(*str) {
	 if((*str >= 'a' && *str <= 'z') ||
	    (*str >= 'A' && *str <= 'Z') ||
	    (*str >= '0' && *str <= '9') ||
	    *str == '-' || *str == '_' || *str == '.' || *str == '~') {
	         *p++ = *str;
	 } else {
	         unsigned char c = (unsigned char)*str;
	         *p++ = '%';
	         *p++ = hex[c >> 4];
	         *p++ = hex[c & 0xF];
	 }
	 str++;
	}
	*p = '\0';
	return encoded;
}

static void ATTR_NONNULL(1)
protonmsg_entry_destruct(protonmsg_entry *const __restrict__ fmsgEntry) {
	free(fmsgEntry->MsgID);
	free(fmsgEntry->payload);
	free(fmsgEntry->address);
	free(fmsgEntry);
}

/* note: we need the length of message as we need to deal with
 * non-NUL terminated strings under some circumstances.
 */
static protonmsg_entry * ATTR_NONNULL(1,3)
protonmsg_entry_construct(	const char *const MsgID, const size_t msgidlen,
				const char *const msg, const size_t msglen,
				const char *const address)
{
	protonmsg_entry *etry = NULL;

	if((etry = malloc(sizeof(struct s_protonmsg_entry))) == NULL) {
		return NULL;
	}
	etry->status = PROTON_UNSUBMITTED; // Unsubmitted default */

	etry->MsgID_len = msgidlen;
	if((etry->MsgID = (uchar*)malloc(msgidlen+1)) == NULL) {
		free(etry);
		return NULL;
	}
	memcpy(etry->MsgID, MsgID, msgidlen);
	etry->MsgID[msgidlen] = '\0';

	etry->payload_len = msglen;
	if((etry->payload = (uchar*)malloc(msglen+1)) == NULL) {
		free(etry->MsgID);
		free(etry);
		return NULL;
	}
	memcpy(etry->payload, msg, msglen);
	etry->payload[msglen] = '\0';

	if((etry->address = (uchar*)strdup(address)) == NULL) {
		free(etry->MsgID);
		free(etry->payload);
		free(etry);
		return NULL;
	}
	return etry;
}

/* Create a message with a map { "sequence" : number } encode it and return the encoded buffer. */
static pn_message_t* proton_encode_message(wrkrInstanceData_t *const pWrkrData, protonmsg_entry* pMsgEntry) {
	instanceData *const pData = (instanceData *const) pWrkrData->pData;
	/* Construct a message with the map */
	pn_message_t* message = pn_message();
	// Optionally include Address?
	// pn_message_set_address(message, (char *) pWrkrData->amqp_address);

	// Send in BINARY MODE ( as Stream )
	pn_message_set_content_type(message, (char*) "application/octet-stream");
	pn_message_set_creation_time(message, time_now());
	pn_message_set_inferred(message, true);
	// Set message ID
	pn_message_set_id(message, (pn_atom_t){
					.type=PN_STRING,
					.u.as_bytes.start = (char*)pMsgEntry->MsgID,
					.u.as_bytes.size = pMsgEntry->MsgID_len
					});

	if (pData->nEventProperties > 0) {
		// Add Event properties
		pn_data_t *props = pn_message_properties(message);
		pn_data_put_map(props);
		pn_data_enter(props);

		for(int i = 0 ; i < pData->nEventProperties ; ++i) {
			DBGPRINTF("proton_encode_message: add eventproperty %s:%s\n",
				pData->eventProperties[i].key,
				pData->eventProperties[i].val);
			pn_data_put_string(props, pn_bytes(strlen(pData->eventProperties[i].key),
				pData->eventProperties[i].key));
			pn_data_put_string(props, pn_bytes(strlen(pData->eventProperties[i].val),
				pData->eventProperties[i].val));
		}
		pn_data_exit(props);
	}

	// Set message BODY
	pn_data_t* body = pn_message_body(message);
	pn_data_enter(body);
	pn_data_put_binary(body, pn_bytes(pMsgEntry->payload_len, (char*)pMsgEntry->payload));
	pn_data_exit(body);

	DBGPRINTF("proton_encode_message: created message id '%s': '%.*s'\n",
		(char*)pMsgEntry->MsgID,
		(pMsgEntry->payload_len > 0 ? (int)pMsgEntry->payload_len-1 : 0),
		(char*)pMsgEntry->payload);
	
	return message;
}

static rsRetVal
closeProton(wrkrInstanceData_t *const __restrict__ pWrkrData)
{
	DEFiRet;
	instanceData *const pData = (instanceData *const) pWrkrData->pData;
#ifndef NDEBUG
	DBGPRINTF("closeProton[%p]: ENTER\n", pWrkrData);
#endif
	if (pWrkrData->pnSender) {
		pn_link_close(pWrkrData->pnSender);
		DBGPRINTF("closeProton[%p]: pn_link_close\n", pWrkrData);
		pn_session_close(pn_link_session(pWrkrData->pnSender));
		DBGPRINTF("closeProton[%p]: pn_session_close\n", pWrkrData);
	}
	if (pWrkrData->pnConn) {
		DBGPRINTF("closeProton[%p]: pn_connection_close connection\n", pWrkrData);
		pn_connection_close(pWrkrData->pnConn);
	}

	pWrkrData->bIsConnecting = 0;
	pWrkrData->bIsConnected = 0;
	pWrkrData->pnStatus = PN_EVENT_NONE;

	pWrkrData->pnSender = NULL;
	pWrkrData->pnConn = NULL;
	pWrkrData->iMsgSeq = 0;
	pWrkrData->iMaxMsgSeq = 0;

	// Mark all remaining entries as REJECTED
	if(pWrkrData->aProtonMsgs != NULL) {
		for(unsigned int i = 0 ; i < pWrkrData->nProtonMsgs ; ++i) {
			if (pWrkrData->aProtonMsgs[i] != NULL && (
					pWrkrData->aProtonMsgs[i]->status == PROTON_UNSUBMITTED ||
					pWrkrData->aProtonMsgs[i]->status == PROTON_SUBMITTED)
				) {
				pWrkrData->aProtonMsgs[i]->status = PROTON_REJECTED;
				DBGPRINTF("closeProton[%p]: Setting ProtonMsg %s to PROTON_REJECTED \n",
					pWrkrData, pWrkrData->aProtonMsgs[i]->MsgID);
				// Increment Stats Counter
				STATSCOUNTER_INC(ctrAzureFail, mutCtrAzureFail);
				INST_STATSCOUNTER_INC(pData, pData->ctrAzureFail, pData->mutCtrAzureFail);
			}
		}
	}

	FINALIZE;
finalize_it:
	RETiRet;

}

static rsRetVal
openProton(wrkrInstanceData_t *const __restrict__ pWrkrData)
{
	DEFiRet;
	instanceData *const pData = (instanceData *const) pWrkrData->pData;
	int pnErr = PN_OK;
	char szAddr[PN_MAX_ADDR];
	pn_ssl_t* pnSsl;
#ifndef NDEBUG
	DBGPRINTF("openProton[%p]: ENTER\n", pWrkrData);
#endif
	if(pWrkrData->bIsConnecting == 1 || pWrkrData->bIsConnected == 1)
		FINALIZE;
	pWrkrData->pnStatus = PN_EVENT_NONE;

	pn_proactor_addr(szAddr, sizeof(szAddr),
		(const char *) pData->azurehost,
		(const char *) pData->azureport);

	// Configure a transport for SSL. The transport will be freed by the proactor.
	pWrkrData->pnTransport = pn_transport();
	DBGPRINTF("openProton[%p]: create transport to '%s:%s'\n",
		pWrkrData, pData->azurehost, pData->azureport);
	pnSsl = pn_ssl(pWrkrData->pnTransport);
	if (pnSsl != NULL) {
		pn_ssl_domain_t* pnDomain = pn_ssl_domain(PN_SSL_MODE_CLIENT);
		if (pData->azure_key_name != NULL && pData->azure_key != NULL) {
			pnErr =  pn_ssl_init(pnSsl, pnDomain, NULL);
			if (pnErr) {
				DBGPRINTF("openProton[%p]: pn_ssl_init failed for '%s:%s' with error %d: %s\n",
					pWrkrData, pData->azurehost, pData->azureport,
					pnErr, pn_code(pnErr));
			}
			pn_sasl_allowed_mechs(pn_sasl(pWrkrData->pnTransport), "PLAIN");
		} else {
			pnErr = pn_ssl_domain_set_peer_authentication(pnDomain, PN_SSL_ANONYMOUS_PEER, NULL);
			if (!pnErr) {
				pnErr = pn_ssl_init(pnSsl, pnDomain, NULL);
			} else {
				DBGPRINTF(
				"openProton[%p]: pn_ssl_domain_set_peer_authentication failed with '%d'\n",
					pWrkrData, pnErr);
			}
		}
		pn_ssl_domain_free(pnDomain);
	} else {
		LogError(0, RS_RET_ERR, "openProton[%p]: openProton pn_ssl_init NULL", pWrkrData);
	}
	
	// Handle ERROR Output
	if (pnErr) {
		LogError(0, RS_RET_IO_ERROR, "openProton[%p]: creating transport to '%s:%s' "
			"failed with error %d: %s\n",
			pWrkrData, pData->azurehost, pData->azureport,
			pnErr, pn_code(pnErr));
		ABORT_FINALIZE(RS_RET_IO_ERROR);
	}

	// Connect to Azure Event Hubs
	pn_proactor_connect2(pWrkrData->pnProactor, NULL, pWrkrData->pnTransport, szAddr);

	// Successfully connecting
	pWrkrData->bIsConnecting = 1;
	pWrkrData->bIsSuspended = 0;
finalize_it:
	if(iRet != RS_RET_OK) {
		closeProton(pWrkrData); // Make sure to free ressources
	}
	RETiRet;
}

static sbool
proton_check_condition(	pn_event_t *event,
			wrkrInstanceData_t *const __restrict__ pWrkrData,
			pn_condition_t *cond,
			const char * pszReason) {
	if (pn_condition_is_set(cond)) {
		DBGPRINTF("proton_check_condition: %s %s: %s: %s",
			pszReason,
			pn_event_type_name(pn_event_type(event)),
			pn_condition_get_name(cond),
			pn_condition_get_description(cond));
		LogError(0, RS_RET_ERR, "omazureeventhubs: %s %s: %s: %s",
			pszReason,
			pn_event_type_name(pn_event_type(event)),
			pn_condition_get_name(cond),
			pn_condition_get_description(cond));

		// Connection can be closed
		closeProton(pWrkrData);

		// Set Worker to suspended state!
		pWrkrData->bIsSuspended = 1;

		return 0;
	} else {
		return 1;
	}
}

static rsRetVal
setupProtonHandle(wrkrInstanceData_t *const __restrict__ pWrkrData, int autoclose)
{
	DEFiRet;
	DBGPRINTF("omazureeventhubs[%p]: setupProtonHandle ENTER\n", pWrkrData);

	pthread_rwlock_wrlock(&pWrkrData->pnLock);
	if (autoclose == SETUP_PROTON_AUTOCLOSE && (pWrkrData->bIsConnected == 1)) {
		DBGPRINTF("omazureeventhubs[%p]: setupProtonHandle closeProton\n", pWrkrData);
		closeProton(pWrkrData);
	}
	CHKiRet(openProton(pWrkrData));
finalize_it:
	if (iRet != RS_RET_OK) {
		/* Parameter Error's cannot be resumed, so we need to disable the action */
		if (iRet == RS_RET_PARAM_ERROR) {
			iRet = RS_RET_DISABLE_ACTION;
			LogError(0, iRet, "omazureeventhubs: action will be disabled due invalid "
				"configuration parameters\n");
		}
	}
	pthread_rwlock_unlock(&pWrkrData->pnLock);
	RETiRet;
}

static rsRetVal
writeProton(wrkrInstanceData_t *__restrict__ const pWrkrData,
	  const actWrkrIParams_t *__restrict__ const pParam,
	  const int iMsg)
{
	DEFiRet;
	instanceData *const pData = (instanceData *const) pWrkrData->pData;
	protonmsg_entry* fmsgEntry;

	// Create Unqiue Message ID
	char szMsgID[64];
	sprintf(szMsgID, "%d", pWrkrData->iMsgSeq);

	const char* pszParamStr = (const char*)actParam(pParam, 1 /*pData->iNumTpls*/, iMsg, 0).param;
	size_t tzParamStrLen = actParam(pParam, 1 /*pData->iNumTpls*/, iMsg, 0).lenStr;

	DBGPRINTF("omazureeventhubs[%p]: writeProton for msg %d (seq %d) msg:'%.*s%s'\n",
		pWrkrData,
		iMsg, pWrkrData->iMsgSeq,
		(int)(tzParamStrLen > 0 ? (tzParamStrLen > 64 ? 64 : tzParamStrLen-1) : 0),
		pszParamStr,
		(tzParamStrLen > 64 ? "..." : ""));
	// Increment Message sequence number
	pWrkrData->iMsgSeq++;

	// Add message to LIST for sending
	CHKmalloc(fmsgEntry = protonmsg_entry_construct(
		szMsgID, sizeof(szMsgID),
		pszParamStr,
		tzParamStrLen,
		(const char*)pData->amqp_address));
	// Add to helper Array
	pWrkrData->aProtonMsgs[iMsg] = fmsgEntry;
finalize_it:
	RETiRet;
}

BEGINcreateInstance
CODESTARTcreateInstance
	DBGPRINTF("createInstance[%p]: ENTER\n", pData);
	pData->amqp_address = NULL;
	pData->azurehost = NULL;
	pData->azureport = NULL;
	pData->azure_key_name = NULL;
	pData->azure_key = NULL;
	pData->container = NULL;
ENDcreateInstance


BEGINcreateWrkrInstance
CODESTARTcreateWrkrInstance
	DBGPRINTF("createWrkrInstance[%p]: ENTER\n", pWrkrData);
	pWrkrData->bIsConnecting = 0;
	pWrkrData->bIsConnected = 0;
	pWrkrData->bIsSuspended = 0;

	// Create Proton proActor in Worker Instance
	pWrkrData->pnProactor = pn_proactor();
	pWrkrData->pnConn = NULL;
	pWrkrData->pnTransport = NULL;
	pWrkrData->pnSender = NULL;

	pWrkrData->iMsgSeq = 0;
	pWrkrData->iMaxMsgSeq = 0;
	pWrkrData->pnMessageBuffer.start = NULL;

	pWrkrData->nProtonMsgs = 0;
	pWrkrData->nMaxProtonMsgs = MAX_DEFAULTMSGS;
	CHKmalloc(pWrkrData->aProtonMsgs = calloc(MAX_DEFAULTMSGS, sizeof(struct s_protonmsg_entry)));

	CHKiRet(pthread_rwlock_init(&pWrkrData->pnLock, NULL));

	pWrkrData->bThreadRunning = 0;
	pWrkrData->tid = 0;

	// Run Proton Worker Thread now
	proton_run_thread(pWrkrData);
finalize_it:
	DBGPRINTF("createWrkrInstance[%p] returned %d\n", pWrkrData, iRet);
ENDcreateWrkrInstance


BEGINisCompatibleWithFeature
CODESTARTisCompatibleWithFeature
ENDisCompatibleWithFeature


BEGINfreeInstance
CODESTARTfreeInstance
	DBGPRINTF("freeInstance[%p]: ENTER\n", pData);

	if (pData->stats) {
		statsobj.Destruct(&pData->stats);
	}

	/* Free other mem */
	free(pData->amqp_address);
	free(pData->azurehost);
	free(pData->azureport);
	free(pData->azure_key_name);
	free(pData->azure_key);
	free(pData->container);

	free(pData->tplName);
	free(pData->statsName);
	for(int i = 0 ; i < pData->nEventProperties ; ++i) {
		free((void*) pData->eventProperties[i].key);
		free((void*) pData->eventProperties[i].val);
	}
	free(pData->eventProperties);
ENDfreeInstance

BEGINfreeWrkrInstance
CODESTARTfreeWrkrInstance
	DBGPRINTF("freeWrkrInstance[%p]: ENTER\n", pWrkrData);

	/* Closing azure first! */
	pthread_rwlock_wrlock(&pWrkrData->pnLock);

	// Close Proton Connection
	closeProton(pWrkrData);

	// Stop Proton Handle Thread
	proton_shutdown_thread(pWrkrData);

	// Free Proton Ressources
	if (pWrkrData->pnProactor != NULL) {
		DBGPRINTF("freeWrkrInstance[%p]:  FREE proactor\n", pWrkrData);
		pn_proactor_free(pWrkrData->pnProactor);
		pWrkrData->pnProactor = NULL;
	}
	free(pWrkrData->pnMessageBuffer.start);

	pthread_rwlock_unlock(&pWrkrData->pnLock);

	// Free our proton helper array
	if(pWrkrData->aProtonMsgs != NULL) {
		for(unsigned int i = 0 ; i < pWrkrData->nProtonMsgs ; ++i) {
			if (pWrkrData->aProtonMsgs[i] != NULL) {
				protonmsg_entry_destruct(pWrkrData->aProtonMsgs[i]);
			}
		}
		free(pWrkrData->aProtonMsgs);
	}

	pthread_rwlock_destroy(&pWrkrData->pnLock);
ENDfreeWrkrInstance


BEGINdbgPrintInstInfo
CODESTARTdbgPrintInstInfo
ENDdbgPrintInstInfo

BEGINtryResume
CODESTARTtryResume
#ifndef NDEBUG
	DBGPRINTF("omazureeventhubs[%p]: tryResume ENTER\n", pWrkrData);
#endif
	if (pWrkrData->bIsConnecting == 0 && pWrkrData->bIsConnected == 0) {
		DBGPRINTF("omazureeventhubs[%p]: tryResume setupProtonHandle\n", pWrkrData);
		CHKiRet(setupProtonHandle(pWrkrData, SETUP_PROTON_AUTOCLOSE));
	}
finalize_it:
	DBGPRINTF("omazureeventhubs[%p]: tryResume returned %d\n", pWrkrData, iRet);
ENDtryResume

BEGINbeginTransaction
CODESTARTbeginTransaction
	/* we have nothing to do to begin a transaction */
	DBGPRINTF("omazureeventhubs[%p]: beginTransaction ENTER\n", pWrkrData);
	if (pWrkrData->bIsConnecting == 0 && pWrkrData->bIsConnected == 0) {
		CHKiRet(setupProtonHandle(pWrkrData, SETUP_PROTON_NONE));
	}
finalize_it:
	RETiRet;
ENDbeginTransaction

/*
 *	New Transaction action interface
*/
BEGINcommitTransaction
	// instanceData *__restrict__ const pData = pWrkrData->pData;
	unsigned i;
	unsigned iNeedSubmission;
	sbool bDone = 0;
	protonmsg_entry* pMsgEntry = NULL;
CODESTARTcommitTransaction
#ifndef NDEBUG
	DBGPRINTF("omazureeventhubs[%p]: commitTransaction [%d msgs] ENTER\n", pWrkrData, nParams);
#endif

	// Handle/Expand our proton helper array
	if (nParams > pWrkrData->nMaxProtonMsgs) {
		// Free old Array
		if(pWrkrData->aProtonMsgs != NULL) {
			free(pWrkrData->aProtonMsgs);
		}
		// Expand our proton helper array
		DBGPRINTF("omazureeventhubs[%p]: commitTransaction expand helper array from %d to %d\n",
			pWrkrData, pWrkrData->nMaxProtonMsgs, nParams);
		pWrkrData->nMaxProtonMsgs = nParams; // Set new MAX
		CHKmalloc(pWrkrData->aProtonMsgs = calloc(pWrkrData->nMaxProtonMsgs, sizeof(struct s_protonmsg_entry)));
	}
	// Copy count of New Messages and increase MaxMsgSeq
	pWrkrData->nProtonMsgs = nParams;
	pWrkrData->iMaxMsgSeq += nParams;

	do {
		iNeedSubmission = 0;
		// Put unsubmitted messages into Proton
		for(i = 0 ; i < nParams ; ++i) {
			// Get reference to Proton Array Helper
			pMsgEntry = ((protonmsg_entry*)pWrkrData->aProtonMsgs[i]);
			if (	pMsgEntry == NULL) {
				// Send Message to Proton
				writeProton(pWrkrData, pParams, i);
			} else if (	pMsgEntry->status == PROTON_REJECTED) {
				// Reset Message Entry, it will be retried
				pMsgEntry->status = PROTON_UNSUBMITTED;
			}
		}
		bDone = 1;

		// Wait 100 milliseconds
		srSleep(0, 100000);

		// Verify if messages have been submitted successfully
		for(i = 0 ; i < nParams ; ++i) {
			// Get reference to Proton Array Helper
			pMsgEntry = ((protonmsg_entry*)pWrkrData->aProtonMsgs[i]);
			if (pMsgEntry != NULL) {
				if (		pMsgEntry->status == PROTON_UNSUBMITTED) {
					iNeedSubmission++;
					// we need to retry check later
					bDone = 0;
				} else if (	pMsgEntry->status == PROTON_SUBMITTED) {
					// we need to retry check later
					bDone = 0;
				}
			}
		}

		if (iNeedSubmission > 0) {
			if (	pWrkrData->bIsConnected == 1) {
				int credits = pn_link_credit(pWrkrData->pnSender);
				if (pn_link_credit(pWrkrData->pnSender) > 0) {
					DBGPRINTF("omazureeventhubs[%p]: trigger pn_connection_wake\n",
						pWrkrData);
					pn_connection_wake(pWrkrData->pnConn);
				} else {
					DBGPRINTF("omazureeventhubs[%p]: warning pn_link_credit returned %d\n",
						pWrkrData, credits);
				}
			} else {
				DBGPRINTF("omazureeventhubs[%p]: commitTransaction Suspended=%s Connecting=%s\n",
					pWrkrData,
					pWrkrData->bIsSuspended == 1 ? "YES" : "NO",
					pWrkrData->bIsConnecting == 1 ? "YES" : "NO");
				if (pWrkrData->bIsSuspended == 1 && pWrkrData->bIsConnecting == 0) {
					ABORT_FINALIZE(RS_RET_SUSPENDED);
				}
			}
		}
	} while (bDone == 0);
finalize_it:
	// Free Proton Message Helpers
	if (pWrkrData->aProtonMsgs != NULL) {
		for(i = 0 ; i < nParams ; ++i) {
			if (pWrkrData->aProtonMsgs[i] != NULL) {
				// Destroy
				protonmsg_entry_destruct(pWrkrData->aProtonMsgs[i]);
				pWrkrData->aProtonMsgs[i] = NULL;
			}
		}
	}

	/* TODO: Suspend Action if broker problems were reported in error callback */
	if (pWrkrData->bIsSuspended == 1) {
		DBGPRINTF("omazureeventhubs[%p]: commitTransaction failed to send messages, suspending action\n",
			pWrkrData);
		iRet = RS_RET_SUSPENDED;
	}
	if(iRet != RS_RET_OK) {
		DBGPRINTF("omazureeventhubs[%p]: commitTransaction failed with status %d\n", pWrkrData, iRet);
	}
	DBGPRINTF("omazureeventhubs[%p]: commitTransaction [%d msgs] EXIT\n", pWrkrData, nParams);
ENDcommitTransaction

static void
setInstParamDefaults(instanceData *pData) {
	DBGPRINTF("setInstParamDefaults[%p]: ENTER\n", pData);
	pData->amqp_address = NULL;
	pData->azurehost = NULL;
	pData->azureport = NULL;
	pData->azure_key_name = NULL;
	pData->azure_key = NULL;
	pData->container = NULL;
	pData->nEventProperties = 0;
	pData->eventProperties = NULL;
}

static rsRetVal
processEventProperty(char *const param,
	const char **const name,
	const char **const paramval) {
	DEFiRet;
	char *val = strstr(param, "=");
	if(val == NULL) {
		LogError(0, RS_RET_PARAM_ERROR, "missing equal sign in "
				"parameter '%s'", param);
		ABORT_FINALIZE(RS_RET_PARAM_ERROR);
	}
	*val = '\0'; /* terminates name */
	++val; /* now points to begin of value */
	CHKmalloc(*name = strdup(param));
	CHKmalloc(*paramval = strdup(val));
finalize_it:
	RETiRet;
}

BEGINnewActInst
	struct cnfparamvals *pvals;
	int i;
	int iNumTpls;
	DBGPRINTF("newActInst: ENTER\n");
CODESTARTnewActInst
	if((pvals = nvlstGetParams(lst, &actpblk, NULL)) == NULL) {
		ABORT_FINALIZE(RS_RET_MISSING_CNFPARAMS);
	}

	CHKiRet(createInstance(&pData));
	setInstParamDefaults(pData);

	for(i = 0 ; i < actpblk.nParams ; ++i) {
		if(!pvals[i].bUsed)
			continue;
		if(!strcmp(actpblk.descr[i].name, "amqp_address")) {
			pData->amqp_address = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(actpblk.descr[i].name, "azurehost")) {
			pData->azurehost = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(actpblk.descr[i].name, "azureport")) {
			pData->azureport = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(actpblk.descr[i].name, "azure_key_name")) {
			pData->azure_key_name = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(actpblk.descr[i].name, "azure_key")) {
			pData->azure_key = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(actpblk.descr[i].name, "container")) {
			pData->container = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(actpblk.descr[i].name, "eventproperties")) {
			pData->nEventProperties = pvals[i].val.d.ar->nmemb;
			CHKmalloc(pData->eventProperties = malloc(sizeof(struct event_property) *
			                                      pvals[i].val.d.ar->nmemb ));
			for(int j = 0 ; j <  pvals[i].val.d.ar->nmemb ; ++j) {
				char *cstr = es_str2cstr(pvals[i].val.d.ar->arr[j], NULL);
				CHKiRet(processEventProperty(cstr, &pData->eventProperties[j].key,
					&pData->eventProperties[j].val));
				free(cstr);
			}
		} else if(!strcmp(actpblk.descr[i].name, "template")) {
			pData->tplName = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(actpblk.descr[i].name, "statsname")) {
			pData->statsName = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else {
			LogError(0, RS_RET_INTERNAL_ERROR,
				"omazureeventhubs: program error, non-handled param '%s'\n", actpblk.descr[i].name);
		}
	}

	if(pData->amqp_address == NULL) {
		if(pData->azurehost == NULL) {
			LogMsg(0, NO_ERRCODE, LOG_INFO, "omazureeventhubs: \"azurehost\" parameter not specified "
				"(youreventhubinstance.servicebus.windows.net- action definition invalid!");
			ABORT_FINALIZE(RS_RET_CONFIG_ERROR);
		}
		if(pData->azureport== NULL) {
			// Set default
			CHKmalloc(pData->azureport = (uchar *) strdup("5671"));
		}
		if(pData->azure_key_name == NULL || pData->azure_key == NULL) {
			LogError(0, RS_RET_CONFIG_ERROR,
				"omazureeventhubs: azure_key_name and azure_key are requires to access azure eventhubs"
				" - action definition invalid");
			ABORT_FINALIZE(RS_RET_CONFIG_ERROR);
		}
		if(pData->container == NULL) {
			LogError(0, RS_RET_CONFIG_ERROR,
				"omazureeventhubs: Event Hubs \"container\" parameter (which is instance) not specified"
				" - action definition invalid");
			ABORT_FINALIZE(RS_RET_CONFIG_ERROR);
		}

		// URL encode the key name and key
		char *encoded_key_name = url_encode((char*)pData->azure_key_name);
		char *encoded_key = url_encode((char*)pData->azure_key);
		if(encoded_key_name == NULL || encoded_key == NULL) {
			free(encoded_key_name);
			free(encoded_key);
			LogError(0, RS_RET_ERR, "omazureeventhubs: failed to encode credentials");
			ABORT_FINALIZE(RS_RET_ERR);
		}

		// Create amqps URL from parameters
		char szAddress[1024];
		snprintf(szAddress, sizeof(szAddress), "amqps://%s:%s@%s:%s/%s",
			encoded_key_name,
			encoded_key,
			pData->azurehost,
			pData->azureport,
			pData->container);

		// Free encoded strings
		free(encoded_key_name);
		free(encoded_key);

		CHKmalloc(pData->amqp_address = (uchar*) strdup(szAddress));
	} else {
		// Free if set first
		pData->azurehost = NULL;
		pData->azureport = NULL;
		pData->azure_key_name = NULL;
		pData->azure_key = NULL;
		pData->container = NULL;

		// Remove the protocol part
		char* amqp_address_dup = strdup((char*)pData->amqp_address);
		char* startstr 	= strstr(amqp_address_dup, "amqps://");
		char* endstr	= NULL;
		if (!startstr) {
			LogError(0, RS_RET_CONFIG_ERROR,
				"omazureeventhubs: \"amqp_address\" parameter (URL) invalid "
				" - could not find prefix in URL");
			free(amqp_address_dup);
			ABORT_FINALIZE(RS_RET_CONFIG_ERROR);
		}
		startstr += strlen("amqps://");

		// Parse AccessKeyName
		endstr = strchr(startstr, ':');
		if (!endstr) {
			LogError(0, RS_RET_CONFIG_ERROR,
				"omazureeventhubs: \"amqp_address\" parameter (URL) invalid "
				" - could not find azure_key_name in URL");
			free(amqp_address_dup);
			ABORT_FINALIZE(RS_RET_CONFIG_ERROR);
		}
		*endstr = '\0';
		pData->azure_key_name = (uchar*) strdup(startstr);

		// Parse AccessKey
		startstr = endstr + 1;
		endstr = strchr(startstr, '@');
		if (!endstr) {
			LogError(0, RS_RET_CONFIG_ERROR,
				"omazureeventhubs: \"amqp_address\" parameter (URL) invalid "
				" - could not find azure_key in URL");
			free(amqp_address_dup);
			ABORT_FINALIZE(RS_RET_CONFIG_ERROR);
		}
		*endstr = '\0';
		pData->azure_key = (uchar*) strdup(startstr);

		// Parse EventHubsNamespace and EventHubsInstance
		startstr = endstr + 1;
		endstr = strchr(startstr, '/');
		if (!endstr) {
			LogError(0, RS_RET_CONFIG_ERROR,
				"omazureeventhubs: \"amqp_address\" parameter (URL) invalid "
				" - could not find azurehost in URL");
			free(amqp_address_dup);
			ABORT_FINALIZE(RS_RET_CONFIG_ERROR);
		}
		*endstr = '\0';
		pData->azurehost = (uchar*) strdup(startstr);

		// Set port to default 5671 (amqps)
		pData->azureport = (uchar*) strdup("5671");

		// Copy Rest into container (EventHubsInstance)
		startstr = endstr + 1;
		pData->container = (uchar*) strdup(startstr);

		// Free dup memory
		free(amqp_address_dup);

		// Output Debug Information
		DBGPRINTF(
		"newActInst: parsed amqp_address parameters for %p@key_name=%s key=%s host=%s port=%s container=%s\n",
			pData,
			pData->azure_key_name,
			pData->azure_key,
			pData->azurehost,
			pData->azureport,
			pData->container
			);
	}

	iNumTpls = 1;
	CODE_STD_STRING_REQUESTnewActInst(iNumTpls);

	CHKiRet(OMSRsetEntry(*ppOMSR, 0, (uchar*)strdup((pData->tplName == NULL) ?
						"RSYSLOG_FileFormat" : (char*)pData->tplName),
						OMSR_NO_RQD_TPL_OPTS));

	if (pData->statsName) {
		CHKiRet(statsobj.Construct(&pData->stats));
		CHKiRet(statsobj.SetName(pData->stats, (uchar *)pData->statsName));
		CHKiRet(statsobj.SetOrigin(pData->stats, (uchar *)"omazureeventhubs"));

		/* Track following stats */
		STATSCOUNTER_INIT(pData->ctrMessageSubmit, pData->mutCtrMessageSubmit);
		CHKiRet(statsobj.AddCounter(pData->stats, (uchar *)"submitted",
			ctrType_IntCtr, CTR_FLAG_RESETTABLE, &pData->ctrMessageSubmit));
		STATSCOUNTER_INIT(pData->ctrAzureFail, pData->mutCtrAzureFail);
		CHKiRet(statsobj.AddCounter(pData->stats, (uchar *)"failures",
			ctrType_IntCtr, CTR_FLAG_RESETTABLE, &pData->ctrAzureFail));
		STATSCOUNTER_INIT(pData->ctrAzureAck, pData->mutCtrAzureAck);
		CHKiRet(statsobj.AddCounter(pData->stats, (uchar *)"accepted",
			ctrType_IntCtr, CTR_FLAG_RESETTABLE, &pData->ctrAzureAck));
		STATSCOUNTER_INIT(pData->ctrAzureOtherErrors, pData->mutCtrAzureOtherErrors);
		CHKiRet(statsobj.AddCounter(pData->stats, (uchar *)"othererrors",
			ctrType_IntCtr, CTR_FLAG_RESETTABLE, &pData->ctrAzureOtherErrors));
		CHKiRet(statsobj.ConstructFinalize(pData->stats));
	}

CODE_STD_FINALIZERnewActInst
	cnfparamvalsDestruct(pvals, &actpblk);
ENDnewActInst


BEGINmodExit
CODESTARTmodExit
	DBGPRINTF("modExit: ENTER\n");
	statsobj.Destruct(&azureStats);
	CHKiRet(objRelease(statsobj, CORE_COMPONENT));
	DESTROY_ATOMIC_HELPER_MUT(mutClock);

	objRelease(glbl, CORE_COMPONENT);
finalize_it:
ENDmodExit

NO_LEGACY_CONF_parseSelectorAct
BEGINqueryEtryPt
CODESTARTqueryEtryPt
CODEqueryEtryPt_STD_OMODTX_QUERIES
CODEqueryEtryPt_STD_OMOD8_QUERIES
CODEqueryEtryPt_STD_CONF2_CNFNAME_QUERIES
CODEqueryEtryPt_STD_CONF2_OMOD_QUERIES
ENDqueryEtryPt


BEGINmodInit()
CODESTARTmodInit
//	uchar *pTmp;
	DBGPRINTF("modInit: ENTER\n");
INITLegCnfVars
	*ipIFVersProvided = CURR_MOD_IF_VERSION;
CODEmodInit_QueryRegCFSLineHdlr
	/* request objects we use */
	CHKiRet(objUse(glbl, CORE_COMPONENT));
	CHKiRet(objUse(datetime, CORE_COMPONENT));
	CHKiRet(objUse(strm, CORE_COMPONENT));
	CHKiRet(objUse(statsobj, CORE_COMPONENT));

//	INIT_ATOMIC_HELPER_MUT(mutClock);
	DBGPRINTF("omazureeventhubs %s using qpid-proton library %d.%d.%d\n",
	          VERSION, PN_VERSION_MAJOR, PN_VERSION_MINOR, PN_VERSION_POINT);

	CHKiRet(statsobj.Construct(&azureStats));
	CHKiRet(statsobj.SetName(azureStats, (uchar *)"omazureeventhubs"));
	CHKiRet(statsobj.SetOrigin(azureStats, (uchar*)"omazureeventhubs"));
	STATSCOUNTER_INIT(ctrMessageSubmit, mutCtrMessageSubmit);
	CHKiRet(statsobj.AddCounter(azureStats, (uchar *)"submitted",
		ctrType_IntCtr, CTR_FLAG_RESETTABLE, &ctrMessageSubmit));
	STATSCOUNTER_INIT(ctrAzureFail, mutCtrAzureFail);
	CHKiRet(statsobj.AddCounter(azureStats, (uchar *)"failures",
		ctrType_IntCtr, CTR_FLAG_RESETTABLE, &ctrAzureFail));
	STATSCOUNTER_INIT(ctrAzureAck, mutCtrAzureAck);
	CHKiRet(statsobj.AddCounter(azureStats, (uchar *)"accepted",
		ctrType_IntCtr, CTR_FLAG_RESETTABLE, &ctrAzureAck));
	STATSCOUNTER_INIT(ctrAzureOtherErrors, mutCtrAzureOtherErrors);
	CHKiRet(statsobj.AddCounter(azureStats, (uchar *)"failures_other",
		ctrType_IntCtr, CTR_FLAG_RESETTABLE, &ctrAzureOtherErrors));
	CHKiRet(statsobj.ConstructFinalize(azureStats));
ENDmodInit

pn_timestamp_t time_now(void)
{
	struct timeval now;
	if (gettimeofday(&now, NULL)) return 0;
	return ((pn_timestamp_t)now.tv_sec) * 1000 + (now.tv_usec / 1000);
}

/*
*	Start PROTON Handling Thread
*/
static rsRetVal proton_run_thread(wrkrInstanceData_t *pWrkrData)
{
	DEFiRet;
	int iErr = 0;
	if (	!pWrkrData->bThreadRunning) {
		DBGPRINTF("omazureeventhubs[%p]: proton_run_thread\n", pWrkrData);

		do {
			iErr = pthread_create(&pWrkrData->tid,
					NULL,
					proton_thread,
					pWrkrData);
			if (!iErr) {
				pWrkrData->bThreadRunning = 1;
				DBGPRINTF("omazureeventhubs[%p]: proton_run_thread (tid %lx) created\n",
					pWrkrData, pWrkrData->tid);
				FINALIZE;
			}
		} while (iErr == EAGAIN);
	} else {
		DBGPRINTF("omazureeventhubs[%p]: proton_run_thread (tid %ld) already running\n",
			pWrkrData, pWrkrData->tid);
	}
finalize_it:
	if (iRet != RS_RET_OK) {
		LogError(0, RS_RET_SYS_ERR, "omazureeventhubs: proton_run_thread thread create failed with error: %d",
			iErr);
	}
	RETiRet;
}
/*	Stop PROTON Handling Thread
*/
static rsRetVal
proton_shutdown_thread(wrkrInstanceData_t *pWrkrData)
{
	DEFiRet;
	if (	pWrkrData->bThreadRunning) {
		DBGPRINTF("omazureeventhubs[%p]: STOPPING Thread\n", pWrkrData);
		int r = pthread_cancel(pWrkrData->tid);
		if(r == 0) {
			pthread_join(pWrkrData->tid, NULL);
		}
		DBGPRINTF("omazureeventhubs[%p]: STOPPED Thread\n", pWrkrData);
		pWrkrData->bThreadRunning = 0;
	}
	FINALIZE;
finalize_it:
	RETiRet;
}

/*
*	Workerthread function for a single ProActor Handler
 */
static void *
proton_thread(void __attribute__((unused)) *pVoidWrkrData)
{
	wrkrInstanceData_t *const pWrkrData = (wrkrInstanceData_t *const) pVoidWrkrData;
	instanceData *const pData = (instanceData *const) pWrkrData->pData;

	DBGPRINTF("proton_thread[%p]: started protocol workerthread(%lx) for %s:%s/%s\n",
		pWrkrData, pthread_self(), pData->azurehost, pData->azureport, pData->container);

	do {
		if (	pWrkrData->pnProactor != NULL) {
			// Process Protocol events
			pn_event_batch_t *events = pn_proactor_wait(pWrkrData->pnProactor);
			pn_event_t *event;
			while ((event = pn_event_batch_next(events))) {
				handleProton(pWrkrData, event);
			}
			pn_proactor_done(pWrkrData->pnProactor, events);
		} else {
			// Wait 10 microseconds
			srSleep(0, 10000);
		}
	} while(glbl.GetGlobalInputTermState() == 0);
	
	DBGPRINTF("proton_thread[%p]: stopped protocol workerthread\n", pWrkrData);
	return NULL;
}

static void
handleProtonDelivery(wrkrInstanceData_t *const pWrkrData) {
	instanceData *const pData = (instanceData *const) pWrkrData->pData;

	/* Process messages from ARRAY */
	for(unsigned int i = 0 ; i < pWrkrData->nProtonMsgs ; ++i) {
		protonmsg_entry* pMsgEntry = (protonmsg_entry*) pWrkrData->aProtonMsgs[i];
		// Process Unsubmitted messages only
		if (pMsgEntry != NULL) {
			if (pMsgEntry->status == PROTON_UNSUBMITTED) {
				int iCreditBalance = pn_link_credit(pWrkrData->pnSender);
				if (iCreditBalance > 0) {
					DBGPRINTF(
					"handleProtonDelivery: PN_LINK_FLOW deliver '%s' @ %p:%s:%s/%s, msg:'%.*s'\n",
						pMsgEntry->MsgID,
						pWrkrData,
						pData->azurehost,
						pData->azureport,
						pData->container,
						(pMsgEntry->payload_len > 0 ?
							(int)pMsgEntry->payload_len-1 : 0),
						pMsgEntry->payload);

					/* Use sent counter as unique delivery tag. */
					pn_delivery(pWrkrData->pnSender, pn_dtag((const char *)pMsgEntry->MsgID,
						pMsgEntry->MsgID_len));

					/* Construct a message with the map */
					pn_message_t* message = proton_encode_message(pWrkrData, pMsgEntry);
					/*
					* pn_message_send does performs the following steps:
					* - call pn_message_encode2() to encode the message to a buffer
					* - call pn_link_send() to send the encoded message bytes
					* - call pn_link_advance() to indicate the message is complete
					*/
					if (pn_message_send(	message,
								pWrkrData->pnSender,
								&pWrkrData->pnMessageBuffer) < 0) {
						LogMsg(0, NO_ERRCODE, LOG_INFO,
							"handleProtonDelivery: PN_LINK_FLOW deliver SEND ERROR %s\n",
							pn_error_text(pn_message_error(message)));
						pn_message_free(message);
						break;
					} else {
						DBGPRINTF("handleProtonDelivery: PN_LINK_FLOW deliver SUCCESS\n");
						pn_message_free(message);
					}

					STATSCOUNTER_INC(ctrMessageSubmit, mutCtrMessageSubmit);
					INST_STATSCOUNTER_INC(	pData,
								pData->ctrMessageSubmit,
								pData->mutCtrMessageSubmit);
					pMsgEntry->status = PROTON_SUBMITTED;
				} else {
					DBGPRINTF("handleProtonDelivery: sender credit balance reached %d. "
						"extend credit for %d\n", iCreditBalance, pWrkrData->nProtonMsgs);
					pn_link_flow(pWrkrData->pnSender, pWrkrData->nProtonMsgs);

					// TODO: MAKE CONFIGUREABLE
					// Wait 10 microseconds
					// srSleep(0, 10000);
					break;
				}
			}
		} else {
			// Abort further processing if pMsgEntry is NULL
			break;
		}
	}
}

/*	Handles PROTON Communication in this Function
*
*/
#pragma GCC diagnostic ignored "-Wswitch"
static void
handleProton(wrkrInstanceData_t *const pWrkrData, pn_event_t *event) {
	instanceData *const pData = (instanceData *const) pWrkrData->pData;
	/* Handle Proton Events */
	switch (pn_event_type(event)) {
	case PN_CONNECTION_BOUND: {
		DBGPRINTF("handleProton: PN_CONNECTION_BOUND to %p:%s:%s/%s\n",
			pWrkrData, pData->azurehost, pData->azureport, pData->container);
		break;
	}
	case PN_SESSION_INIT : {
		DBGPRINTF("handleProton: PN_SESSION_INIT  to %p:%s:%s/%s\n",
			pWrkrData, pData->azurehost, pData->azureport, pData->container);
		break;
	}
	case PN_LINK_INIT: {
		DBGPRINTF("handleProton: PN_LINK_INIT to %p:%s:%s/%s\n",
			pWrkrData, pData->azurehost, pData->azureport, pData->container);
		break;
	}
	case PN_LINK_REMOTE_OPEN: {
		DBGPRINTF("handleProton: PN_LINK_REMOTE_OPEN to %p:%s:%s/%s\n",
			pWrkrData, pData->azurehost, pData->azureport, pData->container);
		pWrkrData->bIsConnected = 1;
		pWrkrData->bIsConnecting = 0;
		break;
	}
	case PN_CONNECTION_INIT: {
		DBGPRINTF("handleProton: PN_CONNECTION_INIT to %p:%s:%s/%s\n",
			pWrkrData, pData->azurehost, pData->azureport, pData->container);
		pWrkrData->pnStatus = PN_CONNECTION_INIT;

		// Get Connection
		pWrkrData->pnConn = pn_event_connection(event);
		// Set AMQP Properties
		pn_connection_set_container(pWrkrData->pnConn, (const char *) pData->container);
		pn_connection_set_hostname(pWrkrData->pnConn, (const char *) pData->azurehost);
		pn_connection_set_user(pWrkrData->pnConn, (const char *) pData->azure_key_name);
		pn_connection_set_password(pWrkrData->pnConn, (const char *) pData->azure_key);

		pn_connection_open(pWrkrData->pnConn);					// Open Connection
		pn_session_t* pnSession = pn_session(pWrkrData->pnConn);			// Create Session
		pn_session_open(pnSession);						// Open Session
		pWrkrData->pnSender = pn_sender(pnSession, (char *)pData->azure_key_name);	// Create Link

		DBGPRINTF("handleProton: PN_CONNECTION_INIT with amqp address: %s\n",
			pData->amqp_address);
		pn_terminus_set_address(pn_link_target(pWrkrData->pnSender),
			(const char *) pData->amqp_address);

		pn_link_open(pWrkrData->pnSender);
		pn_link_flow(pWrkrData->pnSender, pWrkrData->nProtonMsgs);
		break;
	}
	case PN_CONNECTION_REMOTE_OPEN: {
		DBGPRINTF("handleProton: PN_CONNECTION_REMOTE_OPEN to %p:%s:%s/%s\n",
			pWrkrData, pData->azurehost, pData->azureport, pData->container);
		pWrkrData->pnStatus = PN_CONNECTION_REMOTE_OPEN;
		pn_ssl_t *ssl = pn_ssl(pn_event_transport(event));
		if (ssl) {
			char name[1024];
			pn_ssl_get_protocol_name(ssl, name, sizeof(name));
			{
				const char *subject = pn_ssl_get_remote_subject(ssl);
				if (subject) {
					DBGPRINTF(
					"handleProton: handleProton secure connection: to %s using %s\n",
						subject, name);
				} else {
					DBGPRINTF(
					"handleProton: handleProton anonymous connection: using %s\n",
						name);
				}
				fflush(stdout);
			}
		}
		break;
	}
	case PN_CONNECTION_WAKE: {
		DBGPRINTF("handleProton: PN_CONNECTION_WAKE (%d) to %p:%s:%s/%s\n",
			pWrkrData->nProtonMsgs,
			pWrkrData, pData->azurehost, pData->azureport, pData->container);
		/* Process messages */
		handleProtonDelivery(pWrkrData);
		break;
	}
	case PN_LINK_FLOW: {
		pWrkrData->pnStatus = PN_LINK_FLOW;
		/* The peer has given us some credit, now we can send messages */
		pWrkrData->pnSender = pn_event_link(event);

		/* Process messages */
		handleProtonDelivery(pWrkrData);
		break;
	}
	case PN_DELIVERY: {
		pWrkrData->pnStatus = PN_DELIVERY;
		/* We received acknowledgement from the peer that a message was delivered. */
		pn_delivery_t* pDeliveryStatus = pn_event_delivery(event);
		pn_delivery_tag_t pnTag = pn_delivery_tag(pDeliveryStatus);

		if (pnTag.start != NULL) {
			// Convert Tag into Number!
			unsigned int iTagNum = (unsigned int) atoi((pnTag.start != NULL ? pnTag.start : ""));
			// Calc QueueNumber from Tagnum
			unsigned int iQueueNum = pWrkrData->nProtonMsgs - (pWrkrData->iMaxMsgSeq - iTagNum);
			// Get proton Msg Helper (checks for out of bound array access)
			protonmsg_entry* pMsgEntry = NULL;
			if (pWrkrData->nMaxProtonMsgs > iQueueNum) {
				pMsgEntry = (protonmsg_entry*) pWrkrData->aProtonMsgs[iQueueNum];
			}
			// Process if found
			if (pMsgEntry != NULL) {
				if (pn_delivery_remote_state(pDeliveryStatus) == PN_ACCEPTED) {
					DBGPRINTF(
					"handleProton: PN_DELIVERY SUCCESS for MSG '%s(Q %d, MAX %d)' @ %p:%s:%s/%s\n",
						(pnTag.start != NULL ? (char*) pnTag.start : "NULL"),
						iQueueNum,
						pWrkrData->nMaxProtonMsgs,
						pWrkrData, pData->azurehost, pData->azureport, pData->container);
					pn_delivery_settle(pDeliveryStatus); // free the delivered message
					pMsgEntry->status = PROTON_ACCEPTED;

					// Increment Stats Counter
					STATSCOUNTER_INC(ctrAzureAck, mutCtrAzureAck);
					INST_STATSCOUNTER_INC(pData, pData->ctrAzureAck, pData->mutCtrAzureAck);
				} else if (pn_delivery_remote_state(pDeliveryStatus) == PN_REJECTED) {
					LogError(0, RS_RET_ERR,
						"omazureeventhubs: PN_DELIVERY REJECTED for MSG '%s'"
						" - @ %p:%s:%s/%s\n",
						(pnTag.start != NULL ? (char*) pnTag.start : "NULL"),
						pWrkrData, pData->azurehost, pData->azureport, pData->container);
					pMsgEntry->status = PROTON_REJECTED;

					// Increment Stats Counter
					STATSCOUNTER_INC(ctrAzureFail, mutCtrAzureFail);
					INST_STATSCOUNTER_INC(pData, pData->ctrAzureFail, pData->mutCtrAzureFail);
				}
			} else {
				DBGPRINTF("handleProton: PN_DELIVERY MISSING MSG '%d(Q %d,MAX %d)' - @ %p:%s:%s/%s\n",
					iTagNum,
					iQueueNum,
					pWrkrData->nMaxProtonMsgs,
					pWrkrData, pData->azurehost, pData->azureport, pData->container);
				STATSCOUNTER_INC(ctrAzureOtherErrors, mutCtrAzureOtherErrors);
				INST_STATSCOUNTER_INC(pData, pData->ctrAzureOtherErrors, pData->mutCtrAzureOtherErrors);
			}
		} else {
			LogError(0, RS_RET_ERR,"handleProton: PN_DELIVERY HELPER ARRAY is NULL - @ %p:%s:%s/%s\n",
					pWrkrData, pData->azurehost, pData->azureport, pData->container);
			STATSCOUNTER_INC(ctrAzureOtherErrors, mutCtrAzureOtherErrors);
			INST_STATSCOUNTER_INC(pData, pData->ctrAzureOtherErrors, pData->mutCtrAzureOtherErrors);
		}
		break;
	}
	case PN_TRANSPORT_CLOSED:
		DBGPRINTF("handleProton: transport closed for %p:%s\n", pWrkrData, pData->azurehost);
		proton_check_condition(event, pWrkrData, pn_transport_condition(pn_event_transport(event)),
			"transport closed");
		break;
	case PN_CONNECTION_REMOTE_CLOSE:
		DBGPRINTF("handleProton: connection closed for %p:%s\n", pWrkrData, pData->azurehost);
		proton_check_condition(event, pWrkrData, pn_connection_remote_condition(pn_event_connection(event)),
			"connection closed");
		break;
	case PN_SESSION_REMOTE_CLOSE:
		DBGPRINTF("handleProton: remote session closed for %p:%s\n", pWrkrData, pData->azurehost);
		proton_check_condition(event, pWrkrData, pn_session_remote_condition(pn_event_session(event)),
			"remote session closed");
		break;
	case PN_LINK_REMOTE_CLOSE:
	case PN_LINK_REMOTE_DETACH:
		DBGPRINTF("handleProton: remote link closed for %p:%s\n", pWrkrData, pData->azurehost);
		proton_check_condition(event, pWrkrData, pn_link_remote_condition(pn_event_link(event)),
			"remote link closed");
		break;
	case PN_PROACTOR_INACTIVE:
		DBGPRINTF("handleProton: INAKTIVE for %p:%s\n", pWrkrData, pData->azurehost);
		break;
/* Workarround compiler warning:
*	error: enumeration value '...' not handled in switch [-Werror=switch-enum]
*/
#ifdef __GNU_C
	default:
		DBGPRINTF("handleProton: UNHANDELED EVENT %d for %p:%s\n", pn_event_type(event),
			pWrkrData, pData->azurehost);
		break;
#endif
	}
}
